// Copyright (c) Choreo contributors

#include <algorithm>
#include <format>
#include <functional>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include <wpi/util/json.hpp>

#include "api_server_internal.hpp"
#include "choreo/state_server/api_server.hpp"

namespace choreo::state_server {

namespace {

using choreo::rest_router::HttpMethod;
using choreo::rest_router::Request;
using choreo::rest_router::Response;
using choreo::rest_router::RouteParams;
using namespace choreo::state_server::detail;

}  // namespace

void ApiServer::RecordScopeMutation(std::string_view scope_key,
                                    std::string_view reason,
                                    const wpi::util::json& before) {
  const auto after = CaptureScopeSnapshot(scope_key);
  if (!after.has_value()) {
    return;
  }

  EnsureHistoryEngine(scope_key).Record(MakeHistoryEntryFromSnapshots(
      std::string(reason), before, *after, std::chrono::system_clock::now()));
}

wpi::util::json ApiServer::CaptureScopeBefore(std::string_view scope_key) const {
  return CaptureScopeSnapshot(scope_key).value_or(wpi::util::json(nullptr));
}

Response ApiServer::VariablePutResponse(const std::string& variable_uuid,
                                        const wpi::util::json& payload) const {
  auto response = JsonResponse(200, payload);
  response.headers["ETag"] = QuotedEtag(ProjectRevisionToken());
  return response;
}

bool ApiServer::HasDuplicateNameInMap(
    const std::unordered_map<std::string, TrajectoryFile>& items,
    std::string_view current_uuid, std::string_view candidate_name) const {
  for (const auto& [other_uuid, item] : items) {
    if (other_uuid != current_uuid && item.name == candidate_name) {
      return true;
    }
  }
  return false;
}

bool ApiServer::HasDuplicateVariableName(std::string_view current_uuid,
                                         std::string_view candidate_name) const {
  const auto has_name =
      [&](const auto& items) {
        for (const auto& [other_uuid, item] : items) {
          if (other_uuid == current_uuid) {
            continue;
          }
          if (item.name == candidate_name) {
            return true;
          }
        }
        return false;
      };

  return has_name(m_project.variables.expressions) ||
         has_name(m_project.variables.translations) ||
         has_name(m_project.variables.poses) ||
         has_name(m_project.variables.regions);
}

Response ApiServer::WithProjectPrecondition(const Request& request,
                                            ProjectPreconditionHandler on_success) {
  const auto project_scope = ProjectScopeKey();
  const auto current_revision = ProjectRevisionToken();
  if (auto error = ValidateIfMatchPrecondition(request, current_revision)) {
    return *error;
  }
  return on_success(project_scope);
}

Response ApiServer::WithProjectMutation(const Request& request,
                                        ProjectMutationHandler on_success) {
  return WithProjectPrecondition(request, [&](std::string_view project_scope) {
    const auto before = CaptureScopeBefore(project_scope);
    return on_success(project_scope, before);
  });
}

Response ApiServer::WithExistingTrajectory(const RouteParams& params,
                                           TrajectoryLookupHandler on_success) {
  const auto trajectory_uuid =
      RequireRouteParam(params, "uuid", "Missing trajectory UUID parameter");
  if (!trajectory_uuid.has_value()) {
    return trajectory_uuid.error();
  }

  auto trajectory_it = m_trajectories.find(trajectory_uuid->get());
  if (trajectory_it == m_trajectories.end()) {
    return NotFound("Trajectory not found");
  }

  return on_success(trajectory_uuid->get(), trajectory_it);
}

Response ApiServer::WithTrajectoryMutation(
    const Request& request, const RouteParams& params,
    TrajectoryMutationHandler on_success) {
  return WithExistingTrajectory(
      params, [&](const std::string& trajectory_uuid, TrajectoryIterator traj_it) {
        const auto before = CaptureScopeBefore(TrajectoryScopeKey(trajectory_uuid));
        const auto current_revision = TrajectoryRevisionToken(trajectory_uuid);
        if (auto error =
                ValidateIfMatchPrecondition(request, current_revision)) {
          return *error;
        }
        return on_success(trajectory_uuid, traj_it, before);
      });
}

Response ApiServer::DeletedTrajectoryResponse(const std::string& uuid) const {
  auto body = wpi::util::json::object();
  body["uuid"] = uuid;
  body["deleted"] = true;
  auto response = JsonResponse(200, body);
  response.headers["ETag"] = QuotedEtag(TrajectoryRevisionToken(uuid));
  return response;
}

void ApiServer::CommitTrajectoryMutation(const std::string& trajectory_uuid,
                                         std::string_view mutation_reason,
                                         const wpi::util::json& before) {
  ++m_trajectory_revisions[trajectory_uuid];
  RecordScopeMutation(TrajectoryScopeKey(trajectory_uuid), mutation_reason,
                      before);
}

std::expected<std::reference_wrapper<const std::string>, Response>
ApiServer::RequireSubresourceUuid(const RouteParams& params,
                                  std::string_view param_name,
                                  std::string_view missing_message) const {
  return RequireRouteParam(params, param_name, missing_message);
}

std::optional<Response> ApiServer::ParseOrderRequest(
    const wpi::util::json& body, std::vector<std::string>& out_order) const {
  std::string parse_error;
  if (!ParseOrderArray(body, out_order, parse_error)) {
    return InvalidJson(parse_error);
  }
  return std::nullopt;
}

void ApiServer::RegisterTrajectoryHistoryRoute(
    HttpMethod method, const char* route_path,
    std::function<std::optional<Response>(std::string_view)> apply_history,
    const char* missing_entry_message) {
  m_router.Register(
      method, route_path,
      [this, apply_history = std::move(apply_history),
       missing_entry_message](const Request& request, const RouteParams& params) {
        const auto trajectory_uuid =
            RequireRouteParam(params, "uuid", "Missing trajectory UUID parameter");
        if (!trajectory_uuid.has_value()) {
          return trajectory_uuid.error();
        }
        const auto& uuid = trajectory_uuid->get();

        const auto scope_key = TrajectoryScopeKey(uuid);
        const auto current_revision = TrajectoryRevisionToken(uuid);
        if (auto error = ValidateIfMatchPrecondition(request, current_revision)) {
          return *error;
        }

        if (auto history_error = apply_history(scope_key)) {
          return *history_error;
        }

        const auto trajectory = FindMappedValue(m_trajectories, uuid);
        if (!trajectory.has_value()) {
          return missing_entry_message == nullptr
                     ? DeletedTrajectoryResponse(uuid)
                     : NotFound(missing_entry_message);
        }

        return JsonModelResponseWithEtag(200, TrajectoryRevisionToken(uuid),
                                         trajectory->get());
      });
}

    /// @brief Register the trajectory listing route.
    ///
    /// Response body includes trajectory summaries and revision tokens for each
    /// trajectory in memory.
void ApiServer::RegisterTrajectoryListRoute() {
  m_router.Register(
      HttpMethod::kGet, "/api/v1/trajectories",
      [=, this](const Request&, const RouteParams&) {
        wpi::util::json body = wpi::util::json::object(
            "items", wpi::util::json::array(), "nextCursor", nullptr,
            "totalEstimate", static_cast<double>(m_trajectories.size()));
        for (const auto& [uuid, traj] : m_trajectories) {
          wpi::util::json summary = wpi::util::json::object();
          summary["uuid"] = traj.uuid;
          summary["name"] = traj.name;
          summary["version"] = traj.version;
          summary["upToDate"] = !traj.must_be_generated(m_project);
          summary["hasTrajectoryData"] = traj.trajectory.has_value();
          summary["updatedAt"] = "";
          summary["revision"] = TrajectoryRevisionToken(uuid);
          body["items"].emplace_back(std::move(summary));
        }
        return JsonResponse(200, body);
      });
}

/// @brief Register the trajectory creation route.
///
/// Validates UUID uniqueness, records history, and returns the created
/// trajectory with Location and ETag headers.
void ApiServer::RegisterTrajectoryCreateRoute() {
  m_router.Register(
      HttpMethod::kPost, "/api/v1/trajectories",
      [=, this](const Request& request, const RouteParams&) {
        return WithParsedJsonBody<wpi::util::json>(
            request, [&](const auto& body) -> Response {
              auto created = TrajectoryFile::fromJson(body);
              EnsureUuid(created.uuid);
              if (m_trajectories.contains(created.uuid)) {
                return ErrorResponse(ApiError::UuidConflict,
                                     "Trajectory UUID already exists");
              }

              const auto uuid = created.uuid;
              const auto before = CaptureScopeBefore(TrajectoryScopeKey(uuid));
              m_trajectories.emplace(uuid, std::move(created));
              CommitTrajectoryMutation(uuid, "create_trajectory", before);

              auto response = JsonModelResponseWithEtag(
                  201, TrajectoryRevisionToken(uuid), m_trajectories.at(uuid));
              response.headers["Location"] =
                  std::format("/api/v1/trajectories/{}", uuid);
              return response;
            });
      });
}

/// @brief Register the single-trajectory fetch route.
///
/// Resolves trajectory UUID from route params and returns the trajectory with
/// trajectory-scoped ETag.
void ApiServer::RegisterTrajectoryGetRoute() {
  m_router.Register(
      HttpMethod::kGet, "/api/v1/trajectories/{uuid}",
      [=, this](const Request&, const RouteParams& params) {
        return WithExistingTrajectory(
            params, [this](const std::string& trajectory_uuid_value,
                           auto trajectory_it) {
              return JsonModelResponseWithEtag(
                  200, TrajectoryRevisionToken(trajectory_uuid_value),
                  trajectory_it->second);
            });
      });
}

/// @brief Register waypoint delete route and related reference cleanup.
///
/// Ensures at least one waypoint remains and removes any constraint/event
/// references to the deleted waypoint before recording history.
void ApiServer::RegisterWaypointDeleteRoute() {
  m_router.Register(
      HttpMethod::kDelete,
      "/api/v1/trajectories/{uuid}/waypoints/{waypointUuid}",
      [=, this](const Request& request, const RouteParams& params) {
        const auto waypoint_uuid = RequireSubresourceUuid(params, "waypointUuid");
        if (!waypoint_uuid.has_value()) {
          return waypoint_uuid.error();
        }

        return WithTrajectoryMutation(
            request, params,
            [=, this](const std::string& trajectory_uuid, auto traj_it,
                      const wpi::util::json& before) {
              const auto waypoint_index = FindByUuid(
                  traj_it->second.params.waypoints, waypoint_uuid->get());
              if (!waypoint_index.has_value()) {
                return NotFound("Waypoint not found");
              }

              auto& waypoints = traj_it->second.params.waypoints;
              if (waypoints.size() <= 1) {
                return ErrorResponse(
                  ApiError::InvalidOperation,
                    "Trajectory must contain at least one waypoint");
              }
              waypoints.erase(waypoints.begin() + *waypoint_index);

              auto& constraints = traj_it->second.params.constraints;
              constraints.erase(
                  std::remove_if(
                      constraints.begin(), constraints.end(),
                      [&waypoint_uuid](const Constraint& constraint) {
                        const auto matches_id =
                            [&waypoint_uuid](const WaypointID& id) {
                              if (!std::holds_alternative<WaypointUUID>(id)) {
                                return false;
                              }
                              return std::get<WaypointUUID>(id).uuid ==
                                     waypoint_uuid->get();
                            };
                        if (matches_id(constraint.from)) {
                          return true;
                        }
                        if (constraint.to && matches_id(*constraint.to)) {
                          return true;
                        }
                        return false;
                      }),
                  constraints.end());

              for (auto& marker : traj_it->second.events) {
                if (!marker.from.target ||
                    !std::holds_alternative<WaypointUUID>(*marker.from.target)) {
                  continue;
                }
                if (std::get<WaypointUUID>(*marker.from.target).uuid ==
                    waypoint_uuid->get()) {
                  marker.from.target = std::nullopt;
                  marker.from.targetTimestamp = std::nullopt;
                }
              }

              CommitTrajectoryMutation(trajectory_uuid, "delete_waypoint", before);
              return EmptyResponse(204);
            });
      });
}

/// @brief Register full trajectory replacement route.
///
/// Replaces a trajectory payload while preserving the route UUID and updates
/// trajectory history and revision metadata.
void ApiServer::RegisterTrajectoryPutRoute() {
  m_router.Register(
      HttpMethod::kPut, "/api/v1/trajectories/{uuid}",
      [=, this](const Request& request, const RouteParams& params) {
        return WithTrajectoryMutation(
            request, params,
            [=, this](const std::string& trajectory_uuid_value,
                      auto trajectory_it, const wpi::util::json& before) {
              return WithParsedJsonBody<wpi::util::json>(
                  request, [=, this](const wpi::util::json& body) -> Response {
                    auto updated = TrajectoryFile::fromJson(body);
                    updated.uuid = trajectory_uuid_value;
                    trajectory_it->second = std::move(updated);
                    CommitTrajectoryMutation(trajectory_uuid_value,
                                             "put_trajectory", before);
                    return JsonModelResponseWithEtag(
                        200, TrajectoryRevisionToken(trajectory_uuid_value),
                        trajectory_it->second);
                  });
            });
      });
}

/// @brief Register trajectory rename route.
///
/// Validates non-empty and unique names before committing a trajectory
/// metadata mutation.
void ApiServer::RegisterTrajectoryRenameRoute() {
  m_router.Register(
      HttpMethod::kPost, "/api/v1/trajectories/{uuid}/rename",
      [=, this](const Request& request, const RouteParams& params) {
        return WithTrajectoryMutation(
            request, params,
            [=, this](const std::string& trajectory_uuid_value,
                      auto trajectory_it, const wpi::util::json& before) {
              auto& trajectory_value = trajectory_it->second;
              return WithParsedJsonBody<wpi::util::json>(
                  request, [&](const wpi::util::json& body) -> Response {
                    const auto name = RequireStringField(body, "name");
                    if (!name.has_value()) {
                      return InvalidJson(
                          "Request body must include string field 'name'");
                    }
                    const std::string name_value = name->get();
                    if (name_value.empty()) {
                      return ErrorResponse(ApiError::InvalidName,
                                           "Trajectory name must not be empty");
                    }
                    if (HasDuplicateNameInMap(m_trajectories,
                                              trajectory_uuid_value,
                                              name_value)) {
                      return ErrorResponse(ApiError::NameConflict,
                                      "Trajectory name already exists");
                    }
                    trajectory_value.name = name_value;
                    CommitTrajectoryMutation(trajectory_uuid_value,
                                             "rename_trajectory", before);
                    return JsonModelResponseWithEtag(
                        200, TrajectoryRevisionToken(trajectory_uuid_value),
                        trajectory_value);
                  });
            });
      });
}

/// @brief Register waypoint insertion route.
///
/// Parses a waypoint payload, validates UUID uniqueness, applies optional
/// insert index, and returns updated trajectory state.
void ApiServer::RegisterWaypointInsertRoute() {
  m_router.Register(
      HttpMethod::kPost, "/api/v1/trajectories/{uuid}/waypoints",
      [=, this](const Request& request, const RouteParams& params) {
        return WithTrajectoryMutation(
            request, params,
            [=, this](const std::string& trajectory_uuid, auto traj_it,
                      const wpi::util::json& before) {
              return WithParsedJsonBody<wpi::util::json>(
                  request, [&](const wpi::util::json& body) -> Response {
                    const auto item_json = RequireObjectField(body, "waypoint");
                    if (!item_json.has_value()) {
                      return InvalidJson(
                          "Request body must include object field 'waypoint'");
                    }

                    auto item = Waypoint::fromJson(item_json->get());
                    EnsureUuid(item.uuid);

                    auto& items = traj_it->second.params.waypoints;
                    if (FindByUuid(items, item.uuid)) {
                      return ErrorResponse(ApiError::UuidConflict,
                                      "Waypoint UUID already exists");
                    }

                    size_t insert_index = items.size();
                    std::string parse_error;
                    if (!ParseInsertIndex(body, items.size(), insert_index,
                                          parse_error)) {
                      return InvalidJson(parse_error);
                    }

                    items.insert(items.begin() + insert_index, std::move(item));
                    CommitTrajectoryMutation(trajectory_uuid, "add_waypoint",
                                             before);
                    return JsonModelResponseWithEtag(
                        201, TrajectoryRevisionToken(trajectory_uuid),
                        traj_it->second);
                  });
            });
      });
}

/// @brief Register waypoint reorder route.
///
/// Accepts explicit UUID order and enforces exact, duplicate-free waypoint
/// coverage.
void ApiServer::RegisterWaypointReorderRoute() {
  m_router.Register(
      HttpMethod::kPost, "/api/v1/trajectories/{uuid}/waypoints/reorder",
      [=, this](const Request& request, const RouteParams& params) {
        return WithTrajectoryMutation(
            request, params,
            [=, this](const std::string& trajectory_uuid, auto traj_it,
                      const wpi::util::json& before) {
              return WithParsedJsonBody<wpi::util::json>(
                  request, [&](const wpi::util::json& body) -> Response {
                    std::vector<std::string> order;
                    if (auto error = ParseOrderRequest(body, order)) {
                      return *error;
                    }

                    const auto& items = traj_it->second.params.waypoints;
                    if (order.size() != items.size()) {
                      return InvalidOrder(
                          "order must include every waypoint UUID exactly once");
                    }

                    std::unordered_set<std::string> seen;
                    std::vector<Waypoint> reordered;
                    reordered.reserve(items.size());
                    for (const auto& uuid : order) {
                      if (!seen.emplace(uuid).second) {
                        return InvalidOrder(
                            "order contains duplicate waypoint UUID");
                      }
                      const auto index = FindByUuid(items, uuid);
                      if (!index.has_value()) {
                        return InvalidOrder(
                            "order contains unknown waypoint UUID");
                      }
                      reordered.emplace_back(items[*index]);
                    }

                    traj_it->second.params.waypoints = std::move(reordered);
                    CommitTrajectoryMutation(trajectory_uuid,
                                             "reorder_waypoints", before);
                    return JsonModelResponseWithEtag(
                        200, TrajectoryRevisionToken(trajectory_uuid),
                        traj_it->second);
                  });
            });
      });
}

/// @brief Register constraint insertion route.
///
/// Parses a constraint payload, validates UUID uniqueness, applies optional
/// insert index, and returns updated trajectory state.
void ApiServer::RegisterConstraintInsertRoute() {
  m_router.Register(
      HttpMethod::kPost, "/api/v1/trajectories/{uuid}/constraints",
      [=, this](const Request& request, const RouteParams& params) {
        return WithTrajectoryMutation(
            request, params,
            [=, this](const std::string& trajectory_uuid, auto traj_it,
                      const wpi::util::json& before) {
              return WithParsedJsonBody<wpi::util::json>(
                  request, [&](const wpi::util::json& body) -> Response {
                    const auto item_json =
                        RequireObjectField(body, "constraint");
                    if (!item_json.has_value()) {
                      return InvalidJson(
                          "Request body must include object field 'constraint'");
                    }

                    auto item = Constraint::fromJson(item_json->get());
                    EnsureUuid(item.uuid);

                    auto& items = traj_it->second.params.constraints;
                    if (FindByUuid(items, item.uuid)) {
                      return ErrorResponse(ApiError::UuidConflict,
                                      "Constraint UUID already exists");
                    }

                    size_t insert_index = items.size();
                    std::string parse_error;
                    if (!ParseInsertIndex(body, items.size(), insert_index,
                                          parse_error)) {
                      return InvalidJson(parse_error);
                    }

                    items.insert(items.begin() + insert_index, std::move(item));
                    CommitTrajectoryMutation(trajectory_uuid, "add_constraint",
                                             before);
                    return JsonModelResponseWithEtag(
                        201, TrajectoryRevisionToken(trajectory_uuid),
                        traj_it->second);
                  });
            });
      });
}

/// @brief Register constraint deletion route.
///
/// Removes a constraint by UUID and records trajectory history.
void ApiServer::RegisterConstraintDeleteRoute() {
  m_router.Register(
      HttpMethod::kDelete,
      "/api/v1/trajectories/{uuid}/constraints/{constraintUuid}",
      [=, this](const Request& request, const RouteParams& params) {
        const auto item_uuid = RequireSubresourceUuid(params, "constraintUuid");
        if (!item_uuid.has_value()) {
          return item_uuid.error();
        }

        return WithTrajectoryMutation(
            request, params,
            [=, this](const std::string& trajectory_uuid, auto traj_it,
                      const wpi::util::json& before) {
              auto& items = traj_it->second.params.constraints;
              const auto item_index = FindByUuid(items, item_uuid->get());
              if (!item_index.has_value()) {
                return NotFound("Constraint not found");
              }

              items.erase(items.begin() + *item_index);
              CommitTrajectoryMutation(trajectory_uuid, "delete_constraint",
                                       before);
              return EmptyResponse(204);
            });
      });
}

/// @brief Register constraint reorder route.
///
/// Accepts explicit UUID order and enforces exact, duplicate-free constraint
/// coverage.
void ApiServer::RegisterConstraintReorderRoute() {
  m_router.Register(
      HttpMethod::kPost, "/api/v1/trajectories/{uuid}/constraints/reorder",
      [=, this](const Request& request, const RouteParams& params) {
        return WithTrajectoryMutation(
            request, params,
            [=, this](const std::string& trajectory_uuid, auto traj_it,
                      const wpi::util::json& before) {
              return WithParsedJsonBody<wpi::util::json>(
                  request, [&](const wpi::util::json& body) -> Response {
                    std::vector<std::string> order;
                    if (auto error = ParseOrderRequest(body, order)) {
                      return *error;
                    }

                    const auto& items = traj_it->second.params.constraints;
                    if (order.size() != items.size()) {
                      return InvalidOrder(
                          "order must include every constraint UUID exactly once");
                    }

                    std::unordered_set<std::string> seen;
                    std::vector<Constraint> reordered;
                    reordered.reserve(items.size());
                    for (const auto& uuid : order) {
                      if (!seen.emplace(uuid).second) {
                        return InvalidOrder(
                            "order contains duplicate constraint UUID");
                      }
                      const auto index = FindByUuid(items, uuid);
                      if (!index.has_value()) {
                        return InvalidOrder(
                            "order contains unknown constraint UUID");
                      }
                      reordered.emplace_back(items[*index]);
                    }

                    traj_it->second.params.constraints = std::move(reordered);
                    CommitTrajectoryMutation(trajectory_uuid,
                                             "reorder_constraints", before);
                    return JsonModelResponseWithEtag(
                        200, TrajectoryRevisionToken(trajectory_uuid),
                        traj_it->second);
                  });
            });
      });
}

/// @brief Register marker insertion route.
///
/// Parses an event marker payload, validates UUID uniqueness, applies optional
/// insert index, and returns updated trajectory state.
void ApiServer::RegisterMarkerInsertRoute() {
  m_router.Register(
      HttpMethod::kPost, "/api/v1/trajectories/{uuid}/markers",
      [=, this](const Request& request, const RouteParams& params) {
        return WithTrajectoryMutation(
            request, params,
            [=, this](const std::string& trajectory_uuid, auto traj_it,
                      const wpi::util::json& before) {
              return WithParsedJsonBody<wpi::util::json>(
                  request, [&](const wpi::util::json& body) -> Response {
                    const auto item_json = RequireObjectField(body, "marker");
                    if (!item_json.has_value()) {
                      return InvalidJson(
                          "Request body must include object field 'marker'");
                    }

                    auto item = EventMarker::fromJson(item_json->get());
                    EnsureUuid(item.uuid);

                    auto& items = traj_it->second.events;
                    if (FindByUuid(items, item.uuid)) {
                      return ErrorResponse(ApiError::UuidConflict,
                                      "Marker UUID already exists");
                    }

                    size_t insert_index = items.size();
                    std::string parse_error;
                    if (!ParseInsertIndex(body, items.size(), insert_index,
                                          parse_error)) {
                      return InvalidJson(parse_error);
                    }

                    items.insert(items.begin() + insert_index, std::move(item));
                    CommitTrajectoryMutation(trajectory_uuid, "add_marker",
                                             before);
                    return JsonModelResponseWithEtag(
                        201, TrajectoryRevisionToken(trajectory_uuid),
                        traj_it->second);
                  });
            });
      });
}

/// @brief Register marker deletion route.
///
/// Removes an event marker by UUID and records trajectory history.
void ApiServer::RegisterMarkerDeleteRoute() {
  m_router.Register(
      HttpMethod::kDelete, "/api/v1/trajectories/{uuid}/markers/{markerUuid}",
      [=, this](const Request& request, const RouteParams& params) {
        const auto item_uuid = RequireSubresourceUuid(params, "markerUuid");
        if (!item_uuid.has_value()) {
          return item_uuid.error();
        }

        return WithTrajectoryMutation(
            request, params,
            [=, this](const std::string& trajectory_uuid, auto traj_it,
                      const wpi::util::json& before) {
              auto& items = traj_it->second.events;
              const auto item_index = FindByUuid(items, item_uuid->get());
              if (!item_index.has_value()) {
                return NotFound("Marker not found");
              }

              items.erase(items.begin() + *item_index);
              CommitTrajectoryMutation(trajectory_uuid, "delete_marker",
                                       before);
              return EmptyResponse(204);
            });
      });
}

/// @brief Register marker reorder route.
///
/// Accepts explicit UUID order and enforces exact, duplicate-free marker
/// coverage.
void ApiServer::RegisterMarkerReorderRoute() {
  m_router.Register(
      HttpMethod::kPost, "/api/v1/trajectories/{uuid}/markers/reorder",
      [=, this](const Request& request, const RouteParams& params) {
        return WithTrajectoryMutation(
            request, params,
            [=, this](const std::string& trajectory_uuid, auto traj_it,
                      const wpi::util::json& before) {
              return WithParsedJsonBody<wpi::util::json>(
                  request, [&](const wpi::util::json& body) -> Response {
                    std::vector<std::string> order;
                    if (auto error = ParseOrderRequest(body, order)) {
                      return *error;
                    }

                    const auto& items = traj_it->second.events;
                    if (order.size() != items.size()) {
                      return InvalidOrder(
                          "order must include every marker UUID exactly once");
                    }

                    std::unordered_set<std::string> seen;
                    std::vector<EventMarker> reordered;
                    reordered.reserve(items.size());
                    for (const auto& uuid : order) {
                      if (!seen.emplace(uuid).second) {
                        return InvalidOrder(
                            "order contains duplicate marker UUID");
                      }
                      const auto index = FindByUuid(items, uuid);
                      if (!index.has_value()) {
                        return InvalidOrder(
                            "order contains unknown marker UUID");
                      }
                      reordered.emplace_back(items[*index]);
                    }

                    traj_it->second.events = std::move(reordered);
                    CommitTrajectoryMutation(trajectory_uuid, "reorder_markers",
                                             before);
                    return JsonModelResponseWithEtag(
                        200, TrajectoryRevisionToken(trajectory_uuid),
                        traj_it->second);
                  });
            });
      });
}

/// @brief Register document export route.
///
/// Returns a bundle containing the current project and all trajectories.
void ApiServer::RegisterExportRoute() {
  m_router.Register(
      HttpMethod::kGet, "/api/v1/export",
      [=, this](const Request&, const RouteParams&) {
        wpi::util::json body = wpi::util::json::object();
        body["schemaVersion"] = 1;
        body["exportedAt"] = "";
        body["project"] = m_project;
        body["trajectories"] = wpi::util::json::array();
        for (const auto& [_, traj] : m_trajectories) {
          body["trajectories"].emplace_back(traj);
        }
        return JsonResponse(200, body);
      });
}

/// @brief Register document import route.
///
/// Supports merge and replace import modes, updates revisions, clears history,
/// persists snapshots, and returns async-style operation summary payload.
void ApiServer::RegisterImportRoute() {
  m_router.Register(
      HttpMethod::kPost, "/api/v1/import",
      [=, this](const Request& request, const RouteParams&) {
        return WithParsedJsonBody<wpi::util::json>(
            request, [&](const wpi::util::json& body) -> Response {
              const auto mode = RequireStringField(body, "mode");
              const auto bundle = RequireObjectField(body, "bundle");
              if (!mode.has_value() || !bundle.has_value()) {
                return InvalidJson("Request body must include mode and bundle");
              }

              const std::string mode_value = mode->get();
              if (mode_value != "merge" && mode_value != "replace") {
                return ErrorResponse(ApiError::InvalidMode,
                                     "mode must be 'merge' or 'replace'");
              }

              const auto& bundle_value = bundle->get();
              if (!bundle_value.contains("project") ||
                  !bundle_value.contains("trajectories") ||
                  !bundle_value.at("trajectories").is_array()) {
                return ErrorResponse(
                  ApiError::InvalidJson,
                    "bundle must include project and trajectories array");
              }

              auto imported_project =
                  ProjectFile::fromJson(bundle_value.at("project"));
              EnsureUuid(imported_project.uuid);

              std::vector<TrajectoryFile> imported_trajectories;
              imported_trajectories.reserve(
                  bundle_value.at("trajectories").get_array().size());
              for (const auto& traj_json :
                   bundle_value.at("trajectories").get_array()) {
                auto traj = TrajectoryFile::fromJson(traj_json);
                EnsureUuid(traj.uuid);
                imported_trajectories.emplace_back(std::move(traj));
              }

              const int original_count = static_cast<int>(m_trajectories.size());
              m_history_by_scope.clear();

              m_project = std::move(imported_project);
              ++m_project_revision;

              int creates = 0;
              int updates = 0;
              int deletes = 0;
              if (mode_value == "replace") {
                deletes =
                    original_count > static_cast<int>(imported_trajectories.size())
                        ? original_count -
                              static_cast<int>(imported_trajectories.size())
                        : 0;
                m_trajectories.clear();
                m_trajectory_revisions.clear();
                for (auto& traj : imported_trajectories) {
                  const std::string uuid = traj.uuid;
                  m_trajectories[uuid] = std::move(traj);
                  m_trajectory_revisions[uuid] = 1;
                }
                creates = static_cast<int>(m_trajectories.size());
              } else {
                for (auto& traj : imported_trajectories) {
                  const std::string uuid = traj.uuid;
                  if (m_trajectories.contains(uuid)) {
                    ++updates;
                    ++m_trajectory_revisions[uuid];
                  } else {
                    ++creates;
                    m_trajectory_revisions[uuid] = 1;
                  }
                  m_trajectories[uuid] = std::move(traj);
                }
              }

              PersistStateSnapshot();

              OperationId operation_id = generateNextOperationId();
              OperationRecord record("");
              record.markComplete(ProjectRevisionToken());
              m_operations.insert_or_assign(operation_id, std::move(record));

              wpi::util::json response_body = wpi::util::json::object();
              response_body["operationId"] = operation_id;
              response_body["state"] = record.state;
              response_body["summary"] = wpi::util::json::object(
                  "projectAction", mode_value == "replace" ? "replace" : "merge",
                  "trajectoryCreates", creates, "trajectoryUpdates", updates,
                  "trajectoryDeletes", deletes);

              return JsonResponse(202, response_body);
            });
      });
}

/// @brief Register trajectory deletion route.
///
/// Removes the trajectory by UUID, records history, and returns a no-content
/// response with the updated trajectory-scoped ETag.
void ApiServer::RegisterTrajectoryDeleteRoute() {
  m_router.Register(
      HttpMethod::kDelete, "/api/v1/trajectories/{uuid}",
      [=, this](const Request& request, const RouteParams& params) {
        return WithTrajectoryMutation(
            request, params,
            [=, this](const std::string& trajectory_uuid, auto trajectory_it,
                      const wpi::util::json& before) {
              m_trajectories.erase(trajectory_it);
              CommitTrajectoryMutation(trajectory_uuid, "delete_trajectory",
                                       before);
              auto response = EmptyResponse(204);
              response.headers["ETag"] =
                  QuotedEtag(TrajectoryRevisionToken(trajectory_uuid));
              return response;
            });
      });
}

void ApiServer::RegisterDocumentRoutes() {
  RegisterProjectDocumentRoutes();
  RegisterTrajectoryDocumentRoutes();
}

void ApiServer::RegisterProjectDocumentRoutes() {
  const auto require_existing_variable =
      [this](const RouteParams& params, auto& variables,
             std::string_view not_found_message)
      -> std::expected<std::string, Response> {
    const auto variable_uuid =
        RequireSubresourceUuid(params, "variableUuid",
                               "Missing variable UUID parameter");
    if (!variable_uuid.has_value()) {
      return std::unexpected(variable_uuid.error());
    }

    const auto& variable_uuid_value = variable_uuid->get();
    if (!FindMappedValue(variables, variable_uuid_value).has_value()) {
      return std::unexpected(NotFound(not_found_message));
    }

    return variable_uuid_value;
  };

  const auto register_project_variable_put_route =
      [this, require_existing_variable](
          const char* route_path, std::string_view not_found_message,
          std::string_view empty_name_message,
          std::string_view name_conflict_message,
          std::string_view mutation_reason, auto map_accessor) {
        m_router.Register(
            HttpMethod::kPut, route_path,
            [this, require_existing_variable, map_accessor, not_found_message,
             empty_name_message, name_conflict_message,
             mutation_reason](const Request& request, const RouteParams& params) {
              auto& variables = map_accessor(m_project.variables);
              const auto variable_uuid_value_result = require_existing_variable(
                  params, variables, not_found_message);
              if (!variable_uuid_value_result.has_value()) {
                return variable_uuid_value_result.error();
              }
              const std::string variable_uuid_value =
                  *variable_uuid_value_result;

              return WithProjectMutation(
                  request, [&](std::string_view project_scope,
                               const wpi::util::json& before) {
                    using VariableType =
                        typename std::remove_reference_t<decltype(variables)>::
                            mapped_type;
                    return WithParsedJsonBody<VariableType>(
                        request, [&](const VariableType& body) -> Response {
                          auto updated = body;
                          if (updated.name.empty()) {
                            return InvalidJson(empty_name_message);
                          }

                          if (HasDuplicateVariableName(variable_uuid_value,
                                                       updated.name)) {
                            return ErrorResponse(ApiError::NameConflict,
                                            name_conflict_message);
                          }

                          variables[variable_uuid_value] = std::move(updated);
                          ++m_project_revision;
                          RecordScopeMutation(project_scope, mutation_reason,
                                              before);
                          return VariablePutResponse(
                              variable_uuid_value,
                              VariableEntryJsonWithUuid(
                                  variable_uuid_value,
                                  variables.at(variable_uuid_value)));
                        });
                  });
            });
      };

  m_router.Register(
      HttpMethod::kGet, "/api/v1/health",
      [=, this](const Request&, const RouteParams&) {
        const auto uptime_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - m_started_at)
                .count();

        wpi::util::json json = wpi::util::json::object();
        json["status"] = "ok";
        json["uptimeMs"] = static_cast<double>(uptime_ms);
        json["serverVersion"] = "0.1.0";

        return JsonResponse(200, json);
      });

  m_router.Register(
      HttpMethod::kGet, "/api/v1/project",
      [=, this](const Request&, const RouteParams&) {
        return JsonModelResponseWithEtag(200, ProjectRevisionToken(), m_project);
      });

  m_router.Register(
      HttpMethod::kPut, "/api/v1/project",
      [this](const Request& request, const RouteParams&) {
        return WithProjectMutation(
            request,
            [&, this](std::string_view project_scope,
                      const wpi::util::json& before) {
              return WithParsedJsonBody<wpi::util::json>(
                  request, [&, this](const wpi::util::json& body) {
                    auto updated = ProjectFile::fromJson(body);
                    EnsureUuid(updated.uuid);
                    m_project = std::move(updated);
                    ++m_project_revision;
                    RecordScopeMutation(project_scope, "put_project", before);
                    return JsonModelResponseWithEtag(200, ProjectRevisionToken(),
                                                     m_project);
                  });
            });
      });

  m_router.Register(
      HttpMethod::kPut, "/api/v1/project/config",
      [this](const Request& request, const RouteParams&) {
        return WithProjectMutation(
            request,
            [&, this](std::string_view project_scope,
                      const wpi::util::json& before) {
              return WithParsedJsonBody<wpi::util::json>(
                  request, [&, this](const wpi::util::json& body) {
                    auto updated = RobotConfig::fromJson(body);
                    m_project.config = std::move(updated);
                    ++m_project_revision;
                    RecordScopeMutation(project_scope, "put_project_config",
                                        before);
                    return JsonModelResponseWithEtag(200, ProjectRevisionToken(),
                                                     m_project.config);
                  });
            });
      });

  m_router.Register(
      HttpMethod::kPut, "/api/v1/project/codegen",
      [this](const Request& request, const RouteParams&) {
        return WithProjectMutation(
            request,
            [&, this](std::string_view project_scope,
                      const wpi::util::json& before) {
              return WithParsedJsonBody<CodeGenConfig>(
                  request, [&, this](const CodeGenConfig& body) {
                    m_project.codegen = body;
                    ++m_project_revision;
                    RecordScopeMutation(project_scope, "put_project_codegen",
                                        before);
                    return JsonModelResponseWithEtag(200, ProjectRevisionToken(),
                                                     m_project.codegen);
                  });
            });
      });

  m_router.Register(
      HttpMethod::kPut, "/api/v1/project/type",
      [this](const Request& request, const RouteParams&) {
        return WithProjectMutation(
            request,
            [&, this](std::string_view project_scope,
                      const wpi::util::json& before) {
              return WithParsedJsonBody<DriveType>(
                  request, [&, this](const DriveType& body) {
                    m_project.type = body;
                    ++m_project_revision;
                    RecordScopeMutation(project_scope, "put_project_type",
                                        before);
                    return JsonModelResponseWithEtag(200, ProjectRevisionToken(),
                                                     m_project.type);
                  });
            });
      });

  register_project_variable_put_route(
      "/api/v1/project/variables/expressions/{variableUuid}",
      "Expression variable not found",
      "Expression variable name must not be empty",
      "Expression variable name already exists", "put_expression_variable",
      [](Variables& vars) -> auto& { return vars.expressions; });

  register_project_variable_put_route(
      "/api/v1/project/variables/translations/{variableUuid}",
      "Translation variable not found",
      "Translation variable name must not be empty",
      "Translation variable name already exists", "put_translation_variable",
      [](Variables& vars) -> auto& { return vars.translations; });

  register_project_variable_put_route(
      "/api/v1/project/variables/poses/{variableUuid}",
      "Pose variable not found", "Pose variable name must not be empty",
      "Pose variable name already exists", "put_pose_variable",
      [](Variables& vars) -> auto& { return vars.poses; });

  register_project_variable_put_route(
      "/api/v1/project/variables/regions/{variableUuid}",
      "Region variable not found", "Region variable name must not be empty",
      "Region variable name already exists", "put_region_variable",
      [](Variables& vars) -> auto& { return vars.regions; });

  m_router.Register(HttpMethod::kPost, "/api/v1/project/undo",
                    [this](const Request& request, const RouteParams&) {
                      return WithProjectPrecondition(
                          request, [&](std::string_view project_scope) {
                            if (auto history_error = HandleUndo(project_scope)) {
                              return *history_error;
                            }
                            return JsonModelResponseWithEtag(
                                200, ProjectRevisionToken(), m_project);
                          });
                    });

  m_router.Register(HttpMethod::kPost, "/api/v1/project/redo",
                    [this](const Request& request, const RouteParams&) {
                      return WithProjectPrecondition(
                          request, [&](std::string_view project_scope) {
                            if (auto history_error = HandleRedo(project_scope)) {
                              return *history_error;
                            }
                            return JsonModelResponseWithEtag(
                                200, ProjectRevisionToken(), m_project);
                          });
                    });
}

void ApiServer::RegisterTrajectoryDocumentRoutes() {
  RegisterTrajectoryListRoute();
  RegisterTrajectoryCreateRoute();
  RegisterTrajectoryGetRoute();

  RegisterTrajectoryHistoryRoute(
      HttpMethod::kPost, "/api/v1/trajectories/{uuid}/undo",
      [this](std::string_view scope_key) { return HandleUndo(scope_key); },
      nullptr);
  RegisterTrajectoryHistoryRoute(
      HttpMethod::kPost, "/api/v1/trajectories/{uuid}/redo",
      [this](std::string_view scope_key) { return HandleRedo(scope_key); },
      nullptr);
  RegisterTrajectoryPutRoute();
  RegisterTrajectoryRenameRoute();
  RegisterWaypointInsertRoute();
  RegisterWaypointReorderRoute();
  RegisterConstraintInsertRoute();
  RegisterConstraintDeleteRoute();
  RegisterConstraintReorderRoute();
  RegisterMarkerInsertRoute();
  RegisterMarkerDeleteRoute();
  RegisterMarkerReorderRoute();
  RegisterWaypointDeleteRoute();
  RegisterExportRoute();
  RegisterImportRoute();
  RegisterTrajectoryDeleteRoute();
}

}  // namespace choreo::state_server

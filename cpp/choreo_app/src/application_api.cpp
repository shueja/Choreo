#include "choreo/document/application_api.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <fstream>
#include <future>
#include <initializer_list>
#include <iterator>
#include <map>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <wpi/util/json.hpp>

#include "choreo/document/document.hpp"
#include "choreo/document/document_store.hpp"
#include "choreo/document/evaluation.hpp"
#include "choreo/document/generation_process.hpp"

namespace choreo::document {
namespace {

using Json = wpi::util::json;
using ApiResponse = choreo::api::HttpResponse;

ApiResponse JsonResponse(int status, Json body) {
  return ApiResponse{status, "application/json", body.to_string(), {}};
}

ApiResponse ApiError(int status, std::string_view code,
                     std::string_view message) {
  return JsonResponse(status, Json::object("code", code, "message", message));
}

ApiResponse OperationAccepted(std::string_view operationId,
                              std::string_view operation) {
  return JsonResponse(202, Json::object("operationId", operationId, "operation",
                                        operation, "accepted", true));
}

void ApplyMergePatch(Json& target, const Json& patch) {
  if (!patch.is_object()) {
    target = patch;
    return;
  }
  if (!target.is_object()) {
    target = Json::object();
  }
  for (const auto& [key, value] : patch.get_object()) {
    if (value.is_null()) {
      target.erase(key);
    } else {
      ApplyMergePatch(target[key], value);
    }
  }
}

bool HasOnlyKeys(const Json& value,
                 std::initializer_list<std::string_view> allowedKeys) {
  if (!value.is_object() || value.empty()) {
    return false;
  }
  return std::all_of(value.get_object().begin(), value.get_object().end(),
                     [&](const auto& item) {
                       return std::find(allowedKeys.begin(), allowedKeys.end(),
                                        item.first) != allowedKeys.end();
                     });
}

std::string ReadText(const std::filesystem::path& path) {
  std::ifstream input{path, std::ios::binary};
  if (!input) {
    throw std::runtime_error("cannot open file: " + path.string());
  }
  std::string content{std::istreambuf_iterator<char>{input},
                      std::istreambuf_iterator<char>{}};
  if (input.bad()) {
    throw std::runtime_error("cannot read file: " + path.string());
  }
  return content;
}

std::string Lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](char character) {
    return static_cast<char>(
        std::tolower(static_cast<unsigned char>(character)));
  });
  return value;
}

std::filesystem::path NormalizeProjectPath(std::string_view value) {
  if (value.empty()) {
    throw std::invalid_argument("project path is required");
  }
  auto path = std::filesystem::absolute(std::filesystem::path{value})
                  .lexically_normal();
  if (Lower(path.extension().string()) != ".chor") {
    throw std::invalid_argument("project path must name a .chor file");
  }
  return path;
}

std::string NewUuid() {
  std::array<std::uint8_t, 16> bytes{};
  std::random_device random;
  for (auto& byte : bytes) {
    byte = static_cast<std::uint8_t>(random());
  }
  bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0f) | 0x40);
  bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3f) | 0x80);
  constexpr char digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(36);
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    if (index == 4 || index == 6 || index == 8 || index == 10) {
      result.push_back('-');
    }
    result.push_back(digits[bytes[index] >> 4]);
    result.push_back(digits[bytes[index] & 0x0f]);
  }
  return result;
}

std::optional<std::uint64_t> RevisionFromTag(std::string_view tag) {
  constexpr std::string_view prefix = "\"revision-";
  if (!tag.starts_with(prefix) || tag.size() <= prefix.size() ||
      tag.back() != '"') {
    return std::nullopt;
  }
  tag.remove_prefix(prefix.size());
  tag.remove_suffix(1);
  std::uint64_t revision = 0;
  const auto [end, error] =
      std::from_chars(tag.data(), tag.data() + tag.size(), revision);
  if (error != std::errc{} || end != tag.data() + tag.size()) {
    return std::nullopt;
  }
  return revision;
}

std::string QueryValue(std::string_view target, std::string_view name) {
  auto query = target.find('?');
  if (query == std::string_view::npos) {
    return {};
  }
  target.remove_prefix(query + 1);
  while (!target.empty()) {
    auto next = target.find('&');
    auto item = target.substr(0, next);
    auto equals = item.find('=');
    if (equals != std::string_view::npos && item.substr(0, equals) == name) {
      return std::string{item.substr(equals + 1)};
    }
    if (next == std::string_view::npos) {
      break;
    }
    target.remove_prefix(next + 1);
  }
  return {};
}

Json ErrorBody(const ApiResponse& response) {
  try {
    auto body = Json::parse_or_throw(response.body);
    if (body.is_object() && body.contains("code") && body.contains("message")) {
      return body;
    }
  } catch (...) {
  }
  return Json::object("code", "INTERNAL_ERROR", "message", "Operation failed");
}

}  // namespace

struct ApplicationApi::Impl : public std::enable_shared_from_this<Impl> {
  using Task = std::function<void()>;

  struct Generation {
    std::string operationId;
    std::string trajectoryId;
    std::unique_ptr<GenerationProcess> process;
    bool cancelRequested = false;
  };

  struct CachedGeneration {
    std::string requestTarget;
    ApiResponse response;
  };

  struct CachedMutation {
    std::string requestTarget;
    ApiResponse response;
  };

  struct PublishedSnapshot {
    std::uint64_t revision;
    std::string projectId;
    std::string body;
  };

  Impl(choreo::api::ApiServer& serverIn, Options optionsIn)
      : server{serverIn}, options{std::move(optionsIn)} {
    persistenceWorker = std::thread([this] { RunPersistenceWorker(); });
    worker = std::thread([this] { RunWorker(); });
    if (!options.initialProjectPath.empty()) {
      const auto path = options.initialProjectPath;
      auto result = std::make_shared<std::promise<ApiResponse>>();
      auto response = result->get_future();
      if (!Post([this, path, result] {
            try {
              result->set_value(OpenProject(path, false));
            } catch (...) {
              result->set_exception(std::current_exception());
            }
          })) {
        StopWorker();
            StopPersistenceWorker();
        throw std::runtime_error("application worker is stopping");
      }
      try {
        auto initialResponse = response.get();
        if (initialResponse.status < 200 || initialResponse.status >= 300) {
          throw std::runtime_error(initialResponse.body);
        }
      } catch (...) {
        StopWorker();
        StopPersistenceWorker();
        throw;
      }
    }
  }

  ~Impl() {
    StopPersistenceWorker();
    StopWorker();
  }

  template <typename Callback>
  static void WithOwner(const std::weak_ptr<Impl>& weak, Callback&& callback) {
    if (auto self = weak.lock()) {
      std::forward<Callback>(callback)(std::move(self));
    }
  }

  template <typename Callback>
  static ApiResponse WithHttpOwner(const std::weak_ptr<Impl>& weak,
                                   Callback&& callback) {
    if (auto self = weak.lock()) {
      return std::forward<Callback>(callback)(std::move(self));
    }
    return ApiError(503, "INTERNAL_ERROR", "API is stopping");
  }

  void RegisterRoutes() {
    std::weak_ptr<Impl> weak = shared_from_this();
    server.RegisterRoute("POST", "/api/v1/session/open",
                         [weak](const choreo::api::HttpRequest& request) {
                           return WithHttpOwner(
                               weak, [&request](std::shared_ptr<Impl> self) {
                                 return self->OpenSession(request);
                               });
                         });
    server.RegisterRoute("POST", "/api/v1/session/new",
                         [weak](const choreo::api::HttpRequest& request) {
                           return WithHttpOwner(
                               weak, [&request](std::shared_ptr<Impl> self) {
                                 return self->NewSession(request);
                               });
                         });
    server.RegisterRoute("POST", "/api/v1/session/close",
                         [weak](const choreo::api::HttpRequest&) {
                           return WithHttpOwner(weak,
                                                [](std::shared_ptr<Impl> self) {
                                                  return self->CloseSession();
                                                });
                         });
    server.RegisterRoute("POST", "/api/v1/session/undo",
                         [weak](const choreo::api::HttpRequest& request) {
                           return WithHttpOwner(
                               weak, [&request](std::shared_ptr<Impl> self) {
                                 return self->HistoryAction(request, true);
                               });
                         });
    server.RegisterRoute("POST", "/api/v1/session/redo",
                         [weak](const choreo::api::HttpRequest& request) {
                           return WithHttpOwner(
                               weak, [&request](std::shared_ptr<Impl> self) {
                                 return self->HistoryAction(request, false);
                               });
                         });
    server.RegisterRoute(
        "GET", "/api/v1/session/status",
        [weak](const choreo::api::HttpRequest&) {
          return WithHttpOwner(weak, [](std::shared_ptr<Impl> self) {
            return self->SessionStatusResponse();
          });
        });
    server.RegisterRoute(
        "GET", "/api/v1/snapshot", [weak](const choreo::api::HttpRequest&) {
          return WithHttpOwner(weak, [](std::shared_ptr<Impl> self) {
            return self->SnapshotResponse();
          });
        });
    server.RegisterRoute("GET", "/api/v1/projects/{projectId}",
                         [weak](const choreo::api::HttpRequest& request) {
                           return WithHttpOwner(
                               weak, [&request](std::shared_ptr<Impl> self) {
                                 return self->ProjectResponse(request);
                               });
                         });
    server.RegisterRoute("PATCH", "/api/v1/projects/{projectId}",
                         [weak](const choreo::api::HttpRequest& request) {
                           return WithHttpOwner(
                               weak, [&request](std::shared_ptr<Impl> self) {
                                 return self->PatchResource(request, true);
                               });
                         });
    server.RegisterRoute(
        "PATCH", "/api/v1/projects/{projectId}/variables/{variableId}",
        [weak](const choreo::api::HttpRequest& request) {
          return WithHttpOwner(weak, [&request](std::shared_ptr<Impl> self) {
            return self->PatchVariable(request);
          });
        });
    server.RegisterRoute(
        "POST", "/api/v1/projects/{projectId}/geometry-controls",
        [weak](const choreo::api::HttpRequest& request) {
          return WithHttpOwner(weak, [&request](std::shared_ptr<Impl> self) {
            return self->GeometryControls(request);
          });
        });
    server.RegisterRoute("GET", "/api/v1/projects/{projectId}/trajectories",
                         [weak](const choreo::api::HttpRequest& request) {
                           return WithHttpOwner(
                               weak, [&request](std::shared_ptr<Impl> self) {
                                 return self->TrajectoriesResponse(request);
                               });
                         });
    server.RegisterRoute("POST", "/api/v1/projects/{projectId}/trajectories",
                         [weak](const choreo::api::HttpRequest& request) {
                           return WithHttpOwner(
                               weak, [&request](std::shared_ptr<Impl> self) {
                                 return self->CreateTrajectory(request);
                               });
                         });
    server.RegisterRoute(
        "GET", "/api/v1/projects/{projectId}/trajectories/{trajectoryId}",
        [weak](const choreo::api::HttpRequest& request) {
          return WithHttpOwner(weak, [&request](std::shared_ptr<Impl> self) {
            return self->TrajectoryResponse(request);
          });
        });
    server.RegisterRoute(
        "PATCH", "/api/v1/projects/{projectId}/trajectories/{trajectoryId}",
        [weak](const choreo::api::HttpRequest& request) {
          return WithHttpOwner(weak, [&request](std::shared_ptr<Impl> self) {
            return self->PatchResource(request, false);
          });
        });
    struct SubresourceRoute {
      std::string_view path;
      std::string_view field;
      std::string_view itemParameter;
    };
    const std::array<SubresourceRoute, 3> subresources{{
        {"waypoints", "waypoints", "waypointId"},
        {"constraints", "constraints", "constraintId"},
        {"event-markers", "events", "eventMarkerId"},
    }};
    for (const auto& [route, field, itemParameter] : subresources) {
      const auto collectionPath =
          "/api/v1/projects/{projectId}/trajectories/{trajectoryId}/" +
          std::string{route};
      const auto fieldName = std::string{field};
      const auto itemParameterName = std::string{itemParameter};
      server.RegisterRoute(
          "GET", collectionPath,
          [weak, fieldName](const choreo::api::HttpRequest& request) {
            return WithHttpOwner(
                weak, [&request, &fieldName](std::shared_ptr<Impl> self) {
                  return self->TrajectorySubresourceResponse(request, fieldName,
                                                             std::nullopt);
                });
          });
      server.RegisterRoute(
          "POST", collectionPath,
          [weak, fieldName](const choreo::api::HttpRequest& request) {
            return WithHttpOwner(
                weak, [&request, &fieldName](std::shared_ptr<Impl> self) {
                  return self->MutateTrajectorySubresource(request, fieldName,
                                                           {}, "create");
                });
          });
      const auto itemPath = collectionPath + "/{" + itemParameterName + "}";
      server.RegisterRoute(
          "GET", itemPath,
          [weak, fieldName,
           itemParameterName](const choreo::api::HttpRequest& request) {
            return WithHttpOwner(
                weak, [&request, &fieldName,
                       &itemParameterName](std::shared_ptr<Impl> self) {
                  return self->TrajectorySubresourceResponse(
                      request, fieldName,
                      request.pathParameters.at(itemParameterName));
                });
          });
      for (const auto* action : {"patch", "delete"}) {
        const auto method =
            std::string_view{action} == "patch" ? "PATCH" : "DELETE";
        server.RegisterRoute(
            method, itemPath,
            [weak, fieldName, itemParameterName, action = std::string{action}](
                const choreo::api::HttpRequest& request) {
              return WithHttpOwner(
                  weak, [&request, &fieldName, &itemParameterName,
                         &action](std::shared_ptr<Impl> self) {
                    return self->MutateTrajectorySubresource(
                        request, fieldName, itemParameterName, action);
                  });
            });
      }
    }
    server.RegisterRoute("POST", "/api/v1/session/save",
                         [weak](const choreo::api::HttpRequest& request) {
                           return WithHttpOwner(
                               weak, [&request](std::shared_ptr<Impl> self) {
                                 return self->SaveSession(request);
                               });
                         });
    server.RegisterRoute(
        "POST",
        "/api/v1/projects/{projectId}/trajectories/{trajectoryId}/generations",
        [weak](const choreo::api::HttpRequest& request) {
          return WithHttpOwner(weak, [&request](std::shared_ptr<Impl> self) {
            return self->StartGeneration(request);
          });
        });
    server.RegisterRoute(
        "DELETE",
        "/api/v1/projects/{projectId}/trajectories/{trajectoryId}/generations/"
        "{operationId}",
        [weak](const choreo::api::HttpRequest& request) {
          return WithHttpOwner(weak, [&request](std::shared_ptr<Impl> self) {
            return self->CancelGeneration(request);
          });
        });
    server.RegisterWebSocketRoute("/api/v1/events", {});
    server.RegisterWebSocketRoute(
        "/api/v1/generator-progress",
        choreo::api::WebSocketCallbacks{
            [weak](choreo::api::WebSocketId connection,
                   std::string_view target) {
              WithOwner(weak, [connection, target](std::shared_ptr<Impl> self) {
                auto operationId = QueryValue(target, "operationId");
                self->Post(
                    [self, connection, operationId = std::move(operationId)] {
                      if (!operationId.empty() &&
                          self->generations.contains(operationId)) {
                        self->progressConnections[connection] = operationId;
                      }
                    });
              });
            },
            [weak](choreo::api::WebSocketId connection,
                   std::string_view message) {
              WithOwner(weak, [connection, copied = std::string{message}](
                                  std::shared_ptr<Impl> self) mutable {
                self->Post([self, connection, copied = std::move(copied)] {
                  self->ForwardGeneratorMessage(connection, copied);
                });
              });
            },
            [weak](choreo::api::WebSocketId connection, std::uint16_t,
                   std::string_view) {
              WithOwner(weak, [connection](std::shared_ptr<Impl> self) {
                self->Post([self, connection] {
                  self->progressConnections.erase(connection);
                });
              });
            }});
  }

  bool Post(Task task) {
    {
      std::scoped_lock lock{queueMutex};
      if (stopping) {
        return false;
      }
      tasks.emplace_back(std::move(task));
    }
    queueCondition.notify_one();
    return true;
  }

  bool PostPersistence(Task task) {
    {
      std::scoped_lock lock{persistenceQueueMutex};
      if (persistenceStopping) {
        return false;
      }
      persistenceTasks.emplace_back(std::move(task));
    }
    persistenceQueueCondition.notify_one();
    return true;
  }

  void StopPersistenceWorker() {
    {
      std::scoped_lock lock{persistenceQueueMutex};
      persistenceStopping = true;
    }
    persistenceQueueCondition.notify_all();
    if (persistenceWorker.joinable()) {
      persistenceWorker.join();
    }
  }

  void FlushPersistence() {
    auto flushed = std::make_shared<std::promise<void>>();
    auto completion = flushed->get_future();
    if (PostPersistence([flushed] { flushed->set_value(); })) {
      completion.wait();
    }
  }

  void RunPersistenceWorker() {
    while (true) {
      Task task;
      {
        std::unique_lock lock{persistenceQueueMutex};
        persistenceQueueCondition.wait(
            lock, [&] { return persistenceStopping || !persistenceTasks.empty(); });
        if (!persistenceTasks.empty()) {
          task = std::move(persistenceTasks.front());
          persistenceTasks.pop_front();
        } else if (persistenceStopping) {
          break;
        }
      }
      if (task) {
        try {
          task();
        } catch (...) {
        }
      }
    }
  }

  void StopWorker() {
    {
      std::scoped_lock lock{queueMutex};
      stopping = true;
    }
    queueCondition.notify_all();
    if (worker.joinable()) {
      worker.join();
    }
  }

  void EmitSessionOperation(
      std::string_view operationId, std::string_view operation,
      std::string_view status, std::string_view message,
      std::optional<std::uint64_t> revision = std::nullopt,
      std::optional<Json> error = std::nullopt) {
    Json event = Json::object("type", "session.operation", "operationId",
                              operationId, "operation", operation, "status",
                              status, "message", message);
    if (revision) {
      event["revision"] = *revision;
    }
    if (error) {
      event["error"] = std::move(*error);
    }
    server.BroadcastText("/api/v1/events", event.to_string());
  }

  template <typename Work>
  void QueueSessionOperation(std::string operationId, std::string operation,
                             Work work) {
    const auto fallbackOperationId = operationId;
    const auto fallbackOperation = operation;
    auto self = shared_from_this();
    auto task = [self, operationId = std::move(operationId),
                 operation = std::move(operation),
                 work = std::move(work)]() mutable {
      try {
        auto response = work();
        const auto revision =
            self->store ? std::optional{self->store->Revision()} : std::nullopt;
        if (response.status < 400) {
          self->EmitSessionOperation(operationId, operation, "succeeded",
                                     "Operation completed", revision);
          return;
        }
        self->EmitSessionOperation(operationId, operation, "failed",
                                   "Operation failed", revision,
                                   ErrorBody(response));
      } catch (const std::invalid_argument& exception) {
        self->EmitSessionOperation(
            operationId, operation, "failed", "Operation failed",
            self->store ? std::optional{self->store->Revision()} : std::nullopt,
            Json::object("code", "VALIDATION_ERROR", "message",
                         exception.what()));
      } catch (const std::exception& exception) {
        self->EmitSessionOperation(
            operationId, operation, "failed", "Operation failed",
            self->store ? std::optional{self->store->Revision()} : std::nullopt,
            Json::object("code", "INTERNAL_ERROR", "message",
                         exception.what()));
      } catch (...) {
        self->EmitSessionOperation(
            operationId, operation, "failed", "Operation failed",
            self->store ? std::optional{self->store->Revision()} : std::nullopt,
            Json::object("code", "INTERNAL_ERROR", "message",
                         "Unknown operation failure"));
      }
    };
    if (!Post(std::move(task))) {
      EmitSessionOperation(
          fallbackOperationId, fallbackOperation, "failed", "API is stopping",
          std::nullopt,
          Json::object("code", "INTERNAL_ERROR", "message", "API is stopping"));
    }
  }

  ApiResponse OpenSession(const choreo::api::HttpRequest& request) {
    std::string source;
    std::optional<std::filesystem::path> projectPath;
    try {
      auto body = Json::parse_or_throw(request.body);
      if (!body.is_object()) {
        throw std::invalid_argument("request body must be an object");
      }
      source = body.at("source").get_string();
      if (source == "path") {
        projectPath = NormalizeProjectPath(body.at("path").get_string());
      } else if (source != "picker") {
        throw std::invalid_argument("source must be picker or path");
      }
    } catch (const std::exception& error) {
      return ApiError(400, "VALIDATION_ERROR", error.what());
    }
    if (source == "picker" && !options.promptForProjectPath) {
      return ApiError(501, "PICKER_UNAVAILABLE",
                      "No project file picker is configured");
    }

    const auto operationId = NewUuid();
    EmitSessionOperation(operationId, "open", "running",
                         source == "picker" ? "Waiting for project selection"
                                            : "Opening project");
    if (projectPath) {
      auto self = shared_from_this();
      QueueSessionOperation(operationId, "open",
                            [self, path = std::move(*projectPath)] {
                              return self->OpenProject(path);
                            });
      return OperationAccepted(operationId, "open");
    }

    auto weak = weak_from_this();
    auto callbackUsed = std::make_shared<std::atomic_bool>(false);
    try {
      // The injected picker must schedule UI work and return promptly.
      options.promptForProjectPath(
          [weak, operationId,
           callbackUsed](std::optional<std::filesystem::path> path) mutable {
            if (callbackUsed->exchange(true)) {
              return;
            }
            if (!path) {
              WithOwner(weak, [operationId](std::shared_ptr<Impl> self) {
                self->EmitSessionOperation(operationId, "open", "cancelled",
                                           "Project selection cancelled");
              });
              return;
            }
            WithOwner(weak, [operationId, path = std::move(*path)](
                                std::shared_ptr<Impl> self) mutable {
              self->QueueSessionOperation(
                  operationId, "open", [self, path = std::move(path)] {
                    return self->OpenProject(
                        NormalizeProjectPath(path.string()));
                  });
            });
          });
    } catch (const std::exception& error) {
      if (!callbackUsed->exchange(true)) {
        EmitSessionOperation(
            operationId, "open", "failed", "Could not open project picker",
            std::nullopt,
            Json::object("code", "INTERNAL_ERROR", "message", error.what()));
      }
    }
    return OperationAccepted(operationId, "open");
  }

  ApiResponse OpenProject(const std::filesystem::path& projectPath,
                          bool notify = true) {
    FlushPersistence();
    auto files = DiscoverFiles(projectPath);
    auto document = ReadDocument(files);
    if (auto failure = PersistActiveSession()) {
      return *failure;
    }
    StopGenerations();
    auto replacement = std::make_unique<DocumentStore>(std::move(document));
    replacement->ReloadFiles(files);
    store = std::move(replacement);
    ++sessionGeneration;
    documentFiles = std::move(files);
    hasProjectPath.store(true, std::memory_order_release);
    PublishSnapshot(notify);
    return JsonResponse(200, Json::object("status", "opened"));
  }

  ApiResponse NewSession(const choreo::api::HttpRequest& request) {
    const auto operationId = NewUuid();
    const auto body = request.body;
    EmitSessionOperation(operationId, "new", "running",
                         "Creating project session");
    auto self = shared_from_this();
    QueueSessionOperation(operationId, "new", [self, body] {
      auto requestBody = Json::parse_or_throw(body);
      if (!requestBody.is_object()) {
        throw std::invalid_argument("request body must be an object");
      }
        std::optional<std::filesystem::path> path;
        if (requestBody.contains("projectPath")) {
        path = NormalizeProjectPath(
          requestBody.at("projectPath").get_string());
        }
      auto project = requestBody.at("project").get<Project>();
      if (auto failure = self->PersistActiveSession()) {
        return *failure;
      }
      self->StopGenerations();
      Document document;
      document.project = std::move(project);
      auto replacement = std::make_unique<DocumentStore>(std::move(document));
      DocumentFiles files;
      files.project = path.value_or(std::filesystem::path{});
      self->store = std::move(replacement);
      ++self->sessionGeneration;
      self->documentFiles = std::move(files);
      self->hasProjectPath.store(path.has_value(), std::memory_order_release);
      self->PublishSnapshot();
      return JsonResponse(201, Json::object("created", true));
    });
    return OperationAccepted(operationId, "new");
  }

  ApiResponse CloseSession() {
    const auto operationId = NewUuid();
    EmitSessionOperation(operationId, "close", "running", "Closing session");
    auto self = shared_from_this();
    QueueSessionOperation(operationId, "close", [self] {
      if (auto failure = self->PersistActiveSession()) {
        return *failure;
      }
      self->StopGenerations();
      self->store.reset();
      ++self->sessionGeneration;
      self->documentFiles = {};
      self->hasProjectPath.store(false, std::memory_order_release);
      self->PublishSnapshot();
      return JsonResponse(200, Json::object("closed", true));
    });
    return OperationAccepted(operationId, "close");
  }

  ApiResponse SaveSession(const choreo::api::HttpRequest& request) {
    std::optional<std::filesystem::path> saveAs;
    try {
      if (!request.body.empty()) {
        auto requestBody = Json::parse_or_throw(request.body);
        if (!requestBody.is_object()) {
          throw std::invalid_argument("request body must be an object");
        }
        if (requestBody.contains("projectPath") &&
            !requestBody.at("projectPath").is_null()) {
          saveAs =
              NormalizeProjectPath(requestBody.at("projectPath").get_string());
        }
      }
    } catch (const std::exception& error) {
      return ApiError(400, "VALIDATION_ERROR", error.what());
    }

    if (!saveAs &&
        !hasProjectPath.load(std::memory_order_acquire) &&
        !options.promptForSavePath) {
      return ApiError(501, "PICKER_UNAVAILABLE",
                      "No project save picker is configured");
    }

    const auto operationId = NewUuid();
    EmitSessionOperation(operationId, "save", "running", "Saving session");
    auto self = shared_from_this();
    if (!saveAs && !hasProjectPath.load(std::memory_order_acquire)) {
      auto weak = weak_from_this();
      options.promptForSavePath(
          [weak, operationId](
              std::optional<std::filesystem::path> path) mutable {
            if (!path) {
              WithOwner(weak, [operationId](std::shared_ptr<Impl> owner) {
                owner->EmitSessionOperation(operationId, "save", "cancelled",
                                            "Save location selection cancelled");
              });
              return;
            }
            WithOwner(weak, [operationId, path = std::move(*path)](
                                std::shared_ptr<Impl> owner) mutable {
              owner->QueueSessionOperation(
                  operationId, "save", [owner, path = std::move(path)] {
                    return owner->SaveSessionFiles(path);
                  });
            });
          });
      return OperationAccepted(operationId, "save");
    }

    QueueSessionOperation(operationId, "save", [self, saveAs] {
      return self->SaveSessionFiles(saveAs);
    });
    return OperationAccepted(operationId, "save");
  }

  ApiResponse SaveSessionFiles(
      const std::optional<std::filesystem::path>& saveAs) {
    FlushPersistence();
    if (!store) {
      return ApiError(409, "SESSION_NOT_OPEN", "No project is open");
    }
    auto files = documentFiles;
    if (saveAs) {
      files.project = *saveAs;
      for (auto& [id, path] : files.trajectories) {
        path = saveAs->parent_path() / path.filename();
      }
    }
    for (const auto& [id, trajectory] : store->Snapshot()->trajectories) {
      static_cast<void>(trajectory);
      if (!files.trajectories.contains(id)) {
        files.trajectories.emplace(
            id, files.project.parent_path() / (id + ".traj"));
      }
    }
    if (files.project.empty()) {
      return ApiError(422, "VALIDATION_ERROR",
                      "A project path is required for the first save");
    }
    if (auto failure = PersistFiles(files)) {
      return *failure;
    }
    documentFiles = std::move(files);
    hasProjectPath.store(true, std::memory_order_release);
    PublishSnapshot();
    return JsonResponse(200, Json::object("saved", true, "revision",
                                          store->Revision()));
  }

  std::optional<ApiResponse> PersistActiveSession() {
    FlushPersistence();
    if (!store) {
      return std::nullopt;
    }
    if (documentFiles.project.empty()) {
      if (store->Revision() == 0) {
        return std::nullopt;
      }
      return ApiError(
          422, "VALIDATION_ERROR",
          "A project path is required before replacing the session");
    }
    bool dirty = store->IsProjectDirty();
    for (const auto& [id, path] : documentFiles.trajectories) {
      static_cast<void>(path);
      dirty = dirty || store->IsTrajectoryDirty(id);
    }
    if (!dirty && !persistenceBlocked) {
      return std::nullopt;
    }
    return PersistFiles(documentFiles);
  }

  std::optional<ApiResponse> PersistFiles(const DocumentFiles& files) {
    const auto result = store->SaveFiles(files);
    lastAppliedPersistenceSequence = ++persistenceSequence;
    if (result.Success()) {
      persistenceBlocked = false;
      return std::nullopt;
    }
    persistenceBlocked = true;
    std::vector<Json> details;
    if (result.projectError) {
      details.push_back(Json::object("path", files.project.string(), "message",
                                     *result.projectError));
    }
    for (const auto& [id, error] : result.trajectoryErrors) {
      details.push_back(Json::object("path", files.trajectories.at(id).string(),
                                     "message", error));
    }
    return JsonResponse(
        500, Json::object("code", "SAVE_FAILED", "message",
                          "One or more files could not be saved", "details",
                          std::move(details)));
  }

  void PersistFilesIfNamedAsync() {
    if (!store || !hasProjectPath.load(std::memory_order_acquire)) {
      return;
    }
    auto document = store->Snapshot();
    auto files = documentFiles;
    const auto revision = store->Revision();
    const auto projectId = document->project.uuid;
    const auto session = sessionGeneration;
    const auto sequence = ++persistenceSequence;
    auto self = shared_from_this();
    if (!PostPersistence(
            [self, document = std::move(document), files = std::move(files),
             revision, projectId, session, sequence]() mutable {
              SerializedDocument serialized;
              SaveResult result;
              try {
                serialized = SerializeDocument(*document);
                result = SaveSerializedFiles(files, serialized);
              } catch (const std::exception& error) {
                result.projectError = error.what();
              }
              self->Post([self, serialized = std::move(serialized),
                          result = std::move(result), files = std::move(files),
                          revision, projectId, session, sequence]() mutable {
                if (!self->store || self->sessionGeneration != session ||
                    sequence < self->lastAppliedPersistenceSequence) {
                  return;
                }
                if (!serialized.project.empty()) {
                  self->store->MarkSaved(std::move(serialized), result);
                }
                self->lastAppliedPersistenceSequence = sequence;
                self->persistenceBlocked = !result.Success();
                Json event = Json::object(
                    "type", "session.persistence", "projectId", projectId,
                    "revision", revision, "success", result.Success());
                if (!result.Success()) {
                  std::vector<Json> details;
                  if (result.projectError) {
                    details.push_back(Json::object(
                        "path", files.project.string(), "message",
                        *result.projectError));
                  }
                  for (const auto& [id, error] : result.trajectoryErrors) {
                    details.push_back(Json::object(
                        "path", files.trajectories.at(id).string(), "message",
                        error));
                  }
                  event["error"] = Json::object(
                      "code", "SAVE_FAILED", "message",
                      "One or more files could not be saved", "details",
                      std::move(details));
                }
                self->server.BroadcastText("/api/v1/events", event.to_string());
              });
            })) {
      persistenceBlocked = true;
    }
  }

  ApiResponse CreateTrajectory(const choreo::api::HttpRequest& request) {
    const auto expectedRevision = RevisionFromTag(request.Header("if-match"));
    const auto idempotencyKey = std::string{request.Header("idempotency-key")};
    const auto contentType = Lower(std::string{request.Header("content-type")});
    if (!expectedRevision || idempotencyKey.empty() ||
        !(contentType == "application/json" ||
          contentType.starts_with("application/json;"))) {
      return ApiError(
          400, "VALIDATION_ERROR",
          "If-Match, Idempotency-Key, and application/json are required");
    }

    const auto projectId = request.pathParameters.at("projectId");
    const auto fingerprint = "create-trajectory/" + request.path + "/" +
                             std::to_string(*expectedRevision) + "/" +
                             request.body;
    const auto body = request.body;
    auto self = shared_from_this();
    auto result = std::make_shared<std::promise<ApiResponse>>();
    auto response = result->get_future();
    if (!Post([self, expectedRevision = *expectedRevision, idempotencyKey,
               fingerprint, projectId, body, result] {
          try {
            const auto cached = self->idempotentMutations.find(idempotencyKey);
            if (cached != self->idempotentMutations.end()) {
              if (cached->second.requestTarget != fingerprint) {
                result->set_value(
                    ApiError(409, "IDEMPOTENCY_KEY_REUSE",
                             "Idempotency key was reused for another request"));
              } else {
                result->set_value(cached->second.response);
              }
              return;
            }
            if (!self->store) {
              result->set_value(
                  ApiError(409, "SESSION_NOT_OPEN", "No project is open"));
              return;
            }
            if (self->persistenceBlocked) {
              result->set_value(ApiError(
                  409, "SAVE_FAILED",
                  "Save the active session before changing the document"));
              return;
            }
            const auto snapshot = self->store->Snapshot();
            if (snapshot->project.uuid != projectId) {
              result->set_value(
                  ApiError(404, "RESOURCE_NOT_FOUND", "Project was not found"));
              return;
            }
            if (self->store->Revision() != expectedRevision) {
              result->set_value(JsonResponse(
                  409,
                  Json::object("code", "REVISION_CONFLICT", "message",
                               "Document revision is stale", "currentRevision",
                               self->store->Revision())));
              return;
            }

            Trajectory trajectory;
            try {
              trajectory = Json::parse_or_throw(body).get<Trajectory>();
            } catch (const std::exception& error) {
              result->set_value(ApiError(400, "VALIDATION_ERROR", error.what()));
              return;
            }
            const auto trajectoryId = trajectory.uuid;
            if (trajectoryId.empty() ||
                snapshot->trajectories.find(trajectoryId)) {
              result->set_value(ApiError(
                  409, "RESOURCE_EXISTS",
                  "A trajectory with that UUID already exists"));
              return;
            }

            auto candidate = *snapshot;
            candidate.trajectories = candidate.trajectories.set(
                trajectoryId, std::move(trajectory));
            const auto previousRevision = self->store->Revision();
            self->store->Commit(std::move(candidate), expectedRevision);
            if (self->store->Revision() != previousRevision) {
              if (self->hasProjectPath.load(std::memory_order_acquire)) {
                self->documentFiles.trajectories.emplace(
                    trajectoryId,
                    self->documentFiles.project.parent_path() /
                        (trajectoryId + ".traj"));
              }
              self->PersistFilesIfNamedAsync();
              self->PublishSnapshot(true, idempotencyKey);
            }

            ApiResponse mutation{
                201,
                "application/json",
                Json::object("requestId", idempotencyKey, "revision",
                             self->store->Revision(), "resource",
                             Json::object("type", "trajectory", "id",
                                          trajectoryId))
                    .to_string(),
                {}};
            mutation.headers.emplace(
                "ETag",
                "\"revision-" + std::to_string(self->store->Revision()) +
                    "\"");
            self->idempotentMutations.emplace(
                idempotencyKey, CachedMutation{fingerprint, mutation});
            result->set_value(std::move(mutation));
          } catch (const std::exception& error) {
            result->set_value(ApiError(422, "VALIDATION_ERROR", error.what()));
          } catch (...) {
            result->set_value(
                ApiError(500, "INTERNAL_ERROR", "Trajectory creation failed"));
          }
        })) {
      return ApiError(503, "INTERNAL_ERROR", "API is stopping");
    }
    return response.get();
  }

  ApiResponse PatchResource(const choreo::api::HttpRequest& request,
                            bool projectResource) {
    const auto expectedRevision = RevisionFromTag(request.Header("if-match"));
    const auto idempotencyKey = std::string{request.Header("idempotency-key")};
    const auto contentType = Lower(std::string{request.Header("content-type")});
    constexpr std::string_view mergePatchContentType =
        "application/merge-patch+json";
    if (!expectedRevision || idempotencyKey.empty() ||
        !(contentType == mergePatchContentType ||
          contentType.starts_with(std::string{mergePatchContentType} + ";"))) {
      return ApiError(
          400, "VALIDATION_ERROR",
          "If-Match, Idempotency-Key, and merge-patch JSON are required");
    }

    const auto projectId = request.pathParameters.at("projectId");
    const auto trajectoryId = projectResource
                                  ? std::string{}
                                  : request.pathParameters.at("trajectoryId");
    const auto operation = projectResource ? std::string{"patch-project"}
                                           : std::string{"patch-trajectory"};
    const auto fingerprint = operation + "/" + request.path + "/" +
                             std::to_string(*expectedRevision) + "/" +
                             request.body;
    std::string body = request.body;
    auto self = shared_from_this();
    auto result = std::make_shared<std::promise<ApiResponse>>();
    auto response = result->get_future();
    if (!Post([self, expectedRevision = *expectedRevision, idempotencyKey,
               fingerprint, projectId, trajectoryId, projectResource,
               body = std::move(body), result] {
          try {
            const auto cached = self->idempotentMutations.find(idempotencyKey);
            if (cached != self->idempotentMutations.end()) {
              if (cached->second.requestTarget != fingerprint) {
                result->set_value(
                    ApiError(409, "IDEMPOTENCY_KEY_REUSE",
                             "Idempotency key was reused for another request"));
              } else {
                result->set_value(cached->second.response);
              }
              return;
            }

            if (!self->store) {
              result->set_value(
                  ApiError(409, "SESSION_NOT_OPEN", "No project is open"));
              return;
            }
            if (self->persistenceBlocked) {
              result->set_value(ApiError(
                  409, "SAVE_FAILED",
                  "Save the active session before changing the document"));
              return;
            }
            const auto snapshot = self->store->Snapshot();
            if (snapshot->project.uuid != projectId) {
              result->set_value(
                  ApiError(404, "RESOURCE_NOT_FOUND", "Project was not found"));
              return;
            }
            if (self->store->Revision() != expectedRevision) {
              result->set_value(JsonResponse(
                  409,
                  Json::object("code", "REVISION_CONFLICT", "message",
                               "Document revision is stale", "currentRevision",
                               self->store->Revision())));
              return;
            }

            Json patch;
            try {
              patch = Json::parse_or_throw(body);
            } catch (const std::exception& error) {
              result->set_value(
                  ApiError(400, "VALIDATION_ERROR", error.what()));
              return;
            }

            Document candidate = *snapshot;
            if (projectResource) {
              if (!HasOnlyKeys(patch, {"name", "type", "variables", "config",
                                       "codegen"})) {
                result->set_value(ApiError(
                    400, "VALIDATION_ERROR",
                    "Project patch must contain editable project fields"));
                return;
              }
              Json updated = snapshot->project;
              ApplyMergePatch(updated, patch);
              candidate.project = updated.get<Project>();
            } else {
              const auto* original = snapshot->trajectories.find(trajectoryId);
              if (!original) {
                result->set_value(ApiError(404, "RESOURCE_NOT_FOUND",
                                           "Trajectory was not found"));
                return;
              }
              if (!HasOnlyKeys(patch, {"name", "params", "events"})) {
                result->set_value(ApiError(400, "VALIDATION_ERROR",
                                           "Trajectory patch must contain "
                                           "editable trajectory fields"));
                return;
              }
              Json updated = *original;
              ApplyMergePatch(updated, patch);
              auto trajectory = updated.get<Trajectory>();
              if (trajectory.uuid != trajectoryId) {
                result->set_value(
                    ApiError(400, "VALIDATION_ERROR",
                             "Trajectory UUID cannot be changed"));
                return;
              }
              candidate.trajectories = candidate.trajectories.set(
                  trajectoryId, std::move(trajectory));
            }

            const auto previousRevision = self->store->Revision();
            self->store->Commit(std::move(candidate), expectedRevision);
            if (self->store->Revision() != previousRevision) {
              self->PersistFilesIfNamedAsync();
              self->PublishSnapshot(true, idempotencyKey);
            }

            ApiResponse mutation{
                200,
                "application/json",
                Json::object("requestId", idempotencyKey, "revision",
                             self->store->Revision())
                    .to_string(),
                {}};
            mutation.headers.emplace(
                "ETag",
                "\"revision-" + std::to_string(self->store->Revision()) + "\"");
            self->idempotentMutations.emplace(
                idempotencyKey, CachedMutation{fingerprint, mutation});
            result->set_value(std::move(mutation));
          } catch (const std::exception& error) {
            result->set_value(ApiError(422, "VALIDATION_ERROR", error.what()));
          } catch (...) {
            result->set_value(
                ApiError(500, "INTERNAL_ERROR", "Document update failed"));
          }
        })) {
      return ApiError(503, "INTERNAL_ERROR", "API is stopping");
    }
    return response.get();
  }

  ApiResponse PatchVariable(const choreo::api::HttpRequest& request) {
    const auto expectedRevision = RevisionFromTag(request.Header("if-match"));
    const auto idempotencyKey = std::string{request.Header("idempotency-key")};
    const auto contentType = Lower(std::string{request.Header("content-type")});
    constexpr std::string_view mergePatchContentType =
        "application/merge-patch+json";
    if (!expectedRevision || idempotencyKey.empty() ||
        !(contentType == mergePatchContentType ||
          contentType.starts_with(std::string{mergePatchContentType} + ";"))) {
      return ApiError(
          400, "VALIDATION_ERROR",
          "If-Match, Idempotency-Key, and merge-patch JSON are required");
    }

    const auto projectId = request.pathParameters.at("projectId");
    const auto variableId = request.pathParameters.at("variableId");
    const auto fingerprint = "patch-variable/" + request.path + "/" +
                             std::to_string(*expectedRevision) + "/" +
                             request.body;
    const auto body = request.body;
    auto self = shared_from_this();
    auto result = std::make_shared<std::promise<ApiResponse>>();
    auto response = result->get_future();
    if (!Post([self, expectedRevision = *expectedRevision, idempotencyKey,
               fingerprint, projectId, variableId, body, result] {
          try {
            const auto cached = self->idempotentMutations.find(idempotencyKey);
            if (cached != self->idempotentMutations.end()) {
              if (cached->second.requestTarget != fingerprint) {
                result->set_value(
                    ApiError(409, "IDEMPOTENCY_KEY_REUSE",
                             "Idempotency key was reused for another request"));
              } else {
                result->set_value(cached->second.response);
              }
              return;
            }
            if (!self->store) {
              result->set_value(
                  ApiError(409, "SESSION_NOT_OPEN", "No project is open"));
              return;
            }
            if (self->persistenceBlocked) {
              result->set_value(ApiError(
                  409, "SAVE_FAILED",
                  "Save the active session before changing the document"));
              return;
            }
            const auto snapshot = self->store->Snapshot();
            if (snapshot->project.uuid != projectId) {
              result->set_value(
                  ApiError(404, "RESOURCE_NOT_FOUND", "Project was not found"));
              return;
            }
            if (self->store->Revision() != expectedRevision) {
              result->set_value(JsonResponse(
                  409,
                  Json::object("code", "REVISION_CONFLICT", "message",
                               "Document revision is stale", "currentRevision",
                               self->store->Revision())));
              return;
            }
            if (!snapshot->project.variables.contains(variableId)) {
              result->set_value(ApiError(404, "RESOURCE_NOT_FOUND",
                                         "Variable was not found"));
              return;
            }

            Json patch;
            std::optional<std::string> name;
            std::optional<std::string> expression;
            try {
              patch = Json::parse_or_throw(body);
              if (!HasOnlyKeys(patch, {"name", "expression"})) {
                result->set_value(
                    ApiError(400, "VALIDATION_ERROR",
                             "Variable patch must contain name or expression"));
                return;
              }
              if (patch.contains("name")) {
                name = patch.at("name").get_string();
              }
              if (patch.contains("expression")) {
                expression = patch.at("expression").get_string();
              }
            } catch (const std::exception& error) {
              result->set_value(
                  ApiError(400, "VALIDATION_ERROR", error.what()));
              return;
            }

            const auto previousRevision = self->store->Revision();
            self->store->UpdateVariable(variableId, name, expression);
            if (self->store->Revision() != previousRevision) {
              self->PersistFilesIfNamedAsync();
              self->PublishSnapshot(true, idempotencyKey);
            }
            ApiResponse mutation{
                200,
                "application/json",
                Json::object("requestId", idempotencyKey, "revision",
                             self->store->Revision())
                    .to_string(),
                {}};
            mutation.headers.emplace(
                "ETag",
                "\"revision-" + std::to_string(self->store->Revision()) + "\"");
            self->idempotentMutations.emplace(
                idempotencyKey, CachedMutation{fingerprint, mutation});
            result->set_value(std::move(mutation));
          } catch (const std::exception& error) {
            result->set_value(ApiError(422, "VALIDATION_ERROR", error.what()));
          } catch (...) {
            result->set_value(
                ApiError(500, "INTERNAL_ERROR", "Variable update failed"));
          }
        })) {
      return ApiError(503, "INTERNAL_ERROR", "API is stopping");
    }
    return response.get();
  }

  ApiResponse GeometryControls(const choreo::api::HttpRequest& request) const {
    try {
      const auto published = publishedSnapshot.load();
      if (!published) {
        return ApiError(409, "SESSION_NOT_OPEN", "No project is open");
      }
      if (published->projectId != request.pathParameters.at("projectId")) {
        return ApiError(404, "RESOURCE_NOT_FOUND", "Project was not found");
      }
      const auto body = Json::parse_or_throw(request.body);
      if (!HasOnlyKeys(body, {"expression", "type"}) ||
          body.at("expression").get_string().empty()) {
        return ApiError(400, "VALIDATION_ERROR",
                        "A geometry expression and type are required");
      }
      const auto expression = body.at("expression").get_string();
      const auto typeName = body.at("type").get_string();
      evaluation::DragGeometryType type;
      if (typeName == "Pose2d") {
        type = evaluation::DragGeometryType::Pose2d;
      } else if (typeName == "Translation2d") {
        type = evaluation::DragGeometryType::Translation2d;
      } else {
        return ApiError(400, "VALIDATION_ERROR",
                        "Unsupported geometry type for field handles");
      }

      const auto document = Json::parse_or_throw(published->body);
      const auto project = document.at("project").get<Project>();
      const auto controls =
          evaluation::GetGeometryDragControls(project, expression, type);
      std::vector<Json> serializedControls;
      serializedControls.reserve(controls.size());
      for (const auto& control : controls) {
        serializedControls.push_back(Json::object(
            "start", control.start, "end", control.end, "value",
            control.value, "dx", control.dx, "dy", control.dy, "dheading",
            control.dheading));
      }
      return JsonResponse(200, Json::object("controls",
                                             std::move(serializedControls)));
    } catch (const std::exception& error) {
      return ApiError(422, "VALIDATION_ERROR", error.what());
    }
  }

  ApiResponse MutateTrajectorySubresource(
      const choreo::api::HttpRequest& request, std::string_view field,
      std::string_view itemParameter, std::string_view action) {
    const auto expectedRevision = RevisionFromTag(request.Header("if-match"));
    const auto idempotencyKey = std::string{request.Header("idempotency-key")};
    const auto contentType = Lower(std::string{request.Header("content-type")});
    constexpr std::string_view mergePatchContentType =
        "application/merge-patch+json";
    if (!expectedRevision || idempotencyKey.empty() ||
        (action == "patch" &&
         !(contentType == mergePatchContentType ||
           contentType.starts_with(std::string{mergePatchContentType} +
                                   ";"))) ||
        (action == "create" && contentType != "application/json" &&
         !contentType.starts_with("application/json;"))) {
      return ApiError(400, "VALIDATION_ERROR",
                      "If-Match, Idempotency-Key, and the expected JSON "
                      "content type are required");
    }

    const auto projectId = request.pathParameters.at("projectId");
    const auto trajectoryId = request.pathParameters.at("trajectoryId");
    const auto itemId =
        itemParameter.empty()
            ? std::string{}
            : request.pathParameters.at(std::string{itemParameter});
    const auto fingerprint = std::string{action} + "/" + request.path + "/" +
                             std::to_string(*expectedRevision) + "/" +
                             request.body;
    const auto body = request.body;
    auto self = shared_from_this();
    auto result = std::make_shared<std::promise<ApiResponse>>();
    auto response = result->get_future();
    if (!Post([self, expectedRevision = *expectedRevision, idempotencyKey,
               fingerprint, projectId, trajectoryId, itemId,
               field = std::string{field}, action = std::string{action}, body,
               result] {
          try {
            const auto cached = self->idempotentMutations.find(idempotencyKey);
            if (cached != self->idempotentMutations.end()) {
              if (cached->second.requestTarget != fingerprint) {
                result->set_value(
                    ApiError(409, "IDEMPOTENCY_KEY_REUSE",
                             "Idempotency key was reused for another request"));
              } else {
                result->set_value(cached->second.response);
              }
              return;
            }
            if (!self->store) {
              result->set_value(
                  ApiError(409, "SESSION_NOT_OPEN", "No project is open"));
              return;
            }
            if (self->persistenceBlocked) {
              result->set_value(ApiError(
                  409, "SAVE_FAILED",
                  "Save the active session before changing the document"));
              return;
            }
            const auto snapshot = self->store->Snapshot();
            if (snapshot->project.uuid != projectId) {
              result->set_value(
                  ApiError(404, "RESOURCE_NOT_FOUND", "Project was not found"));
              return;
            }
            if (self->store->Revision() != expectedRevision) {
              result->set_value(JsonResponse(
                  409,
                  Json::object("code", "REVISION_CONFLICT", "message",
                               "Document revision is stale", "currentRevision",
                               self->store->Revision())));
              return;
            }
            const auto* original = snapshot->trajectories.find(trajectoryId);
            if (!original) {
              result->set_value(ApiError(404, "RESOURCE_NOT_FOUND",
                                         "Trajectory was not found"));
              return;
            }

            Json item;
            Json patch;
            if (action == "create" || action == "patch") {
              try {
                item = Json::parse_or_throw(body);
              } catch (const std::exception& error) {
                result->set_value(
                    ApiError(400, "VALIDATION_ERROR", error.what()));
                return;
              }
            }
            if (action == "patch") {
              if (field == "waypoints" &&
                  !HasOnlyKeys(item,
                               {"pose", "intervals", "split", "fixTranslation",
                                "fixHeading", "overrideIntervals"})) {
                result->set_value(ApiError(400, "VALIDATION_ERROR",
                                           "Invalid waypoint patch fields"));
                return;
              }
              if (field == "constraints" &&
                  !HasOnlyKeys(item, {"from", "to", "data", "enabled"})) {
                result->set_value(ApiError(400, "VALIDATION_ERROR",
                                           "Invalid constraint patch fields"));
                return;
              }
              if (field == "events" && !HasOnlyKeys(item, {"name", "from"})) {
                result->set_value(
                    ApiError(400, "VALIDATION_ERROR",
                             "Invalid event-marker patch fields"));
                return;
              }
              patch = std::move(item);
            } else if (action == "create" && !item.is_object()) {
              result->set_value(
                  ApiError(400, "VALIDATION_ERROR",
                           "Created resource must be a JSON object"));
              return;
            }

            Json updated = *original;
            Json items = field == "events" ? updated.at("events")
                                           : updated.at("params").at(field);
            auto& itemList = items.get_array();
            if (action == "create") {
              std::string newId;
              try {
                newId = item.at("uuid").get_string();
              } catch (const std::exception& error) {
                result->set_value(
                    ApiError(400, "VALIDATION_ERROR", error.what()));
                return;
              }
              const auto duplicate = std::find_if(
                  itemList.begin(), itemList.end(),
                  [&newId](const Json& existing) {
                    return existing.at("uuid").get_string() == newId;
                  });
              if (duplicate != itemList.end()) {
                result->set_value(
                    ApiError(409, "RESOURCE_EXISTS",
                             "A resource with that UUID already exists"));
                return;
              }
              itemList.emplace_back(std::move(item));
            } else {
              const auto existing =
                  std::find_if(itemList.begin(), itemList.end(),
                               [&itemId](const Json& value) {
                                 return value.at("uuid").get_string() == itemId;
                               });
              if (existing == itemList.end()) {
                result->set_value(
                    ApiError(404, "RESOURCE_NOT_FOUND", "Item was not found"));
                return;
              }
              if (action == "patch") {
                ApplyMergePatch(*existing, patch);
                if (existing->at("uuid").get_string() != itemId) {
                  result->set_value(ApiError(400, "VALIDATION_ERROR",
                                             "Resource UUID cannot change"));
                  return;
                }
              } else {
                itemList.erase(existing);
              }
            }
            if (field == "events") {
              updated["events"] = std::move(items);
            } else {
              updated["params"][field] = std::move(items);
            }
            auto trajectory = updated.get<Trajectory>();
            auto candidate = *snapshot;
            candidate.trajectories =
                candidate.trajectories.set(trajectoryId, std::move(trajectory));
            const auto previousRevision = self->store->Revision();
            self->store->Commit(std::move(candidate), expectedRevision);
            if (self->store->Revision() != previousRevision) {
              self->PersistFilesIfNamedAsync();
              self->PublishSnapshot(true, idempotencyKey);
            }

            const int status = action == "create" ? 201 : 200;
            ApiResponse mutation{
                status,
                "application/json",
                Json::object("requestId", idempotencyKey, "revision",
                             self->store->Revision())
                    .to_string(),
                {}};
            mutation.headers.emplace(
                "ETag",
                "\"revision-" + std::to_string(self->store->Revision()) + "\"");
            self->idempotentMutations.emplace(
                idempotencyKey, CachedMutation{fingerprint, mutation});
            result->set_value(std::move(mutation));
          } catch (const std::exception& error) {
            result->set_value(ApiError(422, "VALIDATION_ERROR", error.what()));
          } catch (...) {
            result->set_value(
                ApiError(500, "INTERNAL_ERROR", "Resource update failed"));
          }
        })) {
      return ApiError(503, "INTERNAL_ERROR", "API is stopping");
    }
    return response.get();
  }

  ApiResponse HistoryAction(const choreo::api::HttpRequest& request,
                            bool undo) {
    const auto expectedRevision = RevisionFromTag(request.Header("if-match"));
    const auto idempotencyKey = std::string{request.Header("idempotency-key")};
    if (!expectedRevision || idempotencyKey.empty()) {
      return ApiError(400, "VALIDATION_ERROR",
                      "If-Match and Idempotency-Key are required");
    }

    const auto operation = undo ? std::string{"undo"} : std::string{"redo"};
    const auto fingerprint =
        operation + "/" + std::to_string(*expectedRevision);
    auto self = shared_from_this();
    auto result = std::make_shared<std::promise<ApiResponse>>();
    auto response = result->get_future();
    if (!Post([self, expectedRevision = *expectedRevision, idempotencyKey,
               fingerprint, undo, result] {
          try {
            const auto cached = self->idempotentMutations.find(idempotencyKey);
            if (cached != self->idempotentMutations.end()) {
              if (cached->second.requestTarget != fingerprint) {
                result->set_value(
                    ApiError(409, "IDEMPOTENCY_KEY_REUSE",
                             "Idempotency key was reused for another request"));
              } else {
                result->set_value(cached->second.response);
              }
              return;
            }

            if (!self->store) {
              result->set_value(
                  ApiError(409, "SESSION_NOT_OPEN", "No project is open"));
              return;
            }
            if (self->persistenceBlocked) {
              result->set_value(ApiError(
                  409, "SAVE_FAILED",
                  "Save the active session before changing document history"));
              return;
            }
            if (self->store->Revision() != expectedRevision) {
              result->set_value(JsonResponse(
                  409,
                  Json::object("code", "REVISION_CONFLICT", "message",
                               "Document revision is stale", "currentRevision",
                               self->store->Revision())));
              return;
            }

            const bool changed =
                undo ? self->store->Undo() : self->store->Redo();
            if (!changed) {
              result->set_value(
                  ApiError(409, "VALIDATION_ERROR",
                           undo ? "No document change is available to undo"
                                : "No document change is available to redo"));
              return;
            }

            self->PersistFilesIfNamedAsync();
            self->PublishSnapshot(true, idempotencyKey);
            ApiResponse mutation{
                200,
                "application/json",
                Json::object("requestId", idempotencyKey, "revision",
                             self->store->Revision())
                    .to_string(),
                {}};
            mutation.headers.emplace(
                "ETag",
                "\"revision-" + std::to_string(self->store->Revision()) + "\"");
            self->idempotentMutations.emplace(
                idempotencyKey, CachedMutation{fingerprint, mutation});
            result->set_value(std::move(mutation));
          } catch (const std::exception& error) {
            result->set_value(ApiError(500, "INTERNAL_ERROR", error.what()));
          } catch (...) {
            result->set_value(
                ApiError(500, "INTERNAL_ERROR", "History operation failed"));
          }
        })) {
      return ApiError(503, "INTERNAL_ERROR", "API is stopping");
    }
    return response.get();
  }

  ApiResponse StartGeneration(const choreo::api::HttpRequest& request) {
    const auto projectId = request.pathParameters.at("projectId");
    const auto trajectoryId = request.pathParameters.at("trajectoryId");
    const auto idempotencyKey = std::string{request.Header("idempotency-key")};
    const auto expectedRevision = RevisionFromTag(request.Header("if-match"));
    if (!expectedRevision || idempotencyKey.empty()) {
      return ApiError(400, "VALIDATION_ERROR",
                      "If-Match and Idempotency-Key are required");
    }
    if (options.generatorExecutable.empty()) {
      return ApiError(503, "GENERATOR_UNAVAILABLE",
                      "No generator executable is configured");
    }
    if (persistenceBlocked) {
      return ApiError(409, "SAVE_FAILED",
                      "Save the active session before starting generation");
    }

    const auto fingerprint = projectId + "/" + trajectoryId + "/" +
                             std::to_string(*expectedRevision) + "/" +
                             request.body;
    auto cached = idempotentGenerations.find(idempotencyKey);
    if (cached != idempotentGenerations.end()) {
      if (cached->second.requestTarget != fingerprint) {
        return ApiError(409, "IDEMPOTENCY_KEY_REUSE",
                        "Idempotency key was reused for another request");
      }
      return cached->second.response;
    }

    const auto operationId = NewUuid();
    const auto acceptedRevision = *expectedRevision;
    const auto response =
        JsonResponse(202, Json::object("operationId", operationId,
                                       "acceptedRevision", acceptedRevision));
    idempotentGenerations.emplace(idempotencyKey,
                                  CachedGeneration{fingerprint, response});
    EmitStatus(operationId, trajectoryId, "queued", "Generation queued",
               acceptedRevision);

    auto self = shared_from_this();
    auto start = [self, projectId, trajectoryId, operationId,
                  expectedRevision = *expectedRevision] {
      auto fail = [&](std::string_view message) {
        self->EmitStatus(operationId, trajectoryId, "failed",
                         "Generation could not start", expectedRevision,
                         message);
      };
      if (!self->store) {
        fail("No project is open");
        return;
      }
      auto snapshot = self->store->Snapshot();
      if (snapshot->project.uuid != projectId ||
          !snapshot->trajectories.find(trajectoryId)) {
        fail("Trajectory was not found");
        return;
      }
      if (self->store->Revision() != expectedRevision) {
        fail("Document revision is stale");
        return;
      }
      try {
        auto progressUrl =
            "ws://127.0.0.1:" + std::to_string(self->server.Port()) +
            "/api/v1/generator-progress?operationId=" + operationId;
        auto process = std::make_unique<GenerationProcess>(
            self->options.generatorExecutable, *self->store, trajectoryId,
            progressUrl);
        self->generations.emplace(
            operationId,
            Generation{operationId, trajectoryId, std::move(process), false});
        self->EmitStatus(operationId, trajectoryId, "running",
                         "Generation started", expectedRevision);
      } catch (const std::exception& error) {
        fail(error.what());
      }
    };
    if (!Post(std::move(start))) {
      idempotentGenerations.erase(idempotencyKey);
      return ApiError(503, "INTERNAL_ERROR", "API is stopping");
    }
    return response;
  }

  ApiResponse CancelGeneration(const choreo::api::HttpRequest& request) {
    const auto projectId = request.pathParameters.at("projectId");
    const auto trajectoryId = request.pathParameters.at("trajectoryId");
    const auto operationId = request.pathParameters.at("operationId");
    auto self = shared_from_this();
    if (!Post([self, projectId, trajectoryId, operationId] {
          if (!self->store ||
              self->store->Snapshot()->project.uuid != projectId) {
            self->EmitStatus(operationId, trajectoryId, "failed",
                             "Cancellation failed", 0, "Project was not found");
            return;
          }
          auto operation = self->generations.find(operationId);
          if (operation == self->generations.end() ||
              operation->second.trajectoryId != trajectoryId) {
            self->EmitStatus(operationId, trajectoryId, "failed",
                             "Cancellation failed", self->store->Revision(),
                             "Operation was not found");
            return;
          }
          operation->second.cancelRequested = true;
          operation->second.process->Cancel();
          self->EmitStatus(operationId, trajectoryId, "cancelling",
                           "Cancellation requested", self->store->Revision());
        })) {
      return ApiError(503, "INTERNAL_ERROR", "API is stopping");
    }
    return JsonResponse(202, Json::object("operationId", operationId, "status",
                                          "cancellation-requested"));
  }

  ApiResponse SnapshotResponse() const {
    auto snapshot = publishedSnapshot.load();
    if (!snapshot) {
      return ApiError(409, "SESSION_NOT_OPEN", "No project is open");
    }
    ApiResponse response{200, "application/json", snapshot->body, {}};
    response.headers.emplace(
        "ETag", "\"revision-" + std::to_string(snapshot->revision) + "\"");
    return response;
  }

  ApiResponse SessionStatusResponse() {
    auto self = shared_from_this();
    auto result = std::make_shared<std::promise<ApiResponse>>();
    auto response = result->get_future();
    if (!Post([self, result] {
          if (!self->store) {
            result->set_value(JsonResponse(
                200, Json::object("open", false, "projectId", nullptr,
                                  "revision", nullptr, "dirty", false,
                                  "persistenceBlocked", false)));
            return;
          }
          auto snapshot = self->store->Snapshot();
          bool dirty = self->store->IsProjectDirty();
          for (const auto& [id, trajectory] : snapshot->trajectories) {
            static_cast<void>(trajectory);
            dirty = dirty || self->store->IsTrajectoryDirty(id);
          }
          const auto published = self->publishedSnapshot.load();
          const auto revision = published ? published->revision : 0;
          result->set_value(JsonResponse(
              200, Json::object("open", true, "projectId",
                                snapshot->project.uuid, "revision", revision,
                                "dirty", dirty,
                                "persistenceBlocked",
                                self->persistenceBlocked)));
        })) {
      return ApiError(503, "INTERNAL_ERROR", "API is stopping");
    }
    return response.get();
  }

  ApiResponse TrajectoryResponse(
      const choreo::api::HttpRequest& request) const {
    auto snapshot = publishedSnapshot.load();
    if (!snapshot) {
      return ApiError(409, "SESSION_NOT_OPEN", "No project is open");
    }
    const auto projectId = request.pathParameters.at("projectId");
    const auto trajectoryId = request.pathParameters.at("trajectoryId");
    const auto snapshotJson = Json::parse_or_throw(snapshot->body);
    if (snapshotJson.at("project").at("uuid").get_string() != projectId) {
      return ApiError(404, "RESOURCE_NOT_FOUND", "Project was not found");
    }
    for (const auto& trajectory : snapshotJson.at("trajectories").get_array()) {
      if (trajectory.at("uuid").get_string() == trajectoryId) {
        ApiResponse response{
            200, "application/json", trajectory.to_string(), {}};
        response.headers.emplace(
            "ETag", "\"revision-" + std::to_string(snapshot->revision) + "\"");
        return response;
      }
    }
    return ApiError(404, "RESOURCE_NOT_FOUND", "Trajectory was not found");
  }

  ApiResponse TrajectorySubresourceResponse(
      const choreo::api::HttpRequest& request, std::string_view field,
      const std::optional<std::string>& itemId) const {
    auto snapshot = publishedSnapshot.load();
    if (!snapshot) {
      return ApiError(409, "SESSION_NOT_OPEN", "No project is open");
    }
    const auto projectId = request.pathParameters.at("projectId");
    const auto trajectoryId = request.pathParameters.at("trajectoryId");
    const auto snapshotJson = Json::parse_or_throw(snapshot->body);
    if (snapshotJson.at("project").at("uuid").get_string() != projectId) {
      return ApiError(404, "RESOURCE_NOT_FOUND", "Project was not found");
    }
    const Json* trajectory = nullptr;
    for (const auto& candidate : snapshotJson.at("trajectories").get_array()) {
      if (candidate.at("uuid").get_string() == trajectoryId) {
        trajectory = &candidate;
        break;
      }
    }
    if (trajectory == nullptr) {
      return ApiError(404, "RESOURCE_NOT_FOUND", "Trajectory was not found");
    }

    const auto& items = field == "events" ? trajectory->at("events")
                                          : trajectory->at("params").at(field);
    Json responseBody;
    int status = 200;
    if (itemId) {
      const auto item =
          std::find_if(items.get_array().begin(), items.get_array().end(),
                       [&itemId](const Json& value) {
                         return value.at("uuid").get_string() == *itemId;
                       });
      if (item == items.get_array().end()) {
        return ApiError(404, "RESOURCE_NOT_FOUND", "Item was not found");
      }
      responseBody = *item;
    } else {
      responseBody =
          Json::object("revision", snapshot->revision, "items", items);
    }
    ApiResponse response{
        status, "application/json", responseBody.to_string(), {}};
    response.headers.emplace(
        "ETag", "\"revision-" + std::to_string(snapshot->revision) + "\"");
    return response;
  }

  ApiResponse ProjectResponse(const choreo::api::HttpRequest& request) const {
    auto snapshot = publishedSnapshot.load();
    if (!snapshot) {
      return ApiError(409, "SESSION_NOT_OPEN", "No project is open");
    }
    const auto projectId = request.pathParameters.at("projectId");
    const auto snapshotJson = Json::parse_or_throw(snapshot->body);
    const auto& project = snapshotJson.at("project");
    if (project.at("uuid").get_string() != projectId) {
      return ApiError(404, "RESOURCE_NOT_FOUND", "Project was not found");
    }
    ApiResponse response{200, "application/json", project.to_string(), {}};
    response.headers.emplace(
        "ETag", "\"revision-" + std::to_string(snapshot->revision) + "\"");
    return response;
  }

  ApiResponse TrajectoriesResponse(
      const choreo::api::HttpRequest& request) const {
    auto snapshot = publishedSnapshot.load();
    if (!snapshot) {
      return ApiError(409, "SESSION_NOT_OPEN", "No project is open");
    }
    const auto projectId = request.pathParameters.at("projectId");
    const auto snapshotJson = Json::parse_or_throw(snapshot->body);
    if (snapshotJson.at("project").at("uuid").get_string() != projectId) {
      return ApiError(404, "RESOURCE_NOT_FOUND", "Project was not found");
    }
    auto items = snapshotJson.at("trajectories");
    ApiResponse response{
        200,
        "application/json",
        Json::object("revision", snapshot->revision, "items", std::move(items))
            .to_string(),
        {}};
    response.headers.emplace(
        "ETag", "\"revision-" + std::to_string(snapshot->revision) + "\"");
    return response;
  }

  Json SnapshotJson() const {
    auto snapshot = store->Snapshot();
    std::vector<Json> trajectories;
    for (const auto& [id, trajectory] : snapshot->trajectories) {
      trajectories.push_back(trajectory);
    }
    return Json::object("revision", store->Revision(), "project",
                        snapshot->project, "trajectories",
                        std::move(trajectories));
  }

  void PublishSnapshot(bool notify = true,
                       std::optional<std::string_view> requestId = std::nullopt,
                       std::optional<std::string_view> operationId =
                           std::nullopt) {
    if (!store) {
      auto previous = publishedSnapshot.exchange({});
      if (previous && notify) {
        server.BroadcastText(
            "/api/v1/events",
            Json::object("type", "session.closed", "projectId",
                         previous->projectId, "revision", previous->revision)
                .to_string());
      }
      return;
    }
    auto document = SnapshotJson();
    const auto revision = store->Revision();
    const auto projectId =
        document.at("project").at("uuid").get_string();
    auto body = document.to_string();
    const auto previous = publishedSnapshot.load();
    const bool changed = !previous || previous->body != body;
    auto snapshot = std::make_shared<const PublishedSnapshot>(
        PublishedSnapshot{revision, projectId, std::move(body)});
    publishedSnapshot.store(std::move(snapshot));
    if (changed && notify) {
      Json event =
          Json::object("type", "document.changed", "projectId", projectId,
                       "revision", revision);
      if (requestId) {
        event["requestId"] = *requestId;
      }
      if (operationId) {
        event["operationId"] = *operationId;
      }
      server.BroadcastText("/api/v1/events", event.to_string());
    }
  }

  DocumentFiles DiscoverFiles(const std::filesystem::path& projectPath) const {
    if (!std::filesystem::is_regular_file(projectPath)) {
      throw std::invalid_argument("project file does not exist");
    }
    DocumentFiles files;
    files.project = projectPath;
    std::error_code error;
    for (std::filesystem::directory_iterator
             it{projectPath.parent_path(), error},
         end;
         !error && it != end; it.increment(error)) {
      const auto path = it->path();
      if (!it->is_regular_file(error) || error ||
          Lower(path.extension().string()) != ".traj") {
        continue;
      }
      const auto trajectory =
          Json::parse_or_throw(ReadText(path)).get<Trajectory>();
      if (!files.trajectories.emplace(trajectory.uuid, path).second) {
        throw std::invalid_argument(
            "duplicate trajectory UUID in project folder");
      }
    }
    if (error) {
      throw std::runtime_error("cannot enumerate project directory: " +
                               error.message());
    }
    return files;
  }

  Document ReadDocument(const DocumentFiles& files) const {
    Document document;
    document.project =
        Json::parse_or_throw(ReadText(files.project)).get<Project>();
    for (const auto& [id, path] : files.trajectories) {
      auto trajectory = Json::parse_or_throw(ReadText(path)).get<Trajectory>();
      if (trajectory.uuid != id) {
        throw std::invalid_argument("trajectory ID changed while loading");
      }
      document.trajectories =
          document.trajectories.set(id, std::move(trajectory));
    }
    return document;
  }

  void ForwardGeneratorMessage(choreo::api::WebSocketId connection,
                               const std::string& message) {
    auto mapping = progressConnections.find(connection);
    if (mapping == progressConnections.end()) {
      return;
    }
    auto operation = generations.find(mapping->second);
    if (operation == generations.end()) {
      return;
    }
    try {
      auto payload = Json::parse_or_throw(message);
      const auto event = payload.at("event").get_string();
      if (event == "completeTrajectory") {
        return;
      }
      Json update = Json::object("type", "generation.progress", "operationId",
                                 operation->first, "trajectoryId",
                                 operation->second.trajectoryId, "message",
                                 event, "data", std::move(payload));
      server.BroadcastText("/api/v1/events", update.to_string());
    } catch (...) {
    }
  }

  void EmitStatus(const std::string& operationId,
                  const std::string& trajectoryId, std::string_view status,
                  std::string_view message, std::uint64_t revision,
                  std::string_view error = {}) {
    Json event = Json::object("type", "generation.finished", "operationId",
                              operationId, "trajectoryId", trajectoryId,
                              "status", status, "revision", revision);
    if (!error.empty()) {
      event["error"] =
          Json::object("code", "GENERATION_FAILED", "message", error);
    }
    if (status == "queued" || status == "running" || status == "cancelling") {
      event = Json::object("type", "generation.status", "operationId",
                           operationId, "trajectoryId", trajectoryId, "status",
                           status, "message", message, "revision", revision);
    }
    server.BroadcastText("/api/v1/events", event.to_string());
  }

  void PollGenerations() {
    for (auto it = generations.begin(); it != generations.end();) {
      auto& generation = it->second;
      try {
        if (!generation.process->Poll()) {
          ++it;
          continue;
        }
        if (generation.cancelRequested) {
          EmitStatus(generation.operationId, generation.trajectoryId,
                     "cancelled", "Generation cancelled", store->Revision());
        } else {
          try {
            generation.process->CommitResult(*store);
            PersistFilesIfNamedAsync();
            PublishSnapshot(true, std::nullopt, generation.operationId);
            EmitStatus(generation.operationId, generation.trajectoryId,
                       "succeeded", "Generation completed", store->Revision());
          } catch (const std::exception& error) {
            EmitStatus(generation.operationId, generation.trajectoryId,
                       "failed", "Generation failed", store->Revision(),
                       error.what());
          }
        }
      } catch (const std::exception& error) {
        EmitStatus(generation.operationId, generation.trajectoryId,
                   generation.cancelRequested ? "cancelled" : "failed",
                   "Generation failed", store ? store->Revision() : 0,
                   error.what());
      }
      it = generations.erase(it);
    }
  }

  void StopGenerations() {
    for (auto& [id, generation] : generations) {
      generation.cancelRequested = true;
      generation.process->Cancel();
      generation.process->Wait();
      EmitStatus(id, generation.trajectoryId, "cancelled",
                 "Generation cancelled because the session changed",
                 store ? store->Revision() : 0);
    }
    generations.clear();
    progressConnections.clear();
  }

  void RunWorker() {
    while (true) {
      Task task;
      {
        std::unique_lock lock{queueMutex};
        queueCondition.wait_for(lock, std::chrono::milliseconds{25},
                                [&] { return stopping || !tasks.empty(); });
        if (!tasks.empty()) {
          task = std::move(tasks.front());
          tasks.pop_front();
        } else if (stopping) {
          break;
        }
      }
      if (task) {
        try {
          task();
        } catch (...) {
        }
      }
      PollGenerations();
    }
    StopGenerations();
  }

  void Shutdown() {
    StopPersistenceWorker();
    {
      std::scoped_lock lock{queueMutex};
      stopping = true;
    }
    queueCondition.notify_all();
    if (worker.joinable()) {
      worker.join();
    }
  }

  choreo::api::ApiServer& server;
  Options options;
  std::thread worker;
  std::thread persistenceWorker;
  std::mutex queueMutex;
  std::condition_variable queueCondition;
  std::deque<Task> tasks;
  bool stopping = false;
  std::mutex persistenceQueueMutex;
  std::condition_variable persistenceQueueCondition;
  std::deque<Task> persistenceTasks;
  bool persistenceStopping = false;
  std::unique_ptr<DocumentStore> store;
  std::atomic<std::shared_ptr<const PublishedSnapshot>> publishedSnapshot;
  DocumentFiles documentFiles;
  std::atomic_bool hasProjectPath{false};
  std::map<std::string, Generation, std::less<>> generations;
  std::map<choreo::api::WebSocketId, std::string> progressConnections;
  std::map<std::string, CachedGeneration, std::less<>> idempotentGenerations;
  std::map<std::string, CachedMutation, std::less<>> idempotentMutations;
  std::uint64_t sessionGeneration = 0;
  std::uint64_t persistenceSequence = 0;
  std::uint64_t lastAppliedPersistenceSequence = 0;
  bool persistenceBlocked = false;
};

ApplicationApi::ApplicationApi(choreo::api::ApiServer& server, Options options)
    : m_impl{std::make_shared<Impl>(server, std::move(options))} {
  m_impl->RegisterRoutes();
}

ApplicationApi::~ApplicationApi() {
  if (m_impl) {
    m_impl->Shutdown();
  }
}

}  // namespace choreo::document
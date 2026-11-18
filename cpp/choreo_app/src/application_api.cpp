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

  struct PublishedSnapshot {
    std::uint64_t revision;
    std::string body;
  };

  Impl(choreo::api::ApiServer& serverIn, Options optionsIn)
      : server{serverIn}, options{std::move(optionsIn)} {
    worker = std::thread([this] { RunWorker(); });
  }

  ~Impl() { StopWorker(); }

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
    server.RegisterRoute(
        "GET", "/api/v1/snapshot", [weak](const choreo::api::HttpRequest&) {
          return WithHttpOwner(weak, [](std::shared_ptr<Impl> self) {
            return self->SnapshotResponse();
          });
        });
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

  ApiResponse OpenProject(const std::filesystem::path& projectPath) {
    auto files = DiscoverFiles(projectPath);
    auto document = ReadDocument(files);
    StopGenerations();
    auto replacement = std::make_unique<DocumentStore>(std::move(document));
    replacement->ReloadFiles(files);
    store = std::move(replacement);
    documentFiles = std::move(files);
    PublishSnapshot();
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
      const auto path =
          NormalizeProjectPath(requestBody.at("projectPath").get_string());
      auto project = requestBody.at("project").get<Project>();
      self->StopGenerations();
      Document document;
      document.project = std::move(project);
      auto replacement = std::make_unique<DocumentStore>(std::move(document));
      DocumentFiles files;
      files.project = path;
      self->store = std::move(replacement);
      self->documentFiles = std::move(files);
      self->PublishSnapshot();
      return JsonResponse(201, Json::object("created", true));
    });
    return OperationAccepted(operationId, "new");
  }

  ApiResponse SaveSession(const choreo::api::HttpRequest& request) {
    const auto operationId = NewUuid();
    const auto body = request.body;
    EmitSessionOperation(operationId, "save", "running", "Saving session");
    auto self = shared_from_this();
    QueueSessionOperation(operationId, "save", [self, body] {
      std::optional<std::filesystem::path> saveAs;
      if (!body.empty()) {
        auto requestBody = Json::parse_or_throw(body);
        if (!requestBody.is_object()) {
          throw std::invalid_argument("request body must be an object");
        }
        if (requestBody.contains("projectPath") &&
            !requestBody.at("projectPath").is_null()) {
          saveAs =
              NormalizeProjectPath(requestBody.at("projectPath").get_string());
        }
      }
      if (!self->store) {
        return ApiError(409, "SESSION_NOT_OPEN", "No project is open");
      }
      auto files = self->documentFiles;
      if (saveAs) {
        files.project = *saveAs;
        for (auto& [id, path] : files.trajectories) {
          path = saveAs->parent_path() / path.filename();
        }
      }
      if (files.project.empty()) {
        return ApiError(422, "VALIDATION_ERROR",
                        "A project path is required for the first save");
      }
      auto result = self->store->SaveFiles(files);
      if (!result.Success()) {
        std::vector<Json> details;
        if (result.projectError) {
          details.push_back(Json::object("path", files.project.string(),
                                         "message", *result.projectError));
        }
        for (const auto& [id, error] : result.trajectoryErrors) {
          details.push_back(Json::object(
              "path", files.trajectories.at(id).string(), "message", error));
        }
        return JsonResponse(
            500, Json::object("code", "SAVE_FAILED", "message",
                              "One or more files could not be saved", "details",
                              std::move(details)));
      }
      self->documentFiles = std::move(files);
      return JsonResponse(200, Json::object("saved", true, "revision",
                                            self->store->Revision()));
    });
    return OperationAccepted(operationId, "save");
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

  void PublishSnapshot() {
    if (!store) {
      publishedSnapshot.store({});
      return;
    }
    auto snapshot = std::make_shared<const PublishedSnapshot>(
        PublishedSnapshot{store->Revision(), SnapshotJson().to_string()});
    publishedSnapshot.store(std::move(snapshot));
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
            PublishSnapshot();
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
  std::mutex queueMutex;
  std::condition_variable queueCondition;
  std::deque<Task> tasks;
  bool stopping = false;
  std::unique_ptr<DocumentStore> store;
  std::atomic<std::shared_ptr<const PublishedSnapshot>> publishedSnapshot;
  DocumentFiles documentFiles;
  std::map<std::string, Generation, std::less<>> generations;
  std::map<choreo::api::WebSocketId, std::string> progressConnections;
  std::map<std::string, CachedGeneration, std::less<>> idempotentGenerations;
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
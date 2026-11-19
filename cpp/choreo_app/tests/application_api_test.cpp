#include "choreo/document/application_api.hpp"

#include <array>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <wpi/util/json.hpp>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include "choreo/document/evaluation.hpp"
#include "choreo/document/trajectory_output.hpp"
#include "document_test_fixture.hpp"

namespace {

using choreo::api::ApiServer;
using choreo::document::ApplicationApi;
using choreo::document::Project;
using choreo::document::Trajectory;
using choreo::document::TrajectoryOutput;
using Json = wpi::util::json;

constexpr std::string_view kProjectFileName = "Project.chor";
constexpr std::string_view kTrajectoryFileName = "Path.traj";
constexpr std::string_view kNewProjectFileName = "Created.chor";
constexpr std::string_view kSessionOpenPath = "/api/v1/session/open";
constexpr std::string_view kSessionNewPath = "/api/v1/session/new";
constexpr std::string_view kSessionClosePath = "/api/v1/session/close";
constexpr std::string_view kSnapshotPath = "/api/v1/snapshot";
constexpr std::string_view kSavePath = "/api/v1/session/save";
constexpr std::string_view kEventsPath = "/api/v1/events";
constexpr std::string_view kIdempotencyKey =
    "00000000-0000-4000-8000-000000009001";
constexpr std::string_view kNewSessionIdempotencyKey =
    "00000000-0000-4000-8000-000000009002";
constexpr std::string_view kUndoIdempotencyKey =
    "00000000-0000-4000-8000-000000009003";
constexpr std::string_view kRedoIdempotencyKey =
    "00000000-0000-4000-8000-000000009004";
constexpr std::string_view kFailedUndoIdempotencyKey =
    "00000000-0000-4000-8000-000000009005";
constexpr std::string_view kStaleUndoIdempotencyKey =
    "00000000-0000-4000-8000-000000009006";
constexpr std::string_view kProjectPatchIdempotencyKey =
    "00000000-0000-4000-8000-000000009007";
constexpr std::string_view kTrajectoryPatchIdempotencyKey =
    "00000000-0000-4000-8000-000000009008";
constexpr std::string_view kFailedPatchIdempotencyKey =
    "00000000-0000-4000-8000-000000009009";
constexpr std::string_view kCreateWaypointIdempotencyKey =
    "00000000-0000-4000-8000-000000009010";
constexpr std::string_view kPatchWaypointIdempotencyKey =
    "00000000-0000-4000-8000-000000009011";
constexpr std::string_view kDeleteWaypointIdempotencyKey =
    "00000000-0000-4000-8000-000000009012";
constexpr std::string_view kCreateConstraintIdempotencyKey =
    "00000000-0000-4000-8000-000000009013";
constexpr std::string_view kPatchConstraintIdempotencyKey =
    "00000000-0000-4000-8000-000000009014";
constexpr std::string_view kDeleteConstraintIdempotencyKey =
    "00000000-0000-4000-8000-000000009015";
constexpr std::string_view kCreateEventIdempotencyKey =
    "00000000-0000-4000-8000-000000009016";
constexpr std::string_view kPatchEventIdempotencyKey =
    "00000000-0000-4000-8000-000000009017";
constexpr std::string_view kDeleteEventIdempotencyKey =
    "00000000-0000-4000-8000-000000009018";
constexpr std::string_view kCreatedWaypointUuid =
    "00000000-0000-4000-8000-000000009101";
constexpr std::string_view kCreatedConstraintUuid =
    "00000000-0000-4000-8000-000000009102";
constexpr std::string_view kCreatedEventUuid =
    "00000000-0000-4000-8000-000000009103";
constexpr int kHttpOk = 200;
constexpr int kHttpAccepted = 202;
constexpr int kHttpBadRequest = 400;
constexpr int kHttpConflict = 409;
constexpr int kHttpNotFound = 404;
constexpr std::size_t kExpectedTrajectoryCount = 1;
constexpr std::size_t kExpectedSingleItemCollectionCount = 1;
constexpr std::size_t kNoTrajectories = 0;
constexpr std::size_t kNoEvents = 0;
constexpr std::size_t kTemporaryDirectoryAttempts = 16;
constexpr std::size_t kMaximumSessionOperationEvents = 8;
constexpr std::size_t kMaximumGenerationEvents = 12;
constexpr std::size_t kMaximumWebSocketPayloadBytes = 1024 * 1024;

void Require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string{message});
  }
}

struct WinsockContext {
#ifdef _WIN32
  WSADATA data{};
  bool ready = WSAStartup(MAKEWORD(2, 2), &data) == 0;
  ~WinsockContext() {
    if (ready) {
      WSACleanup();
    }
  }
#else
  bool ready = true;
#endif
};

#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket kInvalidSocket = INVALID_SOCKET;
void CloseSocket(Socket socket) {
  closesocket(socket);
}
#else
using Socket = int;
constexpr Socket kInvalidSocket = -1;
void CloseSocket(Socket socket) {
  close(socket);
}
#endif

class SocketHandle {
 public:
  SocketHandle() = default;
  explicit SocketHandle(Socket socket) : socket_{socket} {}
  ~SocketHandle() { Reset(); }

  SocketHandle(const SocketHandle&) = delete;
  SocketHandle& operator=(const SocketHandle&) = delete;

  SocketHandle(SocketHandle&& other) noexcept
      : socket_{std::exchange(other.socket_, kInvalidSocket)} {}

  SocketHandle& operator=(SocketHandle&& other) noexcept {
    if (this != &other) {
      Reset();
      socket_ = std::exchange(other.socket_, kInvalidSocket);
    }
    return *this;
  }

  explicit operator bool() const { return socket_ != kInvalidSocket; }
  Socket Get() const { return socket_; }

 private:
  void Reset() {
    if (socket_ != kInvalidSocket) {
      CloseSocket(socket_);
      socket_ = kInvalidSocket;
    }
  }

  Socket socket_ = kInvalidSocket;
};

void SetReceiveTimeout(Socket socket) {
#ifdef _WIN32
  DWORD timeout = 5000;
  setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,
             reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
  timeval timeout{5, 0};
  setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
}

Socket Connect(std::uint16_t port) {
  Socket socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socket == kInvalidSocket) {
    return kInvalidSocket;
  }
  SetReceiveTimeout(socket);
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (connect(socket, reinterpret_cast<const sockaddr*>(&address),
              sizeof(address)) != 0) {
    CloseSocket(socket);
    return kInvalidSocket;
  }
  return socket;
}

bool SendAll(Socket socket, std::string_view text) {
  while (!text.empty()) {
#ifdef _WIN32
    auto amount = send(socket, text.data(), static_cast<int>(text.size()), 0);
    if (amount == SOCKET_ERROR && WSAGetLastError() == WSAEINTR) {
      continue;
    }
#else
    auto amount = send(socket, text.data(), text.size(), 0);
    if (amount < 0 && errno == EINTR) {
      continue;
    }
#endif
    if (amount <= 0) {
      return false;
    }
    text.remove_prefix(static_cast<std::size_t>(amount));
  }
  return true;
}

bool ReadExact(Socket socket, char* buffer, std::size_t size) {
  std::size_t read = 0;
  while (read < size) {
#ifdef _WIN32
    auto amount = recv(socket, buffer + read, static_cast<int>(size - read), 0);
    if (amount == SOCKET_ERROR && WSAGetLastError() == WSAEINTR) {
      continue;
    }
#else
    auto amount = recv(socket, buffer + read, size - read, 0);
    if (amount < 0 && errno == EINTR) {
      continue;
    }
#endif
    if (amount <= 0) {
      return false;
    }
    read += static_cast<std::size_t>(amount);
  }
  return true;
}

std::string ReadAll(Socket socket) {
  std::string response;
  char buffer[2048];
  while (true) {
#ifdef _WIN32
    auto amount = recv(socket, buffer, sizeof(buffer), 0);
#else
    auto amount = recv(socket, buffer, sizeof(buffer), 0);
#endif
    if (amount <= 0) {
#ifdef _WIN32
      if (amount == SOCKET_ERROR && WSAGetLastError() == WSAEINTR) {
        continue;
      }
#else
      if (amount < 0 && errno == EINTR) {
        continue;
      }
#endif
      break;
    }
    response.append(buffer, static_cast<std::size_t>(amount));
  }
  return response;
}

std::string ReadHeaders(Socket socket) {
  std::string response;
  char buffer[512];
  while (response.find("\r\n\r\n") == std::string::npos) {
#ifdef _WIN32
    auto amount = recv(socket, buffer, sizeof(buffer), 0);
#else
    auto amount = recv(socket, buffer, sizeof(buffer), 0);
#endif
    if (amount <= 0) {
#ifdef _WIN32
      if (amount == SOCKET_ERROR && WSAGetLastError() == WSAEINTR) {
        continue;
      }
#else
      if (amount < 0 && errno == EINTR) {
        continue;
      }
#endif
      break;
    }
    response.append(buffer, static_cast<std::size_t>(amount));
  }
  return response;
}

std::string ReadServerTextFrame(Socket socket) {
  while (true) {
    std::array<unsigned char, 2> header{};
    if (!ReadExact(socket, reinterpret_cast<char*>(header.data()),
                   header.size())) {
      return {};
    }
    std::uint64_t size = header[1] & 0x7f;
    if (size == 126) {
      std::array<unsigned char, 2> extended{};
      if (!ReadExact(socket, reinterpret_cast<char*>(extended.data()),
                     extended.size())) {
        return {};
      }
      size = (static_cast<std::uint64_t>(extended[0]) << 8) | extended[1];
    } else if (size == 127) {
      std::array<unsigned char, 8> extended{};
      if (!ReadExact(socket, reinterpret_cast<char*>(extended.data()),
                     extended.size())) {
        return {};
      }
      size = 0;
      for (auto byte : extended) {
        size = (size << 8) | byte;
      }
    }
    if ((header[1] & 0x80) != 0 || size > kMaximumWebSocketPayloadBytes) {
      return {};
    }
    std::string payload(static_cast<std::size_t>(size), '\0');
    if (!ReadExact(socket, payload.data(), payload.size())) {
      return {};
    }

    switch (header[0] & 0x0f) {
      case 0x1:
        return payload;
      case 0x8:
        return {};
      case 0x9: {
        if (payload.size() > 125) {
          return {};
        }
        constexpr std::array<unsigned char, 4> mask{0x13, 0x57, 0x9b, 0xdf};
        std::string pong;
        pong.reserve(payload.size() + 6);
        pong.push_back(static_cast<char>(0x8a));
        pong.push_back(static_cast<char>(0x80 | payload.size()));
        for (auto byte : mask) {
          pong.push_back(static_cast<char>(byte));
        }
        for (std::size_t index = 0; index < payload.size(); ++index) {
          pong.push_back(
              static_cast<char>(static_cast<unsigned char>(payload[index]) ^
                                mask[index % mask.size()]));
        }
        if (!SendAll(socket, pong)) {
          return {};
        }
        break;
      }
      case 0xa:
        break;
      default:
        return {};
    }
  }
}

std::string ReadFileContents(const std::filesystem::path& path) {
  std::ifstream input{path, std::ios::binary};
  if (!input) {
    throw std::runtime_error("cannot open test file: " + path.string());
  }
  std::string content{std::istreambuf_iterator<char>{input},
                      std::istreambuf_iterator<char>{}};
  if (input.bad()) {
    throw std::runtime_error("cannot read test file: " + path.string());
  }
  return content;
}

std::string ReadRequestBody(Socket socket) {
  const auto response = ReadAll(socket);
  const auto separator = response.find("\r\n\r\n");
  if (separator == std::string::npos) {
    throw std::runtime_error("HTTP response is missing its header terminator");
  }
  return response.substr(separator + 4);
}

std::pair<int, std::string> HttpRequest(
    std::uint16_t port, std::string_view method, std::string_view path,
    std::string_view body, std::string_view extraHeaders = {},
    std::string_view contentType = "application/json") {
  SocketHandle socket{Connect(port)};
  if (!socket) {
    return {0, {}};
  }
  std::string request = std::string{method} + " " + std::string{path} +
                        " HTTP/1.1\r\nHost: 127.0.0.1:" + std::to_string(port) +
                        "\r\n";
  if (!body.empty()) {
    request += "Content-Type: " + std::string{contentType} +
               "\r\nContent-Length: " + std::to_string(body.size()) + "\r\n";
  }
  request += extraHeaders;
  request += "Connection: close\r\n\r\n";
  request += body;
  if (!SendAll(socket.Get(), request)) {
    return {0, {}};
  }
  const auto response = ReadAll(socket.Get());
  const auto separator = response.find("\r\n\r\n");
  if (separator == std::string::npos) {
    return {0, {}};
  }
  int status = 0;
  const auto firstSpace = response.find(' ');
  if (firstSpace != std::string::npos && firstSpace + 4 <= response.size()) {
    status = std::atoi(response.c_str() + firstSpace + 1);
  }
  return {status, response.substr(separator + 4)};
}

bool SendProgressMessage(std::string_view url, std::string_view payload) {
  constexpr std::string_view prefix = "ws://127.0.0.1:";
  if (!url.starts_with(prefix)) {
    return false;
  }
  url.remove_prefix(prefix.size());
  const auto pathStart = url.find('/');
  if (pathStart == std::string_view::npos) {
    return false;
  }
  unsigned int port = 0;
  const auto portText = url.substr(0, pathStart);
  const auto [portEnd, portError] =
      std::from_chars(portText.data(), portText.data() + portText.size(), port);
  if (portError != std::errc{} ||
      portEnd != portText.data() + portText.size() || port == 0 ||
      port > std::numeric_limits<std::uint16_t>::max() ||
      payload.size() >= 126) {
    return false;
  }

  SocketHandle socket{Connect(static_cast<std::uint16_t>(port))};
  if (!socket) {
    return false;
  }
  const auto handshake =
      "GET " + std::string{url.substr(pathStart)} +
      " HTTP/1.1\r\nHost: 127.0.0.1:" + std::to_string(port) +
      "\r\n"
      "Upgrade: websocket\r\nConnection: Upgrade\r\n"
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
      "Sec-WebSocket-Version: 13\r\n\r\n";
  if (!SendAll(socket.Get(), handshake) ||
      ReadHeaders(socket.Get()).find("101 Switching Protocols") ==
          std::string::npos) {
    return false;
  }

  std::array<std::uint8_t, 4> mask{};
  std::random_device random;
  for (auto& byte : mask) {
    byte = static_cast<std::uint8_t>(random());
  }
  std::string frame;
  frame.reserve(payload.size() + 6);
  frame.push_back(static_cast<char>(0x81));
  frame.push_back(static_cast<char>(0x80 | payload.size()));
  for (auto byte : mask) {
    frame.push_back(static_cast<char>(byte));
  }
  for (std::size_t index = 0; index < payload.size(); ++index) {
    frame.push_back(static_cast<char>(
        static_cast<std::uint8_t>(payload[index]) ^ mask[index % mask.size()]));
  }
  return SendAll(socket.Get(), frame);
}

int RunFakeGenerator(int argc, char** argv) {
  std::filesystem::path projectPath;
  std::filesystem::path trajectoryPath;
  std::filesystem::path outputPath;
  std::string progressUrl;
  for (int index = 1; index + 1 < argc; ++index) {
    const std::string_view argument{argv[index]};
    if (argument == "--chor") {
      projectPath = std::filesystem::u8path(argv[++index]);
    } else if (argument == "--trajectory") {
      trajectoryPath = std::filesystem::u8path(argv[++index]);
    } else if (argument == "--output") {
      outputPath = std::filesystem::u8path(argv[++index]);
    } else if (argument == "--progress-url") {
      progressUrl = argv[++index];
    }
  }
  if (projectPath.empty() || trajectoryPath.empty() || outputPath.empty()) {
    return EXIT_FAILURE;
  }
  try {
    auto project =
        Json::parse_or_throw(ReadFileContents(projectPath)).get<Project>();
    auto trajectory = Json::parse_or_throw(ReadFileContents(trajectoryPath))
                          .get<Trajectory>();
    trajectory.config = project.config;
    trajectory.snapshot = trajectory.params;
    TrajectoryOutput output;
    output.waypoints.resize(trajectory.params.waypoints.size(), 0.0);
    output.splits = {0};
    output.trajectory = std::optional<wpi::math::HolonomicTrajectory>{
        wpi::math::HolonomicTrajectory{std::vector<wpi::math::HolonomicSample>{
            wpi::math::HolonomicSample{}}}};
    trajectory.output = std::move(output);
    WinsockContext winsock;
    if (!winsock.ready) {
      return EXIT_FAILURE;
    }
    if (!progressUrl.empty() &&
        !SendProgressMessage(
            progressUrl,
            Json::object("version", 1, "event", "diagnostic", "payload",
                         Json::object("text", "fake generator progress"))
                .to_string())) {
      return EXIT_FAILURE;
    }
    std::ofstream file{outputPath, std::ios::binary};
    file << Json{trajectory}.to_string();
    return file ? EXIT_SUCCESS : EXIT_FAILURE;
  } catch (...) {
    return EXIT_FAILURE;
  }
}

class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    std::random_device random;
    for (std::size_t attempt = 0; attempt < kTemporaryDirectoryAttempts;
         ++attempt) {
      auto candidate = std::filesystem::temp_directory_path() /
                       ("choreo-application-api-" + std::to_string(random()) +
                        "-" + std::to_string(random()));
      std::error_code error;
      if (std::filesystem::create_directory(candidate, error)) {
        path_ = std::move(candidate);
        return;
      }
      if (error && error != std::errc::file_exists) {
        throw std::runtime_error("cannot create temporary test directory: " +
                                 error.message());
      }
    }
    throw std::runtime_error("cannot create unique temporary test directory");
  }

  ~TemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }

  const std::filesystem::path& Path() const { return path_; }

 private:
  std::filesystem::path path_;
};

enum class PickerMode { SelectImmediately, CancelImmediately, Deferred };

struct DeferredPicker {
  std::mutex mutex;
  ApplicationApi::ProjectPathCompletion completion;
};

class ApiTestFixture {
 public:
  explicit ApiTestFixture(const std::filesystem::path& executable,
                          PickerMode pickerMode = PickerMode::SelectImmediately)
      : projectPath_{directory_.Path() / kProjectFileName},
        trajectoryPath_{directory_.Path() / kTrajectoryFileName},
        deferredPicker_{std::make_shared<DeferredPicker>()} {
    Require(winsock_.ready, "Winsock initialization failed");
    WriteInitialDocuments();
    ApplicationApi::Options options;
    options.generatorExecutable = std::filesystem::absolute(executable);
    options.promptForProjectPath = [path = projectPath_, pickerMode,
                                    deferred = deferredPicker_](auto complete) {
      if (pickerMode == PickerMode::CancelImmediately) {
        complete(std::nullopt);
      } else if (pickerMode == PickerMode::SelectImmediately) {
        complete(path);
      } else {
        std::scoped_lock lock{deferred->mutex};
        deferred->completion = std::move(complete);
      }
    };
    application_ =
        std::make_unique<ApplicationApi>(server_, std::move(options));
    server_.Start();
  }

  ApiTestFixture(const ApiTestFixture&) = delete;
  ApiTestFixture& operator=(const ApiTestFixture&) = delete;

  ~ApiTestFixture() {
    application_.reset();
    server_.Stop();
  }

  std::uint16_t Port() const { return server_.Port(); }
  const std::filesystem::path& ProjectPath() const { return projectPath_; }
  const std::filesystem::path& TrajectoryPath() const {
    return trajectoryPath_;
  }

  std::pair<int, std::string> OpenPicker() const {
    return HttpRequest(Port(), "POST", kSessionOpenPath,
                       Json::object("source", "picker").to_string());
  }

  std::pair<int, std::string> OpenPath(
      const std::filesystem::path& path) const {
    return HttpRequest(
        Port(), "POST", kSessionOpenPath,
        Json::object("source", "path", "path", path.string()).to_string());
  }

  std::pair<int, std::string> NewSession(const std::filesystem::path& path,
                                         const Json& project) const {
    const auto body =
        Json::object("projectPath", path.string(), "project", project)
            .to_string();
    const auto headers =
        "Idempotency-Key: " + std::string{kNewSessionIdempotencyKey} + "\r\n";
    return HttpRequest(Port(), "POST", kSessionNewPath, body, headers);
  }

  std::pair<int, std::string> Snapshot() const {
    return HttpRequest(Port(), "GET", kSnapshotPath, {});
  }

  std::pair<int, std::string> GetTrajectory(
      std::string_view projectId, std::string_view trajectoryId) const {
    const auto path = "/api/v1/projects/" + std::string{projectId} +
                      "/trajectories/" + std::string{trajectoryId};
    return HttpRequest(Port(), "GET", path, {});
  }

  std::pair<int, std::string> GetProject(std::string_view projectId) const {
    const auto path = "/api/v1/projects/" + std::string{projectId};
    return HttpRequest(Port(), "GET", path, {});
  }

  std::pair<int, std::string> ListTrajectories(
      std::string_view projectId) const {
    const auto path =
        "/api/v1/projects/" + std::string{projectId} + "/trajectories";
    return HttpRequest(Port(), "GET", path, {});
  }

  std::pair<int, std::string> GetTrajectorySubresource(
      std::string_view projectId, std::string_view trajectoryId,
      std::string_view resource) const {
    const auto path = "/api/v1/projects/" + std::string{projectId} +
                      "/trajectories/" + std::string{trajectoryId} + "/" +
                      std::string{resource};
    return HttpRequest(Port(), "GET", path, {});
  }

  std::pair<int, std::string> GetTrajectorySubresourceItem(
      std::string_view projectId, std::string_view trajectoryId,
      std::string_view resource, std::string_view itemId) const {
    const auto path = "/api/v1/projects/" + std::string{projectId} +
                      "/trajectories/" + std::string{trajectoryId} + "/" +
                      std::string{resource} + "/" + std::string{itemId};
    return HttpRequest(Port(), "GET", path, {});
  }

  std::pair<int, std::string> Save() const {
    return HttpRequest(Port(), "POST", kSavePath, "{}");
  }

  std::pair<int, std::string> PatchProject(
      std::string_view projectId, const Json& patch, std::uint64_t revision,
      std::string_view idempotencyKey) const {
    const auto path = "/api/v1/projects/" + std::string{projectId};
    const auto headers =
        "If-Match: \"revision-" + std::to_string(revision) + "\"\r\n" +
        "Idempotency-Key: " + std::string{idempotencyKey} + "\r\n";
    return HttpRequest(Port(), "PATCH", path, patch.to_string(), headers,
                       "application/merge-patch+json");
  }

  std::pair<int, std::string> PatchTrajectory(
      std::string_view projectId, std::string_view trajectoryId,
      const Json& patch, std::uint64_t revision,
      std::string_view idempotencyKey) const {
    const auto path = "/api/v1/projects/" + std::string{projectId} +
                      "/trajectories/" + std::string{trajectoryId};
    const auto headers =
        "If-Match: \"revision-" + std::to_string(revision) + "\"\r\n" +
        "Idempotency-Key: " + std::string{idempotencyKey} + "\r\n";
    return HttpRequest(Port(), "PATCH", path, patch.to_string(), headers,
                       "application/merge-patch+json");
  }

  std::pair<int, std::string> MutateSubresource(
      std::string_view method, std::string_view resource,
      std::optional<std::string_view> itemId, std::string_view body,
      std::string_view contentType, std::uint64_t revision,
      std::string_view idempotencyKey) const {
    auto path = "/api/v1/projects/" + std::string{kTestProjectUuid} +
                "/trajectories/" + std::string{kTestTrajectoryUuid} + "/" +
                std::string{resource};
    if (itemId) {
      path += "/" + std::string{*itemId};
    }
    const auto headers =
        "If-Match: \"revision-" + std::to_string(revision) + "\"\r\n" +
        "Idempotency-Key: " + std::string{idempotencyKey} + "\r\n";
    return HttpRequest(Port(), method, path, body, headers, contentType);
  }

  std::pair<int, std::string> HistoryAction(
      std::string_view action, std::uint64_t revision,
      std::string_view idempotencyKey) const {
    const auto path = "/api/v1/session/" + std::string{action};
    const auto headers =
        "If-Match: \"revision-" + std::to_string(revision) + "\"\r\n" +
        "Idempotency-Key: " + std::string{idempotencyKey} + "\r\n";
    return HttpRequest(Port(), "POST", path, {}, headers);
  }

  void CompletePicker(std::optional<std::filesystem::path> path) const {
    ApplicationApi::ProjectPathCompletion completion;
    {
      std::scoped_lock lock{deferredPicker_->mutex};
      completion = std::move(deferredPicker_->completion);
    }
    Require(static_cast<bool>(completion),
            "deferred picker callback should be available");
    completion(std::move(path));
  }

  SocketHandle ConnectEvents() const {
    SocketHandle socket{Connect(Port())};
    if (!socket) {
      return {};
    }
    const auto request =
        "GET " + std::string{kEventsPath} +
        " HTTP/1.1\r\nHost: 127.0.0.1:" + std::to_string(Port()) +
        "\r\n"
        "Upgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n";
    if (!SendAll(socket.Get(), request) ||
        ReadHeaders(socket.Get()).find("101 Switching Protocols") ==
            std::string::npos) {
      return {};
    }
    return socket;
  }

 private:
  void WriteInitialDocuments() const {
    auto document = MakeTestDocument(MakeTestProject());
    document.project.name = "Project";
    auto evaluated = choreo::document::evaluation::EvaluateDocument(document);
    auto trajectory = evaluated.trajectories.at(kTestTrajectoryUuid);
    trajectory.name = "Path";
    std::ofstream projectFile{projectPath_, std::ios::binary};
    projectFile << Json{evaluated.project}.to_string();
    std::ofstream trajectoryFile{trajectoryPath_, std::ios::binary};
    trajectoryFile << Json{trajectory}.to_string();
    Require(projectFile.good() && trajectoryFile.good(),
            "could not write initial project/trajectory files");
  }

  WinsockContext winsock_;
  TemporaryDirectory directory_;
  std::filesystem::path projectPath_;
  std::filesystem::path trajectoryPath_;
  ApiServer server_;
  std::unique_ptr<ApplicationApi> application_;
  std::shared_ptr<DeferredPicker> deferredPicker_;
};

std::string AcceptedOperationId(int status, std::string_view body,
                                std::string_view expectedOperation) {
  Require(status == kHttpAccepted,
          "session request should return HTTP 202 Accepted (HTTP " +
              std::to_string(status) + "): " + std::string{body});
  const auto accepted = Json::parse_or_throw(body);
  Require(accepted.at("accepted").get_bool(),
          "accepted response should indicate the operation was queued");
  Require(accepted.at("operation").get_string() == expectedOperation,
          "accepted response should identify its operation");
  return accepted.at("operationId").get_string();
}

Json WaitForSessionOperation(Socket socket, std::string_view operationId,
                             std::string_view operation,
                             std::string_view expectedStatus) {
  for (std::size_t attempt = 0; attempt < kMaximumSessionOperationEvents;
       ++attempt) {
    const auto frame = ReadServerTextFrame(socket);
    Require(!frame.empty(), "expected a session operation event");
    const auto event = Json::parse_or_throw(frame);
    if (event.at("type").get_string() != "session.operation" ||
        event.at("operationId").get_string() != operationId ||
        event.at("operation").get_string() != operation) {
      continue;
    }
    const auto status = event.at("status").get_string();
    if (status == "failed" && expectedStatus == "failed") {
      return event;
    }
    if (status == "failed") {
      throw std::runtime_error("session operation failed: " +
                               event.at("error").at("message").get_string());
    }
    if (status == expectedStatus) {
      return event;
    }
  }
  throw std::runtime_error("expected terminal session operation event");
}

Json WaitForDocumentChanged(Socket socket, std::uint64_t revision) {
  for (std::size_t attempt = 0; attempt < kMaximumSessionOperationEvents;
       ++attempt) {
    const auto frame = ReadServerTextFrame(socket);
    Require(!frame.empty(), "expected a committed document event");
    const auto event = Json::parse_or_throw(frame);
    if (event.at("type").get_string() != "document.changed" ||
        static_cast<std::uint64_t>(event.at("revision").get_number()) !=
            revision) {
      continue;
    }
    Require(event.at("projectId").get_string() == kTestProjectUuid,
            "document event should identify the active project");
    return event;
  }
  throw std::runtime_error("expected document revision event");
}

void TestPickerOpenLoadsProjectSnapshot(
    const std::filesystem::path& executable) {
  ApiTestFixture fixture{executable, PickerMode::Deferred};
  auto events = fixture.ConnectEvents();
  Require(static_cast<bool>(events), "event WebSocket should connect");
  const auto [openStatus, openBody] = fixture.OpenPicker();
  const auto operationId = AcceptedOperationId(openStatus, openBody, "open");
  fixture.CompletePicker(fixture.ProjectPath());
  WaitForSessionOperation(events.Get(), operationId, "open", "succeeded");

  const auto [snapshotStatus, snapshotBody] = fixture.Snapshot();
  Require(snapshotStatus == kHttpOk, "snapshot should return HTTP 200");
  const auto snapshot = Json::parse_or_throw(snapshotBody);
  Require(snapshot.at("project").at("uuid").get_string() == kTestProjectUuid,
          "snapshot should contain the selected project");
  const auto trajectories = snapshot.at("trajectories").get_array();
  Require(trajectories.size() == kExpectedTrajectoryCount,
          "snapshot should contain its trajectory");
  Require(trajectories.front().at("uuid").get_string() == kTestTrajectoryUuid,
          "snapshot should preserve trajectory identity");
}

void TestPickerCancellationLeavesSessionClosed(
    const std::filesystem::path& executable) {
  ApiTestFixture fixture{executable, PickerMode::CancelImmediately};
  auto events = fixture.ConnectEvents();
  Require(static_cast<bool>(events), "event WebSocket should connect");
  const auto [openStatus, openBody] = fixture.OpenPicker();
  const auto operationId = AcceptedOperationId(openStatus, openBody, "open");
  WaitForSessionOperation(events.Get(), operationId, "open", "cancelled");
  const auto [snapshotStatus, snapshotBody] = fixture.Snapshot();
  Require(snapshotStatus == kHttpConflict,
          "cancelled picker should not create a session");
  Require(Json::parse_or_throw(snapshotBody).at("code").get_string() ==
              "SESSION_NOT_OPEN",
          "closed session should report SESSION_NOT_OPEN");
}

void TestInvalidProjectPathIsRejected(const std::filesystem::path& executable) {
  ApiTestFixture fixture{executable};
  const auto invalidPath =
      fixture.ProjectPath().parent_path() / "not-project.txt";
  const auto [status, body] = fixture.OpenPath(invalidPath);
  Require(status == kHttpBadRequest, "non-.chor path should be rejected");
  Require(
      Json::parse_or_throw(body).at("code").get_string() == "VALIDATION_ERROR",
      "invalid path should report VALIDATION_ERROR");
}

void TestNewSessionCompletesAsynchronously(
    const std::filesystem::path& executable) {
  ApiTestFixture fixture{executable};
  auto events = fixture.ConnectEvents();
  Require(static_cast<bool>(events), "event WebSocket should connect");
  const auto project =
      Json::parse_or_throw(ReadFileContents(fixture.ProjectPath()));
  const auto newProjectPath =
      fixture.ProjectPath().parent_path() / kNewProjectFileName;
  const auto [status, responseBody] =
      fixture.NewSession(newProjectPath, project);
  const auto operationId = AcceptedOperationId(status, responseBody, "new");
  WaitForSessionOperation(events.Get(), operationId, "new", "succeeded");

  const auto [snapshotStatus, snapshotBody] = fixture.Snapshot();
  Require(snapshotStatus == kHttpOk,
          "new session should be available after its completion event");
  const auto snapshot = Json::parse_or_throw(snapshotBody);
  Require(snapshot.at("project").at("uuid").get_string() == kTestProjectUuid,
          "new session should load its supplied project");
  Require(snapshot.at("trajectories").get_array().empty(),
          "new session should begin without trajectories");
}

void TestCloseSessionClearsSnapshot(const std::filesystem::path& executable) {
  ApiTestFixture fixture{executable};
  auto events = fixture.ConnectEvents();
  Require(static_cast<bool>(events), "event WebSocket should connect");
  const auto project =
      Json::parse_or_throw(ReadFileContents(fixture.ProjectPath()));
  const auto [newStatus, newBody] =
      fixture.NewSession(fixture.ProjectPath(), project);
  const auto newOperationId = AcceptedOperationId(newStatus, newBody, "new");
  WaitForSessionOperation(events.Get(), newOperationId, "new", "succeeded");

  std::filesystem::remove(fixture.ProjectPath());
  const auto [closeStatus, closeBody] =
      HttpRequest(fixture.Port(), "POST", kSessionClosePath, {});
  const auto closeOperationId =
      AcceptedOperationId(closeStatus, closeBody, "close");
  WaitForSessionOperation(events.Get(), closeOperationId, "close", "succeeded");

  const auto [snapshotStatus, snapshotBody] = fixture.Snapshot();
  Require(snapshotStatus == kHttpConflict,
          "closed session should no longer have an available snapshot");
  Require(Json::parse_or_throw(snapshotBody).at("code").get_string() ==
              "SESSION_NOT_OPEN",
          "closed session should report SESSION_NOT_OPEN");
  Require(Json::parse_or_throw(ReadFileContents(fixture.ProjectPath()))
                  .at("uuid")
                  .get_string() == kTestProjectUuid,
          "close should persist the project before releasing the session");
}

void TestCloseSessionRetainsSessionWhenSaveFails(
    const std::filesystem::path& executable) {
  ApiTestFixture fixture{executable};
  auto events = fixture.ConnectEvents();
  Require(static_cast<bool>(events), "event WebSocket should connect");
  const auto project =
      Json::parse_or_throw(ReadFileContents(fixture.ProjectPath()));
  const auto [newStatus, newBody] =
      fixture.NewSession(fixture.ProjectPath(), project);
  const auto newOperationId = AcceptedOperationId(newStatus, newBody, "new");
  WaitForSessionOperation(events.Get(), newOperationId, "new", "succeeded");

  std::filesystem::remove(fixture.ProjectPath());
  std::filesystem::create_directory(fixture.ProjectPath());
  const auto [closeStatus, closeBody] =
      HttpRequest(fixture.Port(), "POST", kSessionClosePath, {});
  const auto closeOperationId =
      AcceptedOperationId(closeStatus, closeBody, "close");
  const auto closeEvent = WaitForSessionOperation(
      events.Get(), closeOperationId, "close", "failed");
  Require(closeEvent.at("error").at("code").get_string() == "SAVE_FAILED",
          "close should report the failed save");

  const auto [snapshotStatus, snapshotBody] = fixture.Snapshot();
  Require(snapshotStatus == kHttpOk,
          "session should remain open when saving before close fails");
  Require(!snapshotBody.empty(), "retained session should remain readable");
}

void TestSessionReplacementRetainsActiveSessionWhenSaveFails(
    const std::filesystem::path& executable) {
  ApiTestFixture fixture{executable};
  auto events = fixture.ConnectEvents();
  Require(static_cast<bool>(events), "event WebSocket should connect");
  const auto project =
      Json::parse_or_throw(ReadFileContents(fixture.ProjectPath()));
  const auto [initialNewStatus, initialNewBody] =
      fixture.NewSession(fixture.ProjectPath(), project);
  const auto initialNewOperationId =
      AcceptedOperationId(initialNewStatus, initialNewBody, "new");
  WaitForSessionOperation(events.Get(), initialNewOperationId, "new",
                          "succeeded");

  const auto alternateDirectory =
      fixture.ProjectPath().parent_path() / "alternate-project";
  std::filesystem::create_directory(alternateDirectory);
  const auto alternateProjectPath = alternateDirectory / kProjectFileName;
  std::filesystem::copy_file(fixture.ProjectPath(), alternateProjectPath);
  std::filesystem::copy_file(fixture.TrajectoryPath(),
                             alternateDirectory / kTrajectoryFileName);
  std::filesystem::remove(fixture.ProjectPath());
  std::filesystem::create_directory(fixture.ProjectPath());

  const auto [replacementStatus, replacementBody] =
      fixture.OpenPath(alternateProjectPath);
  const auto replacementOperationId =
      AcceptedOperationId(replacementStatus, replacementBody, "open");
  const auto replacementEvent = WaitForSessionOperation(
      events.Get(), replacementOperationId, "open", "failed");
  Require(replacementEvent.at("error").at("code").get_string() == "SAVE_FAILED",
          "open should report failure to persist the active session");

  const auto newProjectPath = alternateDirectory / kNewProjectFileName;
  const auto [replacementNewStatus, replacementNewBody] =
      fixture.NewSession(newProjectPath, project);
  const auto replacementNewOperationId =
      AcceptedOperationId(replacementNewStatus, replacementNewBody, "new");
  const auto newEvent = WaitForSessionOperation(
      events.Get(), replacementNewOperationId, "new", "failed");
  Require(newEvent.at("error").at("code").get_string() == "SAVE_FAILED",
          "new should report failure to persist the active session");

  const auto [snapshotStatus, snapshotBody] = fixture.Snapshot();
  Require(snapshotStatus == kHttpOk,
          "failed replacement should leave the active session readable");
  Require(Json::parse_or_throw(snapshotBody)
                  .at("project")
                  .at("uuid")
                  .get_string() == kTestProjectUuid,
          "failed replacement should preserve the original project");
}

void TestTrajectoryReadReturnsDocumentAndRejectsUnknownIds(
    const std::filesystem::path& executable) {
  ApiTestFixture fixture{executable};
  auto events = fixture.ConnectEvents();
  Require(static_cast<bool>(events), "event WebSocket should connect");
  const auto [openStatus, openBody] = fixture.OpenPath(fixture.ProjectPath());
  const auto openOperationId =
      AcceptedOperationId(openStatus, openBody, "open");
  WaitForSessionOperation(events.Get(), openOperationId, "open", "succeeded");

  const auto [status, body] =
      fixture.GetTrajectory(kTestProjectUuid, kTestTrajectoryUuid);
  Require(status == kHttpOk, "existing trajectory should return HTTP 200");
  const auto trajectory = Json::parse_or_throw(body);
  Require(trajectory.at("uuid").get_string() == kTestTrajectoryUuid,
          "trajectory response should preserve the requested UUID");

  const auto [missingStatus, missingBody] =
      fixture.GetTrajectory(kTestProjectUuid, "missing-trajectory");
  Require(missingStatus == kHttpNotFound,
          "unknown trajectory should return HTTP 404");
  Require(Json::parse_or_throw(missingBody).at("code").get_string() ==
              "RESOURCE_NOT_FOUND",
          "unknown trajectory should report RESOURCE_NOT_FOUND");
}

void TestProjectReadReturnsActiveDocument(
    const std::filesystem::path& executable) {
  ApiTestFixture fixture{executable};
  auto events = fixture.ConnectEvents();
  Require(static_cast<bool>(events), "event WebSocket should connect");
  const auto [openStatus, openBody] = fixture.OpenPath(fixture.ProjectPath());
  const auto openOperationId =
      AcceptedOperationId(openStatus, openBody, "open");
  WaitForSessionOperation(events.Get(), openOperationId, "open", "succeeded");

  const auto [status, body] = fixture.GetProject(kTestProjectUuid);
  Require(status == kHttpOk, "existing project should return HTTP 200");
  Require(
      Json::parse_or_throw(body).at("uuid").get_string() == kTestProjectUuid,
      "project response should preserve the requested UUID");

  const auto [missingStatus, missingBody] =
      fixture.GetProject("missing-project");
  Require(missingStatus == kHttpNotFound,
          "unknown project should return HTTP 404");
  Require(Json::parse_or_throw(missingBody).at("code").get_string() ==
              "RESOURCE_NOT_FOUND",
          "unknown project should report RESOURCE_NOT_FOUND");
}

void TestTrajectoryCollectionReturnsRevisionAndItems(
    const std::filesystem::path& executable) {
  ApiTestFixture fixture{executable};
  auto events = fixture.ConnectEvents();
  Require(static_cast<bool>(events), "event WebSocket should connect");
  const auto [openStatus, openBody] = fixture.OpenPath(fixture.ProjectPath());
  const auto openOperationId =
      AcceptedOperationId(openStatus, openBody, "open");
  WaitForSessionOperation(events.Get(), openOperationId, "open", "succeeded");

  const auto [status, body] = fixture.ListTrajectories(kTestProjectUuid);
  Require(status == kHttpOk, "trajectory collection should return HTTP 200");
  const auto collection = Json::parse_or_throw(body);
  Require(collection.at("revision").get_number() >= 0,
          "trajectory collection should include the current revision");
  const auto trajectories = collection.at("items").get_array();
  Require(trajectories.size() == kExpectedTrajectoryCount,
          "trajectory collection should include the active trajectories");
  Require(trajectories.front().at("uuid").get_string() == kTestTrajectoryUuid,
          "trajectory collection should preserve trajectory identity");

  const auto [missingStatus, missingBody] =
      fixture.ListTrajectories("missing-project");
  Require(missingStatus == kHttpNotFound,
          "unknown project collection should return HTTP 404");
  Require(Json::parse_or_throw(missingBody).at("code").get_string() ==
              "RESOURCE_NOT_FOUND",
          "unknown project collection should report RESOURCE_NOT_FOUND");
}

void TestTrajectorySubresourceReads(const std::filesystem::path& executable) {
  ApiTestFixture fixture{executable};
  auto events = fixture.ConnectEvents();
  Require(static_cast<bool>(events), "event WebSocket should connect");
  const auto [openStatus, openBody] = fixture.OpenPath(fixture.ProjectPath());
  const auto openOperationId =
      AcceptedOperationId(openStatus, openBody, "open");
  WaitForSessionOperation(events.Get(), openOperationId, "open", "succeeded");

  const auto [waypointStatus, waypointBody] = fixture.GetTrajectorySubresource(
      kTestProjectUuid, kTestTrajectoryUuid, "waypoints");
  Require(waypointStatus == kHttpOk,
          "waypoint collection should return HTTP 200");
  const auto waypoints =
      Json::parse_or_throw(waypointBody).at("items").get_array();
  Require(waypoints.size() == kExpectedSingleItemCollectionCount,
          "waypoint collection should contain the trajectory waypoint");
  const auto [singleWaypointStatus, singleWaypointBody] =
      fixture.GetTrajectorySubresourceItem(kTestProjectUuid,
                                           kTestTrajectoryUuid, "waypoints",
                                           kTestWaypointUuid);
  Require(singleWaypointStatus == kHttpOk,
          "existing waypoint should return HTTP 200");
  Require(Json::parse_or_throw(singleWaypointBody).at("uuid").get_string() ==
              kTestWaypointUuid,
          "waypoint response should preserve the requested UUID");

  const auto [constraintStatus, constraintBody] =
      fixture.GetTrajectorySubresource(kTestProjectUuid, kTestTrajectoryUuid,
                                       "constraints");
  Require(constraintStatus == kHttpOk,
          "constraint collection should return HTTP 200");
  Require(Json::parse_or_throw(constraintBody).at("items").get_array().size() ==
              kExpectedSingleItemCollectionCount,
          "constraint collection should contain the trajectory constraint");
  const auto [singleConstraintStatus, singleConstraintBody] =
      fixture.GetTrajectorySubresourceItem(kTestProjectUuid,
                                           kTestTrajectoryUuid, "constraints",
                                           kTestConstraintUuid);
  Require(singleConstraintStatus == kHttpOk,
          "existing constraint should return HTTP 200");
  Require(Json::parse_or_throw(singleConstraintBody).at("uuid").get_string() ==
              kTestConstraintUuid,
          "constraint response should preserve the requested UUID");

  const auto [eventStatus, eventBody] = fixture.GetTrajectorySubresource(
      kTestProjectUuid, kTestTrajectoryUuid, "event-markers");
  Require(eventStatus == kHttpOk,
          "event-marker collection should return HTTP 200");
  Require(Json::parse_or_throw(eventBody).at("items").get_array().size() ==
              kNoEvents,
          "event-marker collection should be empty when none are configured");
  const auto [missingEventStatus, missingEventBody] =
      fixture.GetTrajectorySubresourceItem(kTestProjectUuid,
                                           kTestTrajectoryUuid, "event-markers",
                                           "missing-event");
  Require(missingEventStatus == kHttpNotFound,
          "unknown event marker should return HTTP 404");
  Require(Json::parse_or_throw(missingEventBody).at("code").get_string() ==
              "RESOURCE_NOT_FOUND",
          "unknown event marker should report RESOURCE_NOT_FOUND");
}

void TestSaveRestoresPersistedProjectFiles(
    const std::filesystem::path& executable) {
  ApiTestFixture fixture{executable};
  auto events = fixture.ConnectEvents();
  Require(static_cast<bool>(events), "event WebSocket should connect");
  const auto [openStatus, openBody] = fixture.OpenPicker();
  const auto openOperationId =
      AcceptedOperationId(openStatus, openBody, "open");
  WaitForSessionOperation(events.Get(), openOperationId, "open", "succeeded");

  {
    std::ofstream corruptProject{fixture.ProjectPath(),
                                 std::ios::binary | std::ios::trunc};
    std::ofstream corruptTrajectory{fixture.TrajectoryPath(),
                                    std::ios::binary | std::ios::trunc};
    corruptProject << "corrupt project";
    corruptTrajectory << "corrupt trajectory";
    Require(corruptProject.good() && corruptTrajectory.good(),
            "could not prepare save test files");
  }

  const auto [saveStatus, saveBody] = fixture.Save();
  const auto saveOperationId =
      AcceptedOperationId(saveStatus, saveBody, "save");
  WaitForSessionOperation(events.Get(), saveOperationId, "save", "succeeded");
  Require(Json::parse_or_throw(ReadFileContents(fixture.ProjectPath()))
                  .at("uuid")
                  .get_string() == kTestProjectUuid,
          "save should restore the project document");
  Require(Json::parse_or_throw(ReadFileContents(fixture.TrajectoryPath()))
                  .at("uuid")
                  .get_string() == kTestTrajectoryUuid,
          "save should restore the trajectory document");
}

void TestProjectAndTrajectoryPatches(const std::filesystem::path& executable) {
  ApiTestFixture fixture{executable};
  auto events = fixture.ConnectEvents();
  Require(static_cast<bool>(events), "event WebSocket should connect");
  const auto [openStatus, openBody] = fixture.OpenPath(fixture.ProjectPath());
  const auto openOperationId =
      AcceptedOperationId(openStatus, openBody, "open");
  WaitForSessionOperation(events.Get(), openOperationId, "open", "succeeded");

  const auto [initialSnapshotStatus, initialSnapshotBody] = fixture.Snapshot();
  Require(initialSnapshotStatus == kHttpOk,
          "snapshot should be available before patching");
  const auto initialRevision = static_cast<std::uint64_t>(
      Json::parse_or_throw(initialSnapshotBody).at("revision").get_number());
  const auto projectPatch = Json::object("name", "Updated project");
  const auto [projectPatchStatus, projectPatchBody] =
      fixture.PatchProject(kTestProjectUuid, projectPatch, initialRevision,
                           kProjectPatchIdempotencyKey);
  Require(projectPatchStatus == kHttpOk,
          "project patch should return HTTP 200");
  const auto projectPatchResult = Json::parse_or_throw(projectPatchBody);
  const auto projectRevision = static_cast<std::uint64_t>(
      projectPatchResult.at("revision").get_number());
  Require(projectRevision == initialRevision + 1,
          "project patch should advance the document revision");
  WaitForDocumentChanged(events.Get(), projectRevision);
  const auto [projectRetryStatus, projectRetryBody] =
      fixture.PatchProject(kTestProjectUuid, projectPatch, initialRevision,
                           kProjectPatchIdempotencyKey);
  Require(projectRetryStatus == projectPatchStatus &&
              projectRetryBody == projectPatchBody,
          "project patch retry should replay its original response");
  const auto [stalePatchStatus, stalePatchBody] = fixture.PatchProject(
      kTestProjectUuid, Json::object("name", "Stale project"), initialRevision,
      kStaleUndoIdempotencyKey);
  Require(stalePatchStatus == kHttpConflict,
          "project patch with a stale revision should return HTTP 409");
  Require(Json::parse_or_throw(stalePatchBody).at("code").get_string() ==
              "REVISION_CONFLICT",
          "stale project patch should report REVISION_CONFLICT");
  const auto [invalidPatchStatus, invalidPatchBody] =
      fixture.PatchProject(kTestProjectUuid, Json::object("uuid", "changed"),
                           projectRevision, kFailedPatchIdempotencyKey);
  Require(invalidPatchStatus == kHttpBadRequest,
          "project patch should reject immutable fields");
  Require(Json::parse_or_throw(invalidPatchBody).at("code").get_string() ==
              "VALIDATION_ERROR",
          "invalid project patch should report VALIDATION_ERROR");
  const auto [projectStatus, projectBody] =
      fixture.GetProject(kTestProjectUuid);
  Require(projectStatus == kHttpOk &&
              Json::parse_or_throw(projectBody).at("name").get_string() ==
                  "Updated project",
          "project patch should publish and persist the updated name");

  const auto [trajectoryPatchStatus, trajectoryPatchBody] =
      fixture.PatchTrajectory(kTestProjectUuid, kTestTrajectoryUuid,
                              Json::object("name", "Updated trajectory"),
                              projectRevision, kTrajectoryPatchIdempotencyKey);
  Require(trajectoryPatchStatus == kHttpOk,
          "trajectory patch should return HTTP 200");
  const auto trajectoryRevision = static_cast<std::uint64_t>(
      Json::parse_or_throw(trajectoryPatchBody).at("revision").get_number());
  WaitForDocumentChanged(events.Get(), trajectoryRevision);
  const auto [trajectoryStatus, trajectoryBody] =
      fixture.GetTrajectory(kTestProjectUuid, kTestTrajectoryUuid);
  Require(trajectoryStatus == kHttpOk &&
              Json::parse_or_throw(trajectoryBody).at("name").get_string() ==
                  "Updated trajectory",
          "trajectory patch should publish and persist the updated name");

  std::filesystem::remove(fixture.TrajectoryPath());
  std::filesystem::create_directory(fixture.TrajectoryPath());
  const auto [failedPatchStatus, failedPatchBody] =
      fixture.PatchTrajectory(kTestProjectUuid, kTestTrajectoryUuid,
                              Json::object("name", "Pending trajectory"),
                              trajectoryRevision, kFailedPatchIdempotencyKey);
  Require(failedPatchStatus == 500,
          "trajectory patch should fail when persistence fails");
  Require(Json::parse_or_throw(failedPatchBody).at("code").get_string() ==
              "SAVE_FAILED",
          "failed trajectory patch should report SAVE_FAILED");
  const auto [unpublishedStatus, unpublishedBody] =
      fixture.GetTrajectory(kTestProjectUuid, kTestTrajectoryUuid);
  Require(unpublishedStatus == kHttpOk &&
              Json::parse_or_throw(unpublishedBody).at("name").get_string() ==
                  "Updated trajectory",
          "failed trajectory patch should not publish its candidate");

  std::filesystem::remove(fixture.TrajectoryPath());
  const auto [recoverySaveStatus, recoverySaveBody] = fixture.Save();
  const auto recoverySaveOperationId =
      AcceptedOperationId(recoverySaveStatus, recoverySaveBody, "save");
  WaitForSessionOperation(events.Get(), recoverySaveOperationId, "save",
                          "succeeded");
  const auto [recoveredStatus, recoveredBody] =
      fixture.GetTrajectory(kTestProjectUuid, kTestTrajectoryUuid);
  Require(recoveredStatus == kHttpOk &&
              Json::parse_or_throw(recoveredBody).at("name").get_string() ==
                  "Pending trajectory",
          "successful save should publish the persisted pending patch");
}

void TestWaypointConstraintAndEventMarkerMutations(
    const std::filesystem::path& executable) {
  ApiTestFixture fixture{executable};
  auto events = fixture.ConnectEvents();
  Require(static_cast<bool>(events), "event WebSocket should connect");
  const auto [openStatus, openBody] = fixture.OpenPath(fixture.ProjectPath());
  const auto openOperationId =
      AcceptedOperationId(openStatus, openBody, "open");
  WaitForSessionOperation(events.Get(), openOperationId, "open", "succeeded");

  const auto revisionFromSnapshot = [&fixture] {
    const auto [status, body] = fixture.Snapshot();
    Require(status == kHttpOk, "snapshot should be available");
    return static_cast<std::uint64_t>(
        Json::parse_or_throw(body).at("revision").get_number());
  };
  const auto revisionFromMutation = [](const std::string& body) {
    return static_cast<std::uint64_t>(
        Json::parse_or_throw(body).at("revision").get_number());
  };

  auto snapshot = Json::parse_or_throw(fixture.Snapshot().second);
  auto waypoint = snapshot.at("trajectories")
                      .get_array()
                      .front()
                      .at("params")
                      .at("waypoints")
                      .get_array()
                      .front();
  waypoint["uuid"] = kCreatedWaypointUuid;
  const auto createWaypointBaseRevision = revisionFromSnapshot();
  auto [status, body] = fixture.MutateSubresource(
      "POST", "waypoints", std::nullopt, waypoint.to_string(),
      "application/json", createWaypointBaseRevision,
      kCreateWaypointIdempotencyKey);
  Require(status == 201, "waypoint creation should return HTTP 201");
  const auto createdWaypointRevision = revisionFromMutation(body);
  const auto [waypointRetryStatus, waypointRetryBody] =
      fixture.MutateSubresource("POST", "waypoints", std::nullopt,
                                waypoint.to_string(), "application/json",
                                createWaypointBaseRevision,
                                kCreateWaypointIdempotencyKey);
  Require(waypointRetryStatus == status && waypointRetryBody == body,
          "retrying waypoint creation should replay its original response");
  auto [waypointPatchStatus, waypointPatchBody] = fixture.MutateSubresource(
      "PATCH", "waypoints", kCreatedWaypointUuid,
      Json::object("intervals", 7).to_string(), "application/merge-patch+json",
      createdWaypointRevision, kPatchWaypointIdempotencyKey);
  Require(waypointPatchStatus == kHttpOk,
          "waypoint update should return HTTP 200");
  auto waypointRevision = revisionFromMutation(waypointPatchBody);
  auto [staleWaypointStatus, staleWaypointBody] = fixture.MutateSubresource(
      "PATCH", "waypoints", kCreatedWaypointUuid,
      Json::object("intervals", 8).to_string(), "application/merge-patch+json",
      createdWaypointRevision, kFailedPatchIdempotencyKey);
  Require(staleWaypointStatus == kHttpConflict &&
              Json::parse_or_throw(staleWaypointBody).at("code").get_string() ==
                  "REVISION_CONFLICT",
          "stale waypoint updates should be rejected");
  auto [waypointDeleteStatus, waypointDeleteBody] = fixture.MutateSubresource(
      "DELETE", "waypoints", kCreatedWaypointUuid, {}, {}, waypointRevision,
      kDeleteWaypointIdempotencyKey);
  Require(waypointDeleteStatus == kHttpOk,
          "waypoint deletion should return HTTP 200");
  waypointRevision = revisionFromMutation(waypointDeleteBody);
  auto waypointCollection = Json::parse_or_throw(
      fixture
          .GetTrajectorySubresource(kTestProjectUuid, kTestTrajectoryUuid,
                                    "waypoints")
          .second);
  Require(waypointCollection.at("items").get_array().size() ==
              kExpectedSingleItemCollectionCount,
          "waypoint deletion should remove only the selected waypoint");

  snapshot = Json::parse_or_throw(fixture.Snapshot().second);
  auto constraint = snapshot.at("trajectories")
                        .get_array()
                        .front()
                        .at("params")
                        .at("constraints")
                        .get_array()
                        .front();
  constraint["uuid"] = kCreatedConstraintUuid;
  auto [constraintCreateStatus, constraintCreateBody] =
      fixture.MutateSubresource("POST", "constraints", std::nullopt,
                                constraint.to_string(), "application/json",
                                waypointRevision,
                                kCreateConstraintIdempotencyKey);
  Require(constraintCreateStatus == 201,
          "constraint creation should return HTTP 201");
  const auto constraintRevision = revisionFromMutation(constraintCreateBody);
  auto [constraintPatchStatus, constraintPatchBody] = fixture.MutateSubresource(
      "PATCH", "constraints", kCreatedConstraintUuid,
      Json::object("enabled", false).to_string(),
      "application/merge-patch+json", constraintRevision,
      kPatchConstraintIdempotencyKey);
  Require(constraintPatchStatus == kHttpOk,
          "constraint update should return HTTP 200");
  auto [constraintDeleteStatus, constraintDeleteBody] =
      fixture.MutateSubresource("DELETE", "constraints", kCreatedConstraintUuid,
                                {}, {},
                                revisionFromMutation(constraintPatchBody),
                                kDeleteConstraintIdempotencyKey);
  Require(constraintDeleteStatus == kHttpOk,
          "constraint deletion should return HTTP 200");

  const auto event = Json::object(
      "uuid", kCreatedEventUuid, "name", "Added event", "from",
      Json::object("target", "first", "targetTime", nullptr, "offset",
                   Json::object("exp", "0 s", "val", 0.0)));
  auto [eventCreateStatus, eventCreateBody] = fixture.MutateSubresource(
      "POST", "event-markers", std::nullopt, event.to_string(),
      "application/json", revisionFromMutation(constraintDeleteBody),
      kCreateEventIdempotencyKey);
  Require(eventCreateStatus == 201,
          "event-marker creation should return HTTP 201");
  auto [eventPatchStatus, eventPatchBody] = fixture.MutateSubresource(
      "PATCH", "event-markers", kCreatedEventUuid,
      Json::object("name", "Renamed event").to_string(),
      "application/merge-patch+json", revisionFromMutation(eventCreateBody),
      kPatchEventIdempotencyKey);
  Require(eventPatchStatus == kHttpOk,
          "event-marker update should return HTTP 200");
  const auto [eventGetStatus, eventGetBody] =
      fixture.GetTrajectorySubresourceItem(kTestProjectUuid,
                                           kTestTrajectoryUuid, "event-markers",
                                           kCreatedEventUuid);
  Require(eventGetStatus == kHttpOk &&
              Json::parse_or_throw(eventGetBody).at("name").get_string() ==
                  "Renamed event",
          "event-marker update should be visible in the document snapshot");
  const auto [eventDeleteStatus, eventDeleteBody] = fixture.MutateSubresource(
      "DELETE", "event-markers", kCreatedEventUuid, {}, {},
      revisionFromMutation(eventPatchBody), kDeleteEventIdempotencyKey);
  Require(eventDeleteStatus == kHttpOk,
          "event-marker deletion should return HTTP 200");
  Require(Json::parse_or_throw(eventDeleteBody).at("revision").get_number() >
              revisionFromMutation(eventPatchBody),
          "successful resource mutations should advance the document revision");
}

std::uint64_t GenerateOutput(ApiTestFixture& fixture,
                             const SocketHandle& events) {
  const auto [openStatus, openBody] = fixture.OpenPicker();
  const auto openOperationId =
      AcceptedOperationId(openStatus, openBody, "open");
  WaitForSessionOperation(events.Get(), openOperationId, "open", "succeeded");

  const auto [snapshotStatus, snapshotBody] = fixture.Snapshot();
  Require(snapshotStatus == kHttpOk, "snapshot should return HTTP 200");
  const auto revision = static_cast<std::uint64_t>(
      Json::parse_or_throw(snapshotBody).at("revision").get_number());
  const auto generationPath = "/api/v1/projects/" +
                              std::string{kTestProjectUuid} + "/trajectories/" +
                              std::string{kTestTrajectoryUuid} + "/generations";
  const auto requestHeaders =
      "If-Match: \"revision-" + std::to_string(revision) + "\"\r\n" +
      "Idempotency-Key: " + std::string{kIdempotencyKey} + "\r\n";
  const auto [generationStatus, generationBody] = HttpRequest(
      fixture.Port(), "POST", generationPath,
      Json::object("options", Json::object()).to_string(), requestHeaders);
  Require(generationStatus == kHttpAccepted,
          "generation should return HTTP 202");
  const auto operationId =
      Json::parse_or_throw(generationBody).at("operationId").get_string();

  bool receivedQueued = false;
  bool receivedRunning = false;
  bool receivedProgress = false;
  bool receivedSuccess = false;
  for (std::size_t attempt = 0;
       attempt < kMaximumGenerationEvents && !receivedSuccess; ++attempt) {
    const auto frame = ReadServerTextFrame(events.Get());
    Require(!frame.empty(), "expected a generation event");
    const auto event = Json::parse_or_throw(frame);
    if (!event.contains("operationId") ||
        event.at("operationId").get_string() != operationId) {
      continue;
    }
    const auto type = event.at("type").get_string();
    if (type == "generation.status") {
      const auto status = event.at("status").get_string();
      receivedQueued = receivedQueued || status == "queued";
      receivedRunning = receivedRunning || status == "running";
    } else if (type == "generation.progress") {
      receivedProgress =
          receivedProgress ||
          event.at("data").at("event").get_string() == "diagnostic";
    } else if (type == "generation.finished") {
      receivedSuccess = event.at("status").get_string() == "succeeded";
    }
  }
  Require(receivedQueued, "accepted generation should publish queued status");
  Require(receivedRunning, "generation should publish its running status");
  Require(receivedProgress,
          "generator diagnostic should be forwarded as a progress event");
  Require(receivedSuccess, "generation should publish its successful result");
  Require(!Json::parse_or_throw(ReadFileContents(fixture.TrajectoryPath()))
               .at("output")
               .is_null(),
          "generation should persist output before publishing success");
  const auto [generatedSnapshotStatus, generatedSnapshotBody] =
      fixture.Snapshot();
  Require(generatedSnapshotStatus == kHttpOk,
          "generated snapshot should return HTTP 200");
  return static_cast<std::uint64_t>(
      Json::parse_or_throw(generatedSnapshotBody).at("revision").get_number());
}

void TestGenerationPublishesStatusAndSavesOutput(
    const std::filesystem::path& executable) {
  ApiTestFixture fixture{executable};
  auto events = fixture.ConnectEvents();
  Require(static_cast<bool>(events), "event WebSocket should connect");
  GenerateOutput(fixture, events);
  const auto [saveStatus, saveBody] = fixture.Save();
  const auto saveOperationId =
      AcceptedOperationId(saveStatus, saveBody, "save");
  WaitForSessionOperation(events.Get(), saveOperationId, "save", "succeeded");
  Require(!Json::parse_or_throw(ReadFileContents(fixture.TrajectoryPath()))
               .at("output")
               .is_null(),
          "saved trajectory should contain generated output");
}

void TestHistoryAndPersistenceLifecycle(
    const std::filesystem::path& executable) {
  ApiTestFixture fixture{executable};
  auto events = fixture.ConnectEvents();
  Require(static_cast<bool>(events), "event WebSocket should connect");
  const auto generatedRevision = GenerateOutput(fixture, events);

  const auto [undoStatus, undoBody] =
      fixture.HistoryAction("undo", generatedRevision, kUndoIdempotencyKey);
  Require(undoStatus == kHttpOk, "undo should return HTTP 200");
  const auto undoResult = Json::parse_or_throw(undoBody);
  Require(undoResult.at("requestId").get_string() == kUndoIdempotencyKey,
          "undo should return its idempotency key as request ID");
  const auto undoRevision =
      static_cast<std::uint64_t>(undoResult.at("revision").get_number());
  const auto [undoneSnapshotStatus, undoneSnapshotBody] = fixture.Snapshot();
  Require(undoneSnapshotStatus == kHttpOk,
          "snapshot should be available after undo");
  Require(Json::parse_or_throw(undoneSnapshotBody)
              .at("trajectories")
              .get_array()
              .front()
              .at("output")
              .is_null(),
          "undo should remove generated output");
  Require(Json::parse_or_throw(ReadFileContents(fixture.TrajectoryPath()))
              .at("output")
              .is_null(),
          "undo should persist the document before acknowledging");

  const auto [retryStatus, retryBody] =
      fixture.HistoryAction("undo", generatedRevision, kUndoIdempotencyKey);
  Require(retryStatus == undoStatus && retryBody == undoBody,
          "retrying undo with the same key should replay the original result");
  const auto [staleUndoStatus, staleUndoBody] = fixture.HistoryAction(
      "undo", generatedRevision, kStaleUndoIdempotencyKey);
  Require(staleUndoStatus == kHttpConflict,
          "undo with a stale revision should return HTTP 409");
  Require(Json::parse_or_throw(staleUndoBody).at("code").get_string() ==
              "REVISION_CONFLICT",
          "stale undo should report REVISION_CONFLICT");
  const auto [reusedKeyStatus, reusedKeyBody] =
      fixture.HistoryAction("redo", undoRevision, kUndoIdempotencyKey);
  Require(reusedKeyStatus == kHttpConflict,
          "reusing an undo key for redo should return HTTP 409");
  Require(Json::parse_or_throw(reusedKeyBody).at("code").get_string() ==
              "IDEMPOTENCY_KEY_REUSE",
          "cross-operation key reuse should be rejected");

  const auto [redoStatus, redoBody] =
      fixture.HistoryAction("redo", undoRevision, kRedoIdempotencyKey);
  Require(redoStatus == kHttpOk, "redo should return HTTP 200");
  const auto redoResult = Json::parse_or_throw(redoBody);
  const auto redoRevision =
      static_cast<std::uint64_t>(redoResult.at("revision").get_number());
  const auto [redoneSnapshotStatus, redoneSnapshotBody] = fixture.Snapshot();
  Require(redoneSnapshotStatus == kHttpOk,
          "snapshot should be available after redo");
  Require(!Json::parse_or_throw(redoneSnapshotBody)
               .at("trajectories")
               .get_array()
               .front()
               .at("output")
               .is_null(),
          "redo should restore generated output");
  Require(Json::parse_or_throw(ReadFileContents(fixture.TrajectoryPath()))
              .at("output")
              .is_object(),
          "redo should persist restored output before acknowledging");

  std::filesystem::remove(fixture.ProjectPath());
  std::filesystem::create_directory(fixture.ProjectPath());
  const auto [failedUndoStatus, failedUndoBody] =
      fixture.HistoryAction("undo", redoRevision, kFailedUndoIdempotencyKey);
  Require(failedUndoStatus == 500,
          "undo should fail when persistence cannot write the project file");
  Require(Json::parse_or_throw(failedUndoBody).at("code").get_string() ==
              "SAVE_FAILED",
          "persistence failure should report SAVE_FAILED");
  Require(Json::parse_or_throw(fixture.Snapshot().second)
              .at("trajectories")
              .get_array()
              .front()
              .at("output")
              .is_object(),
          "failed undo must not publish the unpersisted document");

  std::filesystem::remove(fixture.ProjectPath());
  const auto [recoverySaveStatus, recoverySaveBody] = fixture.Save();
  const auto recoverySaveOperationId =
      AcceptedOperationId(recoverySaveStatus, recoverySaveBody, "save");
  WaitForSessionOperation(events.Get(), recoverySaveOperationId, "save",
                          "succeeded");
  Require(Json::parse_or_throw(fixture.Snapshot().second)
              .at("trajectories")
              .get_array()
              .front()
              .at("output")
              .is_null(),
          "successful save should publish the pending undo and unblock edits");
}

struct TestCase {
  std::string_view name;
  void (*run)(const std::filesystem::path&);
};

constexpr std::array<TestCase, 16> kTestCases{{
    {"picker open loads project snapshot", TestPickerOpenLoadsProjectSnapshot},
    {"picker cancellation leaves session closed",
     TestPickerCancellationLeavesSessionClosed},
    {"invalid project path is rejected", TestInvalidProjectPathIsRejected},
    {"new session completes asynchronously",
     TestNewSessionCompletesAsynchronously},
    {"close session clears snapshot", TestCloseSessionClearsSnapshot},
    {"close retains session when saving fails",
     TestCloseSessionRetainsSessionWhenSaveFails},
    {"session replacement retains current document when save fails",
     TestSessionReplacementRetainsActiveSessionWhenSaveFails},
    {"trajectory read returns document and rejects unknown IDs",
     TestTrajectoryReadReturnsDocumentAndRejectsUnknownIds},
    {"project read returns active document",
     TestProjectReadReturnsActiveDocument},
    {"trajectory collection returns revision and items",
     TestTrajectoryCollectionReturnsRevisionAndItems},
    {"trajectory subresource reads", TestTrajectorySubresourceReads},
    {"save restores persisted project files",
     TestSaveRestoresPersistedProjectFiles},
    {"project and trajectory patches persist before publication",
     TestProjectAndTrajectoryPatches},
    {"waypoint, constraint, and event-marker mutations",
     TestWaypointConstraintAndEventMarkerMutations},
    {"generation publishes status and saves output",
     TestGenerationPublishesStatusAndSavesOutput},
    {"history and persistence lifecycle", TestHistoryAndPersistenceLifecycle},
}};

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
               SEM_NOOPENFILEERRORBOX);
#endif
  if (argc > 1 && std::string_view{argv[1]} == "--chor") {
    return RunFakeGenerator(argc, argv);
  }
  std::size_t failures = 0;
  const auto executable = std::filesystem::absolute(argv[0]);
  for (const auto& test : kTestCases) {
    try {
      test.run(executable);
      std::cout << "PASS " << test.name << '\n';
    } catch (const std::exception& error) {
      ++failures;
      std::cerr << "FAIL " << test.name << ": " << error.what() << '\n';
    } catch (...) {
      ++failures;
      std::cerr << "FAIL " << test.name << ": unknown exception\n";
    }
  }
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
#include "choreo/document/application_api.hpp"

#include <array>
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
constexpr std::string_view kSnapshotPath = "/api/v1/snapshot";
constexpr std::string_view kSavePath = "/api/v1/session/save";
constexpr std::string_view kEventsPath = "/api/v1/events";
constexpr std::string_view kIdempotencyKey =
    "00000000-0000-4000-8000-000000009001";
constexpr std::string_view kNewSessionIdempotencyKey =
    "00000000-0000-4000-8000-000000009002";
constexpr int kHttpOk = 200;
constexpr int kHttpAccepted = 202;
constexpr int kHttpBadRequest = 400;
constexpr int kHttpConflict = 409;
constexpr std::size_t kExpectedTrajectoryCount = 1;
constexpr std::size_t kNoTrajectories = 0;
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
#else
    auto amount = send(socket, text.data(), text.size(), 0);
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
#else
    auto amount = recv(socket, buffer + read, size - read, 0);
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
      break;
    }
    response.append(buffer, static_cast<std::size_t>(amount));
  }
  return response;
}

std::string ReadServerTextFrame(Socket socket) {
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
  return payload;
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

std::pair<int, std::string> HttpRequest(std::uint16_t port,
                                        std::string_view method,
                                        std::string_view path,
                                        std::string_view body,
                                        std::string_view extraHeaders = {}) {
  SocketHandle socket{Connect(port)};
  if (!socket) {
    return {0, {}};
  }
  std::string request = std::string{method} + " " + std::string{path} +
                        " HTTP/1.1\r\nHost: 127.0.0.1:" + std::to_string(port) +
                        "\r\n";
  if (!body.empty()) {
    request += "Content-Type: application/json\r\nContent-Length: " +
               std::to_string(body.size()) + "\r\n";
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

  std::pair<int, std::string> Snapshot() const {
    return HttpRequest(Port(), "GET", kSnapshotPath, {});
  }

  std::pair<int, std::string> Save() const {
    return HttpRequest(Port(), "POST", kSavePath, "{}");
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
          "session request should return HTTP 202 Accepted");
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
  const auto body =
      Json::object("projectPath", newProjectPath.string(), "project", project)
          .to_string();
  const auto headers =
      "Idempotency-Key: " + std::string{kNewSessionIdempotencyKey} + "\r\n";
  const auto [status, responseBody] =
      HttpRequest(fixture.Port(), "POST", kSessionNewPath, body, headers);
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

void TestGenerationPublishesStatusAndSavesOutput(
    const std::filesystem::path& executable) {
  ApiTestFixture fixture{executable};
  auto events = fixture.ConnectEvents();
  Require(static_cast<bool>(events), "event WebSocket should connect");
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
    if (event.at("operationId").get_string() != operationId) {
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

  const auto [saveStatus, saveBody] = fixture.Save();
  const auto saveOperationId =
      AcceptedOperationId(saveStatus, saveBody, "save");
  WaitForSessionOperation(events.Get(), saveOperationId, "save", "succeeded");
  Require(!Json::parse_or_throw(ReadFileContents(fixture.TrajectoryPath()))
               .at("output")
               .is_null(),
          "saved trajectory should contain generated output");
}

struct TestCase {
  std::string_view name;
  void (*run)(const std::filesystem::path&);
};

constexpr std::array<TestCase, 6> kTestCases{{
    {"picker open loads project snapshot", TestPickerOpenLoadsProjectSnapshot},
    {"picker cancellation leaves session closed",
     TestPickerCancellationLeavesSessionClosed},
    {"invalid project path is rejected", TestInvalidProjectPathIsRejected},
    {"new session completes asynchronously",
     TestNewSessionCompletesAsynchronously},
    {"save restores persisted project files",
     TestSaveRestoresPersistedProjectFiles},
    {"generation publishes status and saves output",
     TestGenerationPublishesStatusAndSavesOutput},
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
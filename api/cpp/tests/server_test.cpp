#include "choreo/api/server.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace {

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

void SetReceiveTimeout(Socket socket) {
#ifdef _WIN32
  DWORD timeout = 3000;
  setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,
             reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
  timeval timeout{3, 0};
  setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
}

Socket ConnectToPort(std::uint16_t port) {
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

bool SendAll(Socket socket, std::string_view data) {
  while (!data.empty()) {
#ifdef _WIN32
    const auto written =
        send(socket, data.data(), static_cast<int>(data.size()), 0);
#else
    const auto written = send(socket, data.data(), data.size(), 0);
#endif
    if (written <= 0) {
      return false;
    }
    data.remove_prefix(static_cast<std::size_t>(written));
  }
  return true;
}

std::string ReadAll(Socket socket) {
  std::string response;
  char buffer[2048];
  while (true) {
#ifdef _WIN32
    const auto count = recv(socket, buffer, sizeof(buffer), 0);
#else
    const auto count = recv(socket, buffer, sizeof(buffer), 0);
#endif
    if (count <= 0) {
      break;
    }
    response.append(buffer, static_cast<std::size_t>(count));
  }
  return response;
}

std::string ReadHttpHeaders(Socket socket) {
  std::string response;
  char buffer[512];
  while (response.find("\r\n\r\n") == std::string::npos) {
    auto count = recv(socket, buffer, sizeof(buffer), 0);
    if (count <= 0) {
      break;
    }
    response.append(buffer, static_cast<std::size_t>(count));
  }
  return response;
}

std::string Exchange(std::uint16_t port, std::string_view request) {
  Socket socket = ConnectToPort(port);
  if (socket == kInvalidSocket) {
    return {};
  }
  const bool sent = SendAll(socket, request);
  auto response = sent ? ReadAll(socket) : std::string{};
  CloseSocket(socket);
  return response;
}

}  // namespace

int main() {
#ifdef _WIN32
  WSADATA winsockData;
  if (WSAStartup(MAKEWORD(2, 2), &winsockData) != 0) {
    return EXIT_FAILURE;
  }
#endif

  std::atomic_bool callbackCalled{false};
  std::atomic_bool originProbeCalled{false};
  std::atomic_bool websocketOpened{false};
  choreo::api::ServerOptions options;
  options.allowedOrigins.emplace_back("tauri://localhost");
  choreo::api::ApiServer server{options};
  const auto staticRoot =
      std::filesystem::temp_directory_path() /
      ("choreo-api-static-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(staticRoot / "assets");
  {
    std::ofstream index{staticRoot / "index.html", std::ios::binary};
    index << "<!doctype html><title>Choreo</title>";
    std::ofstream script{staticRoot / "assets" / "app.js", std::ios::binary};
    script << "console.log('Choreo');";
  }
  server.RegisterStaticDirectory(staticRoot);
  server.RegisterRoute("GET", "/api/v1/projects/{projectId}",
                       [&](const choreo::api::HttpRequest& request) {
                         callbackCalled =
                             request.pathParameters.at("projectId") ==
                                 "550e8400-e29b-41d4-a716-446655440000" &&
                             request.Header("x-test") == "callback";
                         return choreo::api::HttpResponse{
                             200,
                             "application/json",
                             "{\"handled\":true,\"id\":\"" +
                                 request.pathParameters.at("projectId") + "\"}",
                             {}};
                       });
  server.RegisterRoute(
      "POST", "/api/v1/operations", [](const choreo::api::HttpRequest&) {
        return choreo::api::HttpResponse{
            202, "application/json", "{\"accepted\":true}", {}};
      });
  server.RegisterRoute(
      "POST", "/api/v1/origin-probe", [&](const choreo::api::HttpRequest&) {
        originProbeCalled = true;
        return choreo::api::HttpResponse{200, "application/json", "{}", {}};
      });
  server.RegisterWebSocketRoute(
      "/api/v1/events", choreo::api::WebSocketCallbacks{
                            [&](choreo::api::WebSocketId, std::string_view) {
                              websocketOpened = true;
                            },
                            {},
                            {}});

  try {
    server.Start();
  } catch (...) {
#ifdef _WIN32
    WSACleanup();
#endif
    return EXIT_FAILURE;
  }
  if (server.Port() == 0) {
    server.Stop();
#ifdef _WIN32
    WSACleanup();
#endif
    return EXIT_FAILURE;
  }

  const auto host = "127.0.0.1:" + std::to_string(server.Port());
  const auto origin = "http://" + host;
  const auto request =
      "GET /api/v1/projects/550e8400-e29b-41d4-a716-446655440000 HTTP/1.1\r\n"
      "Host: " +
      host + "\r\nOrigin: " + origin +
      "\r\nX-Test: callback\r\nConnection: close\r\n\r\n";
  const auto response = Exchange(server.Port(), request);
  const auto invalidHostResponse = Exchange(
      server.Port(),
      "GET /api/v1/projects/550e8400-e29b-41d4-a716-446655440000 HTTP/1.1\r\n"
      "Host: attacker.example:" +
          std::to_string(server.Port()) + "\r\nConnection: close\r\n\r\n");
  const auto invalidOriginResponse = Exchange(
      server.Port(), "POST /api/v1/origin-probe HTTP/1.1\r\nHost: " + host +
                         "\r\nOrigin: https://attacker.example\r\n"
                         "Connection: close\r\n\r\n");
  const bool invalidOriginDidNotReachHandler = !originProbeCalled;
  const auto allowedOriginResponse = Exchange(
      server.Port(), "POST /api/v1/origin-probe HTTP/1.1\r\nHost: " + host +
                         "\r\nOrigin: tauri://localhost\r\n"
                         "Connection: close\r\n\r\n");
  const auto preflightResponse = Exchange(
      server.Port(), "OPTIONS /api/v1/origin-probe HTTP/1.1\r\nHost: " + host +
                         "\r\nOrigin: tauri://localhost\r\n"
                         "Access-Control-Request-Method: POST\r\n"
                         "Connection: close\r\n\r\n");
  const auto operationResponse = Exchange(
      server.Port(), "POST /api/v1/operations HTTP/1.1\r\nHost: " + host +
                         "\r\nConnection: close\r\n\r\n");
  const auto staticIndexResponse =
      Exchange(server.Port(), "GET / HTTP/1.1\r\nHost: " + host +
                                  "\r\nConnection: close\r\n\r\n");
  const auto staticAssetResponse = Exchange(
      server.Port(), "GET /assets/app.js?v=1 HTTP/1.1\r\nHost: " + host +
                         "\r\nConnection: close\r\n\r\n");
  const auto spaFallbackResponse =
      Exchange(server.Port(), "GET /editor/path HTTP/1.1\r\nHost: " + host +
                                  "\r\nConnection: close\r\n\r\n");
  const auto missingAssetResponse = Exchange(
      server.Port(), "GET /assets/missing.js HTTP/1.1\r\nHost: " + host +
                         "\r\nConnection: close\r\n\r\n");
  const auto apiFallbackResponse =
      Exchange(server.Port(), "GET /api/v1/unknown HTTP/1.1\r\nHost: " + host +
                                  "\r\nConnection: close\r\n\r\n");
  const auto traversalResponse = Exchange(
      server.Port(), "GET /%2e%2e/secret.txt HTTP/1.1\r\nHost: " + host +
                         "\r\nConnection: close\r\n\r\n");
  const auto malformedPathResponse =
      Exchange(server.Port(), "GET /assets/%GG.js HTTP/1.1\r\nHost: " + host +
                                  "\r\nConnection: close\r\n\r\n");
  const auto staticPostResponse =
      Exchange(server.Port(), "POST /assets/app.js HTTP/1.1\r\nHost: " + host +
                                  "\r\nConnection: close\r\n\r\n");

  Socket websocket = ConnectToPort(server.Port());
  if (websocket == kInvalidSocket) {
    server.Stop();
#ifdef _WIN32
    WSACleanup();
#endif
    return EXIT_FAILURE;
  }
  const auto invalidHandshake =
      "GET /api/v1/events HTTP/1.1\r\n"
      "Host: attacker.example:" +
      std::to_string(server.Port()) +
      "\r\nOrigin: http://attacker.example:" + std::to_string(server.Port()) +
      "\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
      "Sec-WebSocket-Version: 13\r\n\r\n";
  const bool invalidHandshakeSent = SendAll(websocket, invalidHandshake);
  const auto invalidHandshakeResponse = ReadHttpHeaders(websocket);
  CloseSocket(websocket);

  websocket = ConnectToPort(server.Port());
  if (websocket == kInvalidSocket) {
    server.Stop();
#ifdef _WIN32
    WSACleanup();
#endif
    return EXIT_FAILURE;
  }
  const auto allowedHandshake =
      "GET /api/v1/events HTTP/1.1\r\n"
      "Host: " +
      host +
      "\r\nOrigin: tauri://localhost\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
      "Sec-WebSocket-Version: 13\r\n\r\n";
  const bool handshakeSent = SendAll(websocket, allowedHandshake);
  const auto handshakeResponse = ReadHttpHeaders(websocket);
  CloseSocket(websocket);
  server.Stop();
  std::filesystem::remove_all(staticRoot);

  choreo::api::ServerOptions invalidBindOptions;
  invalidBindOptions.bindAddress = "0.0.0.0";
  choreo::api::ApiServer invalidBindServer{invalidBindOptions};
  bool invalidBindRejected = false;
  try {
    invalidBindServer.Start();
  } catch (const std::invalid_argument&) {
    invalidBindRejected = true;
  }
#ifdef _WIN32
  WSACleanup();
#endif

  return callbackCalled && originProbeCalled &&
                 invalidOriginDidNotReachHandler && invalidBindRejected &&
                 invalidHandshakeSent && handshakeSent && websocketOpened &&
                 response.find("HTTP/1.1 200 OK") != std::string::npos &&
                 response.find("\"handled\":true") != std::string::npos &&
                 response.find("Access-Control-Allow-Origin: " + origin) !=
                     std::string::npos &&
                 invalidHostResponse.find("HTTP/1.1 403 Forbidden") !=
                     std::string::npos &&
                 invalidOriginResponse.find("HTTP/1.1 403 Forbidden") !=
                     std::string::npos &&
                 invalidOriginResponse.find("ORIGIN_NOT_ALLOWED") !=
                     std::string::npos &&
                 allowedOriginResponse.find("HTTP/1.1 200 OK") !=
                     std::string::npos &&
                 allowedOriginResponse.find(
                     "Access-Control-Allow-Origin: tauri://localhost") !=
                     std::string::npos &&
                 preflightResponse.find("HTTP/1.1 204") != std::string::npos &&
                 preflightResponse.find(
                     "Access-Control-Allow-Origin: tauri://localhost") !=
                     std::string::npos &&
                 operationResponse.find("HTTP/1.1 202 Accepted") !=
                     std::string::npos &&
                 operationResponse.find("\"accepted\":true") !=
                     std::string::npos &&
                 staticIndexResponse.find("HTTP/1.1 200 OK") !=
                     std::string::npos &&
                 staticIndexResponse.find(
                     "Content-Type: text/html; charset=utf-8") !=
                     std::string::npos &&
                 staticIndexResponse.find("<title>Choreo</title>") !=
                     std::string::npos &&
                 staticAssetResponse.find(
                     "Content-Type: text/javascript; charset=utf-8") !=
                     std::string::npos &&
                 staticAssetResponse.find("console.log('Choreo')") !=
                     std::string::npos &&
                 spaFallbackResponse.find("<title>Choreo</title>") !=
                     std::string::npos &&
                 missingAssetResponse.find("HTTP/1.1 404 Not Found") !=
                     std::string::npos &&
                 apiFallbackResponse.find("HTTP/1.1 404 Not Found") !=
                     std::string::npos &&
                 traversalResponse.find("HTTP/1.1 400 Bad Request") !=
                     std::string::npos &&
                 malformedPathResponse.find("HTTP/1.1 400 Bad Request") !=
                     std::string::npos &&
                 staticPostResponse.find("HTTP/1.1 405 Method Not Allowed") !=
                     std::string::npos &&
                 invalidHandshakeResponse.find("101 Switching Protocols") ==
                     std::string::npos &&
                 handshakeResponse.find("101 Switching Protocols") !=
                     std::string::npos
             ? EXIT_SUCCESS
             : EXIT_FAILURE;
}

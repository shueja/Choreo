#include "choreo/api/server.hpp"

#include <atomic>
#include <cstdlib>
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

}  // namespace

int main() {
#ifdef _WIN32
  WSADATA winsockData;
  if (WSAStartup(MAKEWORD(2, 2), &winsockData) != 0) {
    return EXIT_FAILURE;
  }
#endif

  std::atomic_bool callbackCalled{false};
  std::atomic_bool websocketOpened{false};
  choreo::api::ApiServer server;
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

  Socket socket = ConnectToPort(server.Port());
  if (socket == kInvalidSocket) {
    server.Stop();
#ifdef _WIN32
    WSACleanup();
#endif
    return EXIT_FAILURE;
  }
  constexpr std::string_view request =
      "GET /api/v1/projects/550e8400-e29b-41d4-a716-446655440000 HTTP/1.1\r\n"
      "Host: 127.0.0.1\r\n"
      "X-Test: callback\r\n"
      "Connection: close\r\n\r\n";
  const bool sent = SendAll(socket, request);
  const auto response = ReadAll(socket);
  CloseSocket(socket);

  Socket operationSocket = ConnectToPort(server.Port());
  if (operationSocket == kInvalidSocket) {
    server.Stop();
#ifdef _WIN32
    WSACleanup();
#endif
    return EXIT_FAILURE;
  }
  constexpr std::string_view operationRequest =
      "POST /api/v1/operations HTTP/1.1\r\n"
      "Host: 127.0.0.1\r\n"
      "Connection: close\r\n\r\n";
  const bool operationSent = SendAll(operationSocket, operationRequest);
  const auto operationResponse = ReadAll(operationSocket);
  CloseSocket(operationSocket);

  Socket websocket = ConnectToPort(server.Port());
  if (websocket == kInvalidSocket) {
    server.Stop();
#ifdef _WIN32
    WSACleanup();
#endif
    return EXIT_FAILURE;
  }
  const auto origin = "http://127.0.0.1:" + std::to_string(server.Port());
  const auto handshake =
      "GET /api/v1/events HTTP/1.1\r\n"
      "Host: 127.0.0.1:" +
      std::to_string(server.Port()) + "\r\n" + "Origin: " + origin +
      "\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
      "Sec-WebSocket-Version: 13\r\n\r\n";
  const bool handshakeSent = SendAll(websocket, handshake);
  const auto handshakeResponse = ReadHttpHeaders(websocket);
  CloseSocket(websocket);
  server.Stop();
#ifdef _WIN32
  WSACleanup();
#endif

  return sent && callbackCalled && operationSent && handshakeSent &&
                 websocketOpened &&
                 response.find("HTTP/1.1 200 OK") != std::string::npos &&
                 response.find("\"handled\":true") != std::string::npos &&
                 operationResponse.find("HTTP/1.1 202 Accepted") !=
                     std::string::npos &&
                 operationResponse.find("\"accepted\":true") !=
                     std::string::npos &&
                 handshakeResponse.find("101 Switching Protocols") !=
                     std::string::npos
             ? EXIT_SUCCESS
             : EXIT_FAILURE;
}

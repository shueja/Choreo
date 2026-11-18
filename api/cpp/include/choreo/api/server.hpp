#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace choreo::api {

using WebSocketId = std::uint64_t;

/**
 * One parsed HTTP request. Header names are lower-case; path parameters are
 * populated from registered route templates such as /projects/{projectId}.
 */
struct HttpRequest {
  std::string method;
  std::string target;
  std::string path;
  std::string query;
  std::string body;
  std::map<std::string, std::string, std::less<>> headers;
  std::map<std::string, std::string, std::less<>> pathParameters;

  std::string_view Header(std::string_view name) const;
};

struct HttpResponse {
  int status = 200;
  std::string contentType = "application/json";
  std::string body;
  std::map<std::string, std::string, std::less<>> headers;
};

/** Callbacks for one WebSocket route. Text callbacks receive complete messages.
 */
struct WebSocketCallbacks {
  std::function<void(WebSocketId, std::string_view)> onOpen;
  std::function<void(WebSocketId, std::string_view)> onText;
  std::function<void(WebSocketId, std::uint16_t, std::string_view)> onClose;
};

/** Configuration for a loopback-first HTTP/WebSocket listener. */
struct ServerOptions {
  std::string bindAddress = "127.0.0.1";
  std::uint16_t port = 0;
  std::size_t maxBodyBytes = 1024 * 1024;
  std::vector<std::string> allowedOrigins;
};

/**
 * Owns HTTP and WebSocket hosting while delegating request behavior to
 * registered callbacks. Register routes before Start(); handlers execute on
 * the server event-loop thread and should not block or call Start()/Stop().
 * SendText() and BroadcastText() may be called from other threads.
 */
class ApiServer {
 public:
  using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

  explicit ApiServer(ServerOptions options = {});
  ~ApiServer();

  ApiServer(const ApiServer&) = delete;
  ApiServer& operator=(const ApiServer&) = delete;
  ApiServer(ApiServer&&) = delete;
  ApiServer& operator=(ApiServer&&) = delete;

  void RegisterRoute(std::string method, std::string pathTemplate,
                     HttpHandler handler);
  void RegisterWebSocketRoute(std::string path, WebSocketCallbacks callbacks);

  void Start();
  void Stop();
  bool IsRunning() const noexcept;
  std::uint16_t Port() const noexcept;

  void SendText(WebSocketId connection, std::string message);
  void BroadcastText(std::string_view path, std::string message);

 private:
  struct Impl;
  std::shared_ptr<Impl> m_impl;
};

}  // namespace choreo::api
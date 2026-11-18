#include "choreo/api/server.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <exception>
#include <format>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include <llhttp.h>

#include "wpi/net/EventLoopRunner.hpp"
#include "wpi/net/HttpWebSocketServerConnection.hpp"
#include "wpi/net/uv/Buffer.hpp"
#include "wpi/net/uv/Tcp.hpp"
#include "wpi/util/fmt/raw_ostream.hpp"
#include "wpi/util/json.hpp"
#include "wpi/util/print.hpp"
#include "wpi/util/raw_ostream.hpp"

namespace choreo::api {
namespace {

std::string Lower(std::string_view value) {
  std::string result{value};
  std::transform(result.begin(), result.end(), result.begin(), [](char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  });
  return result;
}

std::string Upper(std::string_view value) {
  std::string result{value};
  std::transform(result.begin(), result.end(), result.begin(), [](char c) {
    return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  });
  return result;
}

std::string_view PathOnly(std::string_view target) {
  auto query = target.find('?');
  if (query != std::string_view::npos) {
    target = target.substr(0, query);
  }
  return target;
}

std::string_view ReasonPhrase(int status) {
  switch (status) {
    case 200:
      return "OK";
    case 201:
      return "Created";
    case 202:
      return "Accepted";
    case 204:
      return "No Content";
    case 400:
      return "Bad Request";
    case 403:
      return "Forbidden";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    case 409:
      return "Conflict";
    case 413:
      return "Content Too Large";
    case 500:
      return "Internal Server Error";
    case 503:
      return "Service Unavailable";
    default:
      return "Response";
  }
}

bool IsToken(std::string_view value) {
  if (value.empty()) {
    return false;
  }
  return std::all_of(value.begin(), value.end(), [](unsigned char c) {
    return std::isalnum(c) != 0 || c == '!' || c == '#' || c == '$' ||
           c == '%' || c == '&' || c == '\'' || c == '*' || c == '+' ||
           c == '-' || c == '.' || c == '^' || c == '_' || c == '`' ||
           c == '|' || c == '~';
  });
}

bool HasHeaderInjection(std::string_view value) {
  return value.find('\r') != std::string_view::npos ||
         value.find('\n') != std::string_view::npos;
}

std::vector<std::string_view> PathSegments(std::string_view path) {
  std::vector<std::string_view> result;
  if (path.empty() || path.front() != '/') {
    return result;
  }
  std::size_t start = 1;
  while (start < path.size()) {
    auto end = path.find('/', start);
    if (end == std::string_view::npos) {
      end = path.size();
    }
    result.emplace_back(path.substr(start, end - start));
    start = end + 1;
  }
  return result;
}

bool IsPathTemplateValid(std::string_view path) {
  if (path.empty() || path.front() != '/' || path.find('?') != path.npos) {
    return false;
  }
  for (auto segment : PathSegments(path)) {
    auto open = segment.find('{');
    auto close = segment.find('}');
    if (open == segment.npos && close == segment.npos) {
      continue;
    }
    if (open != 0 || close != segment.size() - 1 || close <= open + 1 ||
        segment.find('{', 1) != segment.npos ||
        segment.find('}', close + 1) != segment.npos) {
      return false;
    }
  }
  return true;
}

bool MatchPath(std::string_view route, std::string_view path,
               std::map<std::string, std::string, std::less<>>& parameters) {
  auto routeSegments = PathSegments(route);
  auto pathSegments = PathSegments(path);
  if (routeSegments.size() != pathSegments.size()) {
    return false;
  }
  for (std::size_t index = 0; index < routeSegments.size(); ++index) {
    auto routeSegment = routeSegments[index];
    auto pathSegment = pathSegments[index];
    if (routeSegment.size() > 2 && routeSegment.front() == '{' &&
        routeSegment.back() == '}') {
      if (pathSegment.empty()) {
        return false;
      }
      parameters.emplace(
          std::string{routeSegment.substr(1, routeSegment.size() - 2)},
          std::string{pathSegment});
    } else if (routeSegment != pathSegment) {
      return false;
    }
  }
  return true;
}

HttpResponse ErrorResponse(int status, std::string_view code,
                           std::string_view message) {
  HttpResponse response;
  response.status = status;
  response.body =
      wpi::util::json::object("code", code, "message", message).to_string();
  return response;
}

}  // namespace

std::string_view HttpRequest::Header(std::string_view name) const {
  auto it = headers.find(Lower(name));
  if (it == headers.end()) {
    return {};
  }
  return it->second;
}

struct ApiServer::Impl : public std::enable_shared_from_this<ApiServer::Impl> {
  struct Route {
    std::string method;
    std::string pathTemplate;
    HttpHandler handler;
  };

  struct WebSocketRoute {
    std::string path;
    WebSocketCallbacks callbacks;
  };

  struct WebSocketSession {
    std::string path;
    std::shared_ptr<wpi::net::WebSocket> socket;
    WebSocketCallbacks callbacks;
  };

  struct ClientConnection;

  explicit Impl(ServerOptions optionsIn) : options{std::move(optionsIn)} {}

  void RegisterRoute(std::string method, std::string pathTemplate,
                     HttpHandler handler) {
    std::scoped_lock lock{registrationMutex};
    if (running) {
      throw std::logic_error("routes must be registered before server start");
    }
    method = Upper(method);
    if (!IsToken(method) || !IsPathTemplateValid(pathTemplate) || !handler) {
      throw std::invalid_argument("invalid HTTP route registration");
    }
    auto duplicate =
        std::find_if(routes.begin(), routes.end(), [&](const Route& route) {
          return route.method == method && route.pathTemplate == pathTemplate;
        });
    if (duplicate != routes.end()) {
      throw std::invalid_argument("HTTP route is already registered");
    }
    routes.push_back(
        Route{std::move(method), std::move(pathTemplate), std::move(handler)});
  }

  void RegisterWebSocketRoute(std::string path, WebSocketCallbacks callbacks) {
    std::scoped_lock lock{registrationMutex};
    if (running) {
      throw std::logic_error("routes must be registered before server start");
    }
    if (!IsPathTemplateValid(path) || path.find('{') != path.npos) {
      throw std::invalid_argument("invalid WebSocket route");
    }
    auto duplicate = std::find_if(
        websocketRoutes.begin(), websocketRoutes.end(),
        [&](const WebSocketRoute& route) { return route.path == path; });
    if (duplicate != websocketRoutes.end()) {
      throw std::invalid_argument("WebSocket route is already registered");
    }
    websocketRoutes.push_back(
        WebSocketRoute{std::move(path), std::move(callbacks)});
  }

  void Start() {
    std::scoped_lock lifecycleLock{lifecycleMutex};
    std::scoped_lock registrationLock{registrationMutex};
    if (running) {
      throw std::logic_error("API server is already running");
    }

    std::string setupError;
    std::uint16_t boundPort = 0;
    runner.ExecSync([&](wpi::net::uv::Loop& loop) {
      listener = wpi::net::uv::Tcp::Create(loop);
      if (!listener) {
        setupError = "failed to create TCP listener";
        return;
      }
      auto errorConnection = listener->error.connect_connection(
          [&](wpi::net::uv::Error error) { setupError = error.str(); });
      listener->Bind(options.bindAddress, options.port);
      if (setupError.empty()) {
        listener->connection.connect([this] { AcceptConnection(); });
        listener->Listen();
      }
      if (!setupError.empty()) {
        listener->Close();
        listener.reset();
        return;
      }
      auto address = listener->GetSock();
      if (address.ss_family != AF_INET) {
        setupError = "API server must bind an IPv4 loopback address";
        listener->Close();
        listener.reset();
        return;
      }
      boundPort =
          ntohs(reinterpret_cast<const sockaddr_in*>(&address)->sin_port);
      if (boundPort == 0) {
        setupError = "failed to determine bound API port";
        listener->Close();
        listener.reset();
      }
    });
    if (!setupError.empty()) {
      throw std::runtime_error("failed to start API server: " + setupError);
    }
    port = boundPort;
    running = true;
  }

  void Stop() {
    std::scoped_lock lifecycleLock{lifecycleMutex};
    if (!running) {
      return;
    }
    runner.ExecSync([this](wpi::net::uv::Loop&) {
      if (listener) {
        listener->Close();
        listener.reset();
      }
      std::vector<std::shared_ptr<wpi::net::WebSocket>> sockets;
      sockets.reserve(websocketSessions.size());
      for (auto& [id, session] : websocketSessions) {
        sockets.push_back(session.socket);
      }
      websocketSessions.clear();
      for (const auto& socket : sockets) {
        socket->Close(1001, "server stopping");
      }
      for (auto& weakConnection : connections) {
        if (auto connection = weakConnection.lock()) {
          connection->Close();
        }
      }
      connections.clear();
    });
    port = 0;
    running = false;
  }

  void AcceptConnection() {
    if (!listener) {
      return;
    }
    auto stream = listener->Accept();
    if (!stream) {
      return;
    }
    stream->error.connect([stream](wpi::net::uv::Error) { stream->Close(); });
    auto connection = std::make_shared<ClientConnection>(
        stream, weak_from_this(), options.maxBodyBytes);
    stream->SetData(connection);
    connections.emplace_back(connection);
    std::erase_if(connections, [](const auto& weak) { return weak.expired(); });
  }

  HttpResponse Dispatch(HttpRequest request) {
    if (request.method == "OPTIONS") {
      if (!IsOriginAllowed(request.Header("origin"), request.Header("host"))) {
        return ErrorResponse(403, "ORIGIN_NOT_ALLOWED",
                             "Origin is not allowed");
      }
      HttpResponse response;
      response.status = 204;
      response.contentType = "text/plain";
      response.headers.emplace("Access-Control-Allow-Methods",
                               "GET, POST, PATCH, DELETE, OPTIONS");
      response.headers.emplace("Access-Control-Allow-Headers",
                               "Content-Type, If-Match, Idempotency-Key");
      return response;
    }

    const Route* pathMatch = nullptr;
    for (const auto& route : routes) {
      std::map<std::string, std::string, std::less<>> parameters;
      if (!MatchPath(route.pathTemplate, request.path, parameters)) {
        continue;
      }
      pathMatch = &route;
      if (route.method == request.method) {
        request.pathParameters = std::move(parameters);
        try {
          return route.handler(request);
        } catch (const std::exception&) {
          return ErrorResponse(500, "INTERNAL_ERROR", "Request handler failed");
        } catch (...) {
          return ErrorResponse(500, "INTERNAL_ERROR", "Request handler failed");
        }
      }
    }
    if (pathMatch) {
      HttpResponse response =
          ErrorResponse(405, "METHOD_NOT_ALLOWED", "Method is not allowed");
      std::set<std::string> methods;
      for (const auto& route : routes) {
        std::map<std::string, std::string, std::less<>> ignored;
        if (MatchPath(route.pathTemplate, request.path, ignored)) {
          methods.insert(route.method);
        }
      }
      std::string allow;
      for (const auto& method : methods) {
        if (!allow.empty()) {
          allow += ", ";
        }
        allow += method;
      }
      response.headers.emplace("Allow", std::move(allow));
      return response;
    }
    return ErrorResponse(404, "RESOURCE_NOT_FOUND", "Route was not found");
  }

  bool IsOriginAllowed(std::string_view origin, std::string_view host) const {
    if (origin.empty()) {
      return true;
    }
    if (HasHeaderInjection(origin)) {
      return false;
    }
    if (!host.empty() &&
        (origin == std::string{"http://"} + std::string{host} ||
         origin == std::string{"https://"} + std::string{host})) {
      return true;
    }
    return std::find(options.allowedOrigins.begin(),
                     options.allowedOrigins.end(),
                     origin) != options.allowedOrigins.end();
  }

  const WebSocketRoute* FindWebSocketRoute(std::string_view path) const {
    auto it = std::find_if(
        websocketRoutes.begin(), websocketRoutes.end(),
        [&](const WebSocketRoute& route) { return route.path == path; });
    return it == websocketRoutes.end() ? nullptr : &*it;
  }

  WebSocketId AddWebSocket(std::string path,
                           std::shared_ptr<wpi::net::WebSocket> socket,
                           WebSocketCallbacks callbacks) {
    auto id = nextWebSocketId.fetch_add(1);
    websocketSessions.emplace(
        id, WebSocketSession{std::move(path), std::move(socket),
                             std::move(callbacks)});
    return id;
  }

  void RemoveWebSocket(WebSocketId id) { websocketSessions.erase(id); }

  void SendText(WebSocketId id, std::string message) {
    auto it = websocketSessions.find(id);
    if (it == websocketSessions.end() || !it->second.socket->IsOpen()) {
      return;
    }
    std::array<wpi::net::uv::Buffer, 1> buffers{
        wpi::net::uv::Buffer::Dup(message)};
    it->second.socket->SendText(
        buffers, [](std::span<wpi::net::uv::Buffer> sent, wpi::net::uv::Error) {
          for (auto& buffer : sent) {
            buffer.Deallocate();
          }
        });
  }

  void BroadcastText(std::string_view path, std::string message) {
    std::vector<WebSocketId> ids;
    for (const auto& [id, session] : websocketSessions) {
      if (session.path == path) {
        ids.push_back(id);
      }
    }
    for (auto id : ids) {
      SendText(id, message);
    }
  }

  ServerOptions options;
  wpi::net::EventLoopRunner runner;
  std::shared_ptr<wpi::net::uv::Tcp> listener;
  std::vector<std::weak_ptr<ClientConnection>> connections;
  std::vector<Route> routes;
  std::vector<WebSocketRoute> websocketRoutes;
  std::map<WebSocketId, WebSocketSession> websocketSessions;
  std::mutex registrationMutex;
  std::mutex lifecycleMutex;
  std::atomic<bool> running{false};
  std::atomic<std::uint16_t> port{0};
  std::atomic<WebSocketId> nextWebSocketId{1};

  struct ClientConnection final
      : wpi::net::HttpWebSocketServerConnection<ClientConnection> {
    ClientConnection(std::shared_ptr<wpi::net::uv::Stream> stream,
                     std::weak_ptr<Impl> owner, std::size_t maxBodyBytes)
        : HttpWebSocketServerConnection{std::move(stream),
                                        std::initializer_list<
                                            std::string_view>{},
                                        wpi::net::uv::Timer::Time{10}},
          impl{std::move(owner)},
          bodyLimit{maxBodyBytes} {
      m_request.messageBegin.connect([this] {
        requestHeaders.clear();
        requestBody.clear();
        bodyTooLarge = false;
      });
      m_request.header.connect([this](std::string_view name,
                                      std::string_view value) {
        auto key = Lower(name);
        auto [it, inserted] = requestHeaders.try_emplace(std::move(key), value);
        if (!inserted) {
          it->second += ", ";
          it->second += value;
        }
      });
      m_request.body.connect([this](std::string_view chunk) {
        if (requestBody.size() > bodyLimit ||
            chunk.size() > bodyLimit - requestBody.size()) {
          bodyTooLarge = true;
          return;
        }
        requestBody.append(chunk);
      });
    }

    void Close() { m_stream.Close(); }

   protected:
    void ProcessRequest() override {
      auto owner = impl.lock();
      if (!owner) {
        SendResponse(503, "Service Unavailable", "text/plain",
                     "API server is stopping");
        return;
      }
      if (bodyTooLarge) {
        auto response =
            ErrorResponse(413, "PAYLOAD_TOO_LARGE",
                          "Request body exceeds the configured limit");
        Send(response);
        return;
      }

      HttpRequest request;
      request.method = llhttp_method_name(m_request.GetMethod());
      request.target = m_request.GetUrl();
      auto queryPosition = request.target.find('?');
      if (queryPosition == std::string::npos) {
        request.path = request.target;
      } else {
        request.path = request.target.substr(0, queryPosition);
        request.query = request.target.substr(queryPosition + 1);
      }
      request.body = requestBody;
      request.headers = requestHeaders;
      Send(owner->Dispatch(std::move(request)));
    }

    bool IsValidWsUpgrade(std::string_view protocol) override {
      auto owner = impl.lock();
      if (!owner) {
        m_stream.Close();
        return false;
      }
      websocketPath = std::string{PathOnly(m_request.GetUrl())};
      websocketTarget = std::string{m_request.GetUrl()};
      auto route = owner->FindWebSocketRoute(websocketPath);
      auto origin = requestHeaders.find("origin");
      auto host = requestHeaders.find("host");
      auto originValue = origin == requestHeaders.end()
                             ? std::string_view{}
                             : std::string_view{origin->second};
      auto hostValue = host == requestHeaders.end()
                           ? std::string_view{}
                           : std::string_view{host->second};
      if (!route || !owner->IsOriginAllowed(originValue, hostValue)) {
        m_stream.Close();
        return false;
      }
      websocketCallbacks = route->callbacks;
      return true;
    }

    void ProcessWsUpgrade() override {
      auto owner = impl.lock();
      if (!owner || !m_websocket) {
        m_stream.Close();
        return;
      }
      auto socket = m_websocket->shared_from_this();
      auto id = owner->AddWebSocket(websocketPath, socket, websocketCallbacks);
      auto callbacks = websocketCallbacks;
      std::weak_ptr<Impl> weakOwner = owner;
      auto target = websocketTarget;
      socket->open.connect([callbacks, id, target](std::string_view) {
        if (callbacks.onOpen) {
          try {
            callbacks.onOpen(id, target);
          } catch (...) {
          }
        }
      });
      socket->text.connect([callbacks, id](std::string_view text, bool final) {
        if (final && callbacks.onText) {
          try {
            callbacks.onText(id, text);
          } catch (...) {
          }
        }
      });
      socket->closed.connect([weakOwner, callbacks, id](
                                 std::uint16_t code, std::string_view reason) {
        if (auto owner = weakOwner.lock()) {
          owner->RemoveWebSocket(id);
        }
        if (callbacks.onClose) {
          try {
            callbacks.onClose(id, code, reason);
          } catch (...) {
          }
        }
      });
    }

    void BuildHeader(wpi::util::raw_ostream& os, int code,
                     std::string_view codeText, std::string_view contentType,
                     std::uint64_t contentLength,
                     std::string_view extra = {}) override {
      wpi::util::print(os, "HTTP/{}.{} {} {}\r\n", m_request.GetMajor(),
                       m_request.GetMinor(), code, codeText);
      if (contentLength == 0) {
        m_keepAlive = false;
      }
      if (!m_keepAlive) {
        os << "Connection: close\r\n";
      }
      BuildCommonHeaders(os);
      os << "Content-Type: " << contentType << "\r\n";
      if (contentLength != 0) {
        wpi::util::print(os, "Content-Length: {}\r\n", contentLength);
      }
      auto origin = requestHeaders.find("origin");
      auto host = requestHeaders.find("host");
      auto owner = impl.lock();
      if (owner && origin != requestHeaders.end() &&
          owner->IsOriginAllowed(origin->second,
                                 host == requestHeaders.end()
                                     ? std::string_view{}
                                     : std::string_view{host->second})) {
        os << "Access-Control-Allow-Origin: " << origin->second << "\r\n"
           << "Vary: Origin\r\n";
      }
      if (!extra.empty()) {
        os << extra;
      }
      os << "\r\n";
    }

   private:
    void Send(const HttpResponse& response) {
      if (response.status < 100 || response.status > 599 ||
          HasHeaderInjection(response.contentType)) {
        SendResponse(500, "Internal Server Error", "text/plain",
                     "Handler returned an invalid response");
        return;
      }
      std::string extraHeaders;
      for (const auto& [name, value] : response.headers) {
        if (!IsToken(name) || HasHeaderInjection(value) ||
            Lower(name) == "content-length" || Lower(name) == "connection") {
          SendResponse(500, "Internal Server Error", "text/plain",
                       "Handler returned an invalid response header");
          return;
        }
        extraHeaders += name;
        extraHeaders += ": ";
        extraHeaders += value;
        extraHeaders += "\r\n";
      }
      SendResponse(response.status, ReasonPhrase(response.status),
                   response.contentType, response.body, extraHeaders);
    }

    std::weak_ptr<Impl> impl;
    std::size_t bodyLimit;
    std::map<std::string, std::string, std::less<>> requestHeaders;
    std::string requestBody;
    std::string websocketPath;
    std::string websocketTarget;
    WebSocketCallbacks websocketCallbacks;
    bool bodyTooLarge = false;
  };
};

ApiServer::ApiServer(ServerOptions options)
    : m_impl{std::make_shared<Impl>(std::move(options))} {}

ApiServer::~ApiServer() {
  if (m_impl) {
    m_impl->Stop();
  }
}

void ApiServer::RegisterRoute(std::string method, std::string pathTemplate,
                              HttpHandler handler) {
  m_impl->RegisterRoute(std::move(method), std::move(pathTemplate),
                        std::move(handler));
}

void ApiServer::RegisterWebSocketRoute(std::string path,
                                       WebSocketCallbacks callbacks) {
  m_impl->RegisterWebSocketRoute(std::move(path), std::move(callbacks));
}

void ApiServer::Start() {
  m_impl->Start();
}

void ApiServer::Stop() {
  m_impl->Stop();
}

bool ApiServer::IsRunning() const noexcept {
  return m_impl->running;
}

std::uint16_t ApiServer::Port() const noexcept {
  return m_impl->port;
}

void ApiServer::SendText(WebSocketId connection, std::string message) {
  std::weak_ptr<Impl> weakImpl = m_impl;
  m_impl->runner.ExecAsync([weakImpl, connection, message = std::move(message)](
                               wpi::net::uv::Loop&) mutable {
    if (auto impl = weakImpl.lock()) {
      impl->SendText(connection, std::move(message));
    }
  });
}

void ApiServer::BroadcastText(std::string_view path, std::string message) {
  std::weak_ptr<Impl> weakImpl = m_impl;
  std::string route{path};
  m_impl->runner.ExecAsync(
      [weakImpl, route = std::move(route),
       message = std::move(message)](wpi::net::uv::Loop&) mutable {
        if (auto impl = weakImpl.lock()) {
          impl->BroadcastText(route, std::move(message));
        }
      });
}

}  // namespace choreo::api

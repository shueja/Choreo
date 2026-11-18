# Choreo API Server

`choreo::api::ApiServer` owns the local `wpinet` event loop, HTTP listener, HTTP/WebSocket connections, route dispatch, and shutdown. It does not contain project or trajectory business logic; callers register handlers before starting the server.

```cpp
choreo::api::ApiServer api;
api.RegisterRoute("GET", "/api/v1/snapshot", [](const auto& request) {
  return choreo::api::HttpResponse{200, "application/json", "{}", {}};
});
api.RegisterWebSocketRoute(
    "/api/v1/events",
    choreo::api::WebSocketCallbacks{
      .onOpen = [](choreo::api::WebSocketId id, std::string_view target) {},
        .onText = [](choreo::api::WebSocketId id, std::string_view message) {},
        .onClose = [](choreo::api::WebSocketId id, std::uint16_t code,
                      std::string_view reason) {}});
api.Start();
const auto port = api.Port();
```

HTTP route templates use `{name}` path parameters. Request header names are normalized to lower case. HTTP and WebSocket callbacks execute on the server event-loop thread, so they should be short and non-blocking; `SendText` and `BroadcastText` may be called from other threads. Register all routes before `Start()`. The default bind address is loopback and port `0` requests an available ephemeral port. The bind address must be a numeric IPv4 loopback address, and HTTP requests and WebSocket upgrades must send a `Host` header exactly matching the bound address and selected port. A supplied `Origin` must be same-origin or listed in `ServerOptions::allowedOrigins`; this is checked before dispatch for all HTTP requests and WebSocket upgrades. Requests without an `Origin` header are accepted for non-browser clients. Allowed cross-origin HTTP responses include the matching `Access-Control-Allow-Origin` header.

The target is added to `cpp/choreo_app` when the `wpinet` CMake target is available. The loopback integration test is `choreo_api_server.loopback`.

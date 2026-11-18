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
        .onOpen = [](choreo::api::WebSocketId id) {},
        .onText = [](choreo::api::WebSocketId id, std::string_view message) {},
        .onClose = [](choreo::api::WebSocketId id, std::uint16_t code,
                      std::string_view reason) {}});
api.Start();
const auto port = api.Port();
```

HTTP route templates use `{name}` path parameters. Request header names are normalized to lower case. HTTP and WebSocket callbacks execute on the server event-loop thread, so they should be short and non-blocking; `SendText` and `BroadcastText` may be called from other threads. Register all routes before `Start()`. The default bind address is loopback and port `0` requests an available ephemeral port. Configure `ServerOptions::allowedOrigins` for cross-origin browser or Tauri clients; same-origin and requests without an `Origin` header are accepted by default.

The target is added to `cpp/choreo_app` when the `wpinet` CMake target is available. The loopback integration test is `choreo_api_server.loopback`.

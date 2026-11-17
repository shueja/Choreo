# Choreo 2027 Architecture Plan

This document records the current architecture decisions and format-independent
research for Choreo 2027. It is a planning document, not a specification of the
new project format or an implementation plan for every component. Revisit open
items as local project-format work becomes available.

## Confirmed decisions

- The application will support both a standalone command-line server with a
  regular browser UI and an optional embedded Tauri webview.
- Starting the server should open the browser automatically. Users can select
  a project through both a command-line path argument and a server-hosted file
  picker.
- The C++ server is authoritative and supports one user and one open project.
  The server owns document state, undo/redo, and autosave.
- The server listens only on localhost. The browser UI uses REST for commands
  and WebSockets for server-published state and progress. NetworkTables is not
  the document-state interface.
- Project edits are written continuously. If a write fails, pause further
  mutations until persistence recovers; do not report an edit as committed
  before it has been saved.
- Existing project-format backward compatibility and import are not required.
  The new format should have a version and a migration mechanism for future
  format changes.
- Remove Rust business logic. Rust may remain only as a thin launcher for the
  embedded webview.
- Preserve the current platform support matrix except Linux ARM32.
- Prefer WPILib tooling and avoid unnecessary dependencies.

## Explicitly out of scope

- The new project format's fields, schema, serialization, validation rules, and
  migration details are not defined here; related work is in a local branch.
- The user's local C++ wrapper around TrajoptLib that configures it from Choreo
  project parameters is not designed here. Treat it as an external component
  to integrate when available.
- This plan does not specify solver internals or format-dependent generation
  policy.

## Proposed component boundaries

These are architectural boundaries for investigation, not a mandate to create
one library or executable per item.

1. **Server host** — command-line arguments, startup/shutdown, logging,
   loopback binding, browser launch, and reporting the selected URL.
2. **Application server** — owns the active session, accepts commands,
   coordinates persistence and application operations, and publishes committed
   revisions and progress.
3. **HTTP/WebSocket transport** — serves the UI and API, validates incoming
   requests, and translates transport messages into application operations.
   Keep document behavior out of this layer.
4. **Expression service** — evaluate whether the existing standalone
   `wpilib-expression` C++ library should serve the future backend. This is a
   separate decision; the current UI uses `mathjs`.
5. **Persistence** — keep the new format adapter separate from generic
   persistence mechanics such as safe replacement, error reporting, and
   serializing writes.
6. **Web UI** — React/Vite renders server-provided state and submits commands.
   Keep only presentation state (selection, hover, panel layout) in the client;
   do not preserve a second authoritative document model.
7. **Optional webview launcher** — starts or locates the same C++ server and
   displays its UI URL. It must not expose a privileged Tauri command API for
   application business logic.

## API and synchronization direction

- Use REST for project lifecycle, state-changing commands, undo/redo, and
  operation requests. The server validates and persists a proposed change
  before acknowledging it as committed.
- Use WebSockets for committed state changes, diagnostics, generation progress,
  and connection status.
- Design a generic command/event envelope with request correlation, monotonic
  revisions or sequence numbers, structured errors, and a way to request a
  current snapshot after reconnect. Do not bind these envelopes to document
  fields until the new format work is available.
- Keep the server authoritative: reject invalid or stale commands instead of
  attempting last-write-wins conflict resolution.
- The app is currently single-user/single-project, but preserve a clean
  transport/application boundary so future transport changes do not require
  rewriting document behavior.

## Transport research

**Current recommendation: use WPILib `wpinet` as the first implementation
candidate.** At the WPILib revision pinned by the existing C++ expression
project, the library includes HTTP connection handling, WebSocket upgrade
support, and a dedicated WebSocket server. Its API is low-level and requires
custom request dispatch; a small transport adapter should isolate that detail.

- [`HttpServerConnection.hpp`](https://github.com/wpilibsuite/allwpilib/blob/f06afad7b7bd3fa2ef94a6c0c7d0174fe207750a/wpinet/src/main/native/include/wpi/net/HttpServerConnection.hpp)
- [`HttpWebSocketServerConnection.hpp`](https://github.com/wpilibsuite/allwpilib/blob/f06afad7b7bd3fa2ef94a6c0c7d0174fe207750a/wpinet/src/main/native/include/wpi/net/HttpWebSocketServerConnection.hpp)
- [`WebSocketServer.hpp`](https://github.com/wpilibsuite/allwpilib/blob/f06afad7b7bd3fa2ef94a6c0c7d0174fe207750a/wpinet/src/main/native/include/wpi/net/WebSocketServer.hpp)
- [`wpinet` CMake target](https://github.com/wpilibsuite/allwpilib/blob/f06afad7b7bd3fa2ef94a6c0c7d0174fe207750a/wpinet/CMakeLists.txt)

The existing CMake setup in
`/home/runner/work/Choreo/Choreo/wpilib-expression/CMakeLists.txt` fetches
allwpilib and enables `WPILIB_WITH_NTCORE`, which brings in `wpinet`. Confirm
the smallest practical WPILib build configuration and packaging impact before
committing to this choice. If integration cost is excessive, evaluate
cpp-httplib as a smaller alternative; Boost.Beast appears heavier than needed
for this localhost, single-user service.

## Localhost security notes

Loopback binding is necessary but should not be treated as the entire security
boundary. Validate Host and Origin on HTTP requests and WebSocket upgrades,
protect state-changing routes against cross-origin requests, and avoid treating
CORS response headers as request authorization. Prefer serving the UI and API
from one origin. Revisit how the embedded webview's origin fits the allow-list
before implementation.

## Persistence semantics

- A mutation becomes visible as committed only after its write succeeds.
- Pause new mutations on persistence failure, surface a clear error, and resume
  only after persistence is healthy.
- Investigate atomic replacement and durability behavior on each supported
  platform once the new format's serializer and file lifecycle are known.
- Do not replace the format-specific reader/writer before the new format work
  is available.

## Current repository observations

- `/home/runner/work/Choreo/Choreo/src-tauri/src/api.rs` exposes business
  operations through Tauri commands; the UI calls them through
  `/home/runner/work/Choreo/Choreo/src/document/tauriCommands.ts`.
- `/home/runner/work/Choreo/Choreo/src/document/DocumentModel.tsx` keeps the
  editable document in a MobX-State-Tree model and attaches undo/redo there.
- `/home/runner/work/Choreo/Choreo/src/document/ExpressionStore.tsx` uses
  `mathjs` for expression evaluation. The standalone C++ experiment is under
  `/home/runner/work/Choreo/Choreo/wpilib-expression`; its own README says it
  is an independent library rather than production Choreo integration.
- `/home/runner/work/Choreo/Choreo/trajoptlib/CMakeLists.txt` builds and tests
  the native TrajoptLib library independently of its Rust FFI adapter. The
  format-dependent generation pipeline is in
  `/home/runner/work/Choreo/Choreo/src-core/src/generation/transformers/mod.rs`
  and should not be re-designed here.
- `/home/runner/work/Choreo/Choreo/vite.config.ts` builds the web UI, while
  `/home/runner/work/Choreo/Choreo/src-tauri/tauri.conf.json` currently embeds
  it and bundles a CLI sidecar.
- `/home/runner/work/Choreo/Choreo/.github/workflows/choreo.yml` currently
  covers Windows x86_64/ARM64, macOS x86_64/ARM64, and Linux x86_64/ARM64.
  Linux ARM32 is not listed in this matrix.

## Open research questions

- Can the existing WPILib CMake build expose `wpinet` without building an
  unnecessary set of NTCore dependencies, and what are the resulting bundle
  sizes and platform-specific packaging requirements?
- What should the standalone server's port-selection and browser-startup
  handshake be, especially when using an OS-assigned ephemeral port?
- How should the webview launcher discover the server URL and coordinate
  startup/shutdown without becoming a second application backend?
- Does `wpilib-expression` meet Choreo's expression-language, diagnostics, and
  execution-safety needs, or should expression behavior remain separate?
- What write-through and atomic-replacement guarantees can be provided
  consistently on Windows, macOS, and Linux?
- What API reconnection behavior is sufficient for the single-user UI without
  adding unnecessary event-log complexity?

## Suggested research order

1. Measure the `wpinet` CMake/build/package footprint on representative
   Windows, macOS, and Linux targets; compare cpp-httplib only if needed.
2. Specify generic request, event, revision, rejection, and reconnect behavior
   without document-specific payloads.
3. Evaluate expression-service adoption independently of project persistence.
4. Define write-through failure and safe replacement behavior once the new
   format's storage contract is available.
5. Integrate the user's project-parameter TrajoptLib wrapper only after its
   local interface is available; keep its design outside this plan.

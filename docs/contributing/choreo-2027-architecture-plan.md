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
- The new C++ server must use `wpilib-expression`; the new project format is
  designed around it. Treat the expression library as a production server
  dependency, not an optional candidate.
- Complexity limits are sufficient for expression evaluation; a hard
  evaluation timeout is not required.
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
3. **HTTP/WebSocket transport** — exposes the API, validates incoming requests,
   and translates transport messages into application operations. Recommended
   default: serve the production Vite assets from the C++ server as well, so
   both launch modes use one origin and no separate static-file server is
   needed. Verify Tauri navigation, CSP, and origin validation on supported
   platforms before committing to this. Keep document behavior out of this
   layer.
4. **Expression service** — integrate the existing `wpilib-expression` C++
   library as the server's expression engine. The current UI uses `mathjs`;
   migration should make the server the source of expression evaluation and
   validation. The library README states evaluation is not resource-limited, so
   define appropriate expression input and execution bounds for server use.
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
- Recommended reconnect behavior for the single-user application: events are
  live notifications, not a durable event log. On reconnect, fetch a complete
  snapshot with its revision, then resume processing events newer than that
  revision; reject or resynchronize on revision gaps. Persist no transport
  history solely to replay events missed while the UI was disconnected.
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

Additional `wpinet` integration findings at the pinned revision:

- The combined `HttpWebSocketServerConnection` supports HTTP and WebSocket
  upgrades on one connection/port. The API remains low-level: request dispatch
  is custom path parsing rather than a declarative router.
- Recommended default: serve the production Vite assets from the C++ server,
  alongside REST and WebSocket routes. This keeps standalone and embedded
  modes on one origin, avoids a second static-file server and cross-origin
  setup, and aligns the UI lifecycle with the server process. Keep Vite's
  development server for local frontend development. Verify Tauri can navigate
  to the server URL with the intended CSP and origin checks before adopting
  this in production.
- `uv::Tcp` exposes `Bind`, `GetSock`, and `AddrToName`, so an ephemeral-port
  flow can bind port `0` and query the selected port afterward. This path was
  inferred from the API and libuv behavior, not built or tested in Choreo.
  The convenience `WebServer::Start(port, path)` stores servers by the
  requested port and is not suitable for this handoff without adaptation.
- The WebSocket API exposes connection state and lifecycle signals. Its
  server helper offers URL and Host validation hooks, but Origin validation
  must be implemented by the application during the HTTP/WebSocket handshake.
- `HttpServerConnection` adds wildcard `Access-Control-Allow-Origin: *` and
  methods headers by default. Do not treat these response headers as request
  authorization; override/customize response handling and explicitly handle
  any required `OPTIONS` preflight.
- Sources: [`uv::Tcp`](https://github.com/wpilibsuite/allwpilib/blob/f06afad7b7bd3fa2ef94a6c0c7d0174fe207750a/wpinet/src/main/native/include/wpi/net/uv/Tcp.hpp),
  [`uv::AddrToName`](https://github.com/wpilibsuite/allwpilib/blob/f06afad7b7bd3fa2ef94a6c0c7d0174fe207750a/wpinet/src/main/native/include/wpi/net/uv/util.hpp),
  [`HttpWebSocketServerConnection`](https://github.com/wpilibsuite/allwpilib/blob/f06afad7b7bd3fa2ef94a6c0c7d0174fe207750a/wpinet/src/main/native/include/wpi/net/HttpWebSocketServerConnection.hpp),
  [`WebSocketServer`](https://github.com/wpilibsuite/allwpilib/blob/f06afad7b7bd3fa2ef94a6c0c7d0174fe207750a/wpinet/src/main/native/include/wpi/net/WebSocketServer.hpp),
  and [`HttpServerConnection` response headers](https://github.com/wpilibsuite/allwpilib/blob/f06afad7b7bd3fa2ef94a6c0c7d0174fe207750a/wpinet/src/main/native/cpp/HttpServerConnection.cpp).

## Localhost security notes

Loopback binding is necessary but should not be treated as the entire security
boundary. DNS rebinding can make an attacker-controlled hostname resolve to a
local service, so validate the `Host` header against the actual loopback
address/port as well as validating browser `Origin`. Apply the checks to
state-changing HTTP requests and WebSocket upgrades; CORS response headers do
not prevent a request from being sent. Do not use `GET` for mutations.

**Recommended baseline, not yet an implementation decision:** serve the UI and
API from the same origin; bind only to loopback; reject unexpected Host and
Origin values; and require a cryptographically random per-launch token in a
request header for mutations and WebSocket upgrades. Deliver the token through
the server/launcher bootstrap rather than as an API query parameter. A
same-origin deployment avoids needing permissive CORS, while the token adds
protection against unrelated local browser tabs. If the Tauri webview needs a
distinct origin, explicitly allow only that origin and provide the token
through a narrowly scoped startup mechanism, not a business-logic Tauri API.
This security analysis was not independently checked against external
standards sources in this environment; verify the threat model against current
browser and OWASP guidance before implementation.

## Persistence semantics

- A mutation becomes visible as committed only after its write succeeds.
- Pause new mutations on persistence failure, surface a clear error, and resume
  only after persistence is healthy.
- Investigate atomic replacement and durability behavior on each supported
  platform once the new format's serializer and file lifecycle are known.
- Do not replace the format-specific reader/writer before the new format work
  is available.

## Current repository observations

- `/home/runner/work/Choreo/Choreo/Cargo.toml` currently defines a Rust workspace;
  no first-class C++ application server exists. TrajoptLib is currently a
  separately buildable CMake library, not the application server.
- `/home/runner/work/Choreo/Choreo/src-tauri/src/api.rs` exposes business
  operations through Tauri commands; the UI calls them through
  `/home/runner/work/Choreo/Choreo/src/document/tauriCommands.ts`.
- `/home/runner/work/Choreo/Choreo/src/document/DocumentModel.tsx` keeps the
  editable document in a MobX-State-Tree model and attaches undo/redo there.
- `/home/runner/work/Choreo/Choreo/src/document/ExpressionStore.tsx` uses
  `mathjs` for expression evaluation. The new server is required to use
  `/home/runner/work/Choreo/Choreo/wpilib-expression`, and the new format is
  being designed for it. The root
  `/home/runner/work/Choreo/Choreo/IMPLEMENTATION_PLAN.md` currently describes
  the expression library as an independent project not to integrate into Choreo;
  that older experiment plan must be reconciled with this newer architecture
  decision. The library README says expression evaluation is not resource-limited.
- `/home/runner/work/Choreo/Choreo/trajoptlib/CMakeLists.txt` builds and tests
  the native TrajoptLib library independently of its Rust FFI adapter. The
  format-dependent generation pipeline is in
  `/home/runner/work/Choreo/Choreo/src-core/src/generation/transformers/mod.rs`
  and should not be re-designed here.
- `/home/runner/work/Choreo/Choreo/vite.config.ts` builds the web UI, while
  `/home/runner/work/Choreo/Choreo/src-tauri/tauri.conf.json` currently embeds
  it and bundles a CLI sidecar.
- The current Tauri sidecar is the Rust CLI, not a long-running server.
  `/home/runner/work/Choreo/Choreo/copy-sidecar.cjs` applies target-triple
  naming for Tauri packaging, and `/home/runner/work/Choreo/Choreo/src-tauri/build.rs`
  temporarily disables `externalBin` during the Cargo build.
- `/home/runner/work/Choreo/Choreo/.github/workflows/choreo.yml` currently
  covers Windows x86_64/ARM64, macOS x86_64/ARM64, and Linux x86_64/ARM64.
  Linux ARM32 is not listed in this matrix.
- The existing Linux ARM64 target is exercised on the native
  `ubuntu-24.04-arm` runner: CI runs the Rust test suite, builds the Tauri app,
  and packages standalone ZIP, AppImage, deb, and rpm artifacts. This verifies
  the current app pipeline only; the future C++ server still needs its own
  build, test, and packaging coverage.
- The legacy project writer in
  `/home/runner/work/Choreo/Choreo/src-core/src/file_management/mod.rs`
  coalesces pending project updates in a single `Option<ProjectFile>` and
  writes them on a background thread. The thread discards the write result and
  logs “Wrote project” regardless of success; `write_serializable` writes
  directly to the target via `fs::write`, without temporary-file replacement.
  This is not a suitable implementation to reuse for the 2027 server's
  write-before-acknowledge, pause-on-failure, and safe-replacement contract.
  Relevant code: `ProjectUpdater::update`/`wait_for`,
  `WritingResources::new`, and `write_serializable`.

## Expression runtime and build integration research

- The expression engine deliberately disables ChaiScript module loading,
  external scripts, and I/O. It validates setup and evaluation ASTs against
  restricted forms: project expressions are single read-only expressions,
  while setup accepts initialized variable declarations. These restrictions
  reduce the attack surface but do not constitute a resource budget.
- `wpilib-expression` documents that evaluation is not resource-limited. Its
  public `EvaluationContext` API has no deadline or cancellation parameter.
  Evaluation is synchronous: `EvaluateResult` expands, parses, validates, and
  evaluates the input while holding the engine-cache mutex
  (`wpilib-expression/src/expression.cpp`, `EvaluateResult`). Copies of a
  context share that cache, so concurrent evaluations through copies serialize.
- The architecture decision is to use input and expression-complexity limits;
  no hard evaluation timeout or process isolation is required. Establish
  concrete bounds before calling the library, including the input bytes
  permitted before expansion, setup-variable count, and parsed AST complexity.
- The pinned ChaiScript revision has parser nesting and function-call depth
  guards (defaults 512 and 256, respectively), but no time-based or
  cooperative interruption mechanism. These fixed recursion guards are not a
  substitute for the selected application-level bounds. Reject oversized
  requests before expansion/parsing and keep complexity limits below the
  library's internal recursion caps where practical.
  - [ChaiScript parser depth guard, pinned revision](https://github.com/ChaiScript/ChaiScript/blob/f635b51e3f8e95868210edfc66fdbd8e475710f5/include/chaiscript/language/chaiscript_parser.hpp#L88-L104)
  - [ChaiScript call-depth guard, pinned revision](https://github.com/ChaiScript/ChaiScript/blob/f635b51e3f8e95868210edfc66fdbd8e475710f5/include/chaiscript/chaiscript_defines.hpp#L76-L110)
- `wpilib-expression` directly links `wpimath`, LLNL units, and `wpi::extras`;
  its project CMake currently forces `WPILIB_WITH_NTCORE=ON`. In the pinned
  allwpilib CMake configuration, `WPILIB_WITH_WPIMATH` and
  `WPILIB_WITH_NTCORE` are independent options. Disabling NTCore skips the
  NTCore/wpinet, datalog, and libuv dependency path while retaining WPIMath.
  This is a static CMake dependency-graph finding, not a verified configure or
  build result. If the server chooses `wpinet` for transport, that transport
  choice may require enabling the NTCore/wpinet build path; measure it
  separately from the expression library's needs.
  - [allwpilib feature options and dependency gates, pinned revision](https://github.com/wpilibsuite/allwpilib/blob/f06afad7b7bd3fa2ef94a6c0c7d0174fe207750a/CMakeLists.txt#L91-L95)
  - [allwpilib NTCore and WPIMath subdirectory gates, pinned revision](https://github.com/wpilibsuite/allwpilib/blob/f06afad7b7bd3fa2ef94a6c0c7d0174fe207750a/CMakeLists.txt#L312-L327)
  - [WPIMath target dependencies, pinned revision](https://github.com/wpilibsuite/allwpilib/blob/f06afad7b7bd3fa2ef94a6c0c7d0174fe207750a/wpimath/CMakeLists.txt#L71-L111)
- The repository has a separate `wpilib-expression` workflow that configures,
  builds, and tests the library on Ubuntu, plus a setup-replacement benchmark.
  The main Choreo workflow covers six desktop/ARM64 targets but currently
  builds Cargo/Tauri rather than a C++ application server. A new server will
  need explicit CMake build, test, and packaging coverage across its supported
  targets.

## Packaging research

- Build the C++ localhost server as a first-class CMake executable, separate
  from TrajoptLib's library target and from the optional webview launcher.
- Produce one server binary per supported target, using a stable logical name
  such as `choreo-server`; let packaging handle platform suffixes and
  target-triple filenames.
- Both launch modes should use that same server binary. Standalone mode starts
  it and opens the browser; Tauri starts/manages it as a sidecar and opens the
  webview at the reported URL.
- Existing Tauri external-binary and copy-sidecar configuration may be a useful
  packaging pattern, but the current Rust CLI sidecar does not provide server
  readiness, URL/port negotiation, shutdown, or crash-recovery behavior.
- Recommended startup contract: bind to loopback port `0`, finish initializing,
  then emit one machine-readable readiness record containing the selected URL.
  In standalone mode the server opens that URL in the default browser; in
  Tauri mode the thin launcher starts the same executable with browser opening
  disabled, reads readiness, and navigates its webview to the URL. Keep
  diagnostics on stderr so stdout remains a reliable control channel. Validate
  this flow in the prototype, including startup failure and early child exit.
- Recommended process ownership: the Tauri launcher directly owns the server
  child process (no shell wrapper), requests graceful shutdown on app exit, and
  force-terminates only if needed. The server should not spawn helper processes
  that complicate Windows process cleanup. This is a proposed lifecycle, not
  yet tested in the existing Tauri app.
- Add explicit CMake configure/build/test/package steps to CI. The current
  Choreo workflow builds with Cargo/Tauri and packages both the GUI and CLI;
  it does not yet build a C++ application server.
- The current release pipeline provides Windows NSIS plus standalone ZIP,
  macOS DMG plus standalone ZIP, and Linux AppImage/deb/rpm plus standalone
  ZIP. Recommended starting point is to retain these formats, include the
  C++ server in GUI installers and include the server with its UI assets in
  standalone ZIPs. Confirm whether UI assets are embedded or shipped as files
  and measure their effect on bundle size before finalizing.

## Open research questions

- Can `wpilib-expression` build and pass tests with `WPILIB_WITH_NTCORE=OFF`?
  If the server selects `wpinet`, what extra build dependencies, compile time,
  and packaged runtime size does enabling NTCore/wpinet add? Static analysis
  suggests it can; configure/build verification is still needed.
- Does the proposed port-0/readiness-record startup contract work reliably
  with `wpinet` and Tauri on all supported platforms?
- Can Tauri own and clean up the direct C++ child process reliably on Windows,
  macOS, and Linux, including startup errors and forced termination?
- Should the recommended per-launch request token be mandatory in both browser
  and Tauri modes, and how should the launcher deliver it without exposing
  business operations through Tauri IPC? Are non-browser local API clients in
  scope?
- What concrete request-size, setup-count, and expression-complexity bounds
  should the server enforce before invoking `wpilib-expression`? A hard
  evaluation timeout is not required.
- What write-through and atomic-replacement guarantees can be provided
  consistently on Windows, macOS, and Linux?
- Can the optional Tauri webview load UI assets from the C++ server's loopback
  origin on all supported platforms while satisfying its CSP and origin
  validation? If so, use that same server to serve Vite assets.
- Does the new C++ server build and run across the existing six-target release
  matrix? Linux ARM64 is currently tested natively for the legacy app, but the
  future C++ server is not yet covered.

## Suggested research order

1. Verify a WPIMath-only `wpilib-expression` CMake configure/build with
   `WPILIB_WITH_NTCORE=OFF`; separately prototype the selected server transport
   and measure `wpinet` build/package footprint. Compare cpp-httplib only if
   integration cost warrants it.
2. Prototype port-0 binding, readiness reporting, browser launch, and direct
   Tauri child-process lifecycle; verify the same server binary serves both
   modes and that Tauri can load its UI origin.
3. Add a dedicated server build/test/package path to CI for all six retained
   targets, then verify the release artifact matrix, including native Linux
   ARM64.
4. Specify generic request/event envelopes and test snapshot-based reconnect
   behavior without document-specific payloads or durable event replay.
5. Define and test expression request-size, setup-count, and complexity limits
   and diagnostics around the required `wpilib-expression` integration.
6. Define write-through failure and safe replacement behavior once the new
   format's storage contract is available; test same-directory replacement
   and failure recovery on each supported operating system.
7. Integrate the user's project-parameter TrajoptLib wrapper only after its
   local interface is available; keep its design outside this plan.

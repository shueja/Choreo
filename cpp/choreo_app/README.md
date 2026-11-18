# C++ App Document Store

This project provides `choreo::document_app`, the app-only document store. It
depends on the robot-safe `choreo::document_core`, `wpilib-expression`, and
Immer. The public headers retain the `choreo/document/` include paths.

From the repository root, configure, build, and test with a C++23 compiler
supported by WPILib:

```powershell
cmake -S .\cpp\choreo_app -B .\cpp\choreo_app\build -G Ninja
cmake --build .\cpp\choreo_app\build
ctest --test-dir .\cpp\choreo_app\build --output-on-failure
```

By default the build uses the repository's document and expression projects.
Pass `-DCHOREO_APP_USE_INSTALLED_DEPENDENCIES=ON` and set `CMAKE_PREFIX_PATH`
to use their installed packages instead. An existing `choreo::document_core`
or `wpilib::expression` target is reused when composing a parent build.

`DocumentStore` evaluates editable expressions before publishing a snapshot,
supports undo and redo, and persists or reloads project and trajectory files.
Use the store from one writer thread; it does not provide synchronization.
Variable renames update editable expressions atomically. Subscribers receive
ordered snapshots after successful changes. Exceptions from subscribers are
collected by `TakeNotificationErrors` and do not roll back a committed change.

`SaveFiles` writes each file atomically and reports failures per file; saving
multiple files is not an atomic transaction. `ReloadFiles` reads and validates
the complete set before replacing the snapshot. Paths are keyed by trajectory
UUID. Dirty state is tracked per file and returns to clean when undo restores
the saved contents. `evaluation::EvaluationError` provides the affected
object ID and field.

When `wpinet` is available, the in-tree `choreo::generation_process` target
provides a subprocess handoff: it starts a generator with evaluated snapshot
files and can commit a successful, non-stale result as one undoable store
update. The originating store must outlive the process, and `Poll`, `Wait`, and
`CommitResult` must run on its owning thread. Cancellation force-terminates the
child; an optional progress URL is passed through but progress forwarding is not
implemented. This target is not yet installed, and the store does not provide
the server's durable multi-file commit contract. See `../../generator/README.md`
for the process boundary.

When `wpinet` is available, the `choreo_server` executable composes
`ApiServer` and `ApplicationApi`. It binds to `127.0.0.1`, defaults to an
ephemeral port, and prints one JSON readiness record to stdout:

```text
{"type":"ready","url":"http://127.0.0.1:PORT"}
```

Pass the Vite output directory with `--web-root PATH` to serve its static
assets, and configure an optional generator path with `--generator PATH`.
Repeat `--allowed-origin ORIGIN` to permit cross-origin UI clients. The default
Vite output directory is `dist`. The current host does not yet open a browser
or provide a native project picker. Open projects using the API's path-based
session operation. Stop the process with Ctrl+C or SIGTERM. The existing UI
still uses Tauri commands, so static hosting is not yet a usable browser app.

When `wpinet` is available, separate CTest cases cover the test-child handoff
and real-generator differential, swerve, and split scenarios. Set
`CHOREO_GENERATOR_TEST_EXECUTABLE` to the built generator's full path before
running CTest; without it, the real-generator cases are reported as skipped.
See [tests/README.md](tests/README.md) for the test inventory and run commands.

The `choreo-document-app` install component exports the app target and its
headers. Its consumers also need the installed shared document,
wpilib-expression, LLNL units, WPILib, and wpilib-extras packages.
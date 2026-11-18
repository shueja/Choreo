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
Variable renames update editable expressions atomically; application
integration is not implemented yet. Subscribers receive ordered snapshots
after successful changes. Exceptions from subscribers are collected by
`TakeNotificationErrors` and do not roll back a committed change.

`SaveFiles` writes each file atomically and reports failures per file; saving
multiple files is not an atomic transaction. `ReloadFiles` reads and validates
the complete set before replacing the snapshot. Paths are keyed by trajectory
UUID. Dirty state is tracked per file and returns to clean when undo restores
the saved contents. `evaluation::EvaluationError` provides the affected
object ID and field.

The `choreo-document-app` install component exports the app target and its
headers. Its consumers also need the installed shared document,
wpilib-expression, LLNL units, WPILib, and wpilib-extras packages.
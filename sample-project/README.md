# WPILib Expression Sample

This standalone REPL consumes the installed `wpilib-expression` CMake package. It does not include or compile the library sources.

## Build

Configure and install `wpilib-expression` first. Then configure this project with both the expression install prefix and a compiler supported by WPILib:

```powershell
cmake -S .\sample-project -B .\sample-project\build -G Ninja `
  -DCMAKE_PREFIX_PATH="C:\path\to\expression-install" `
  -DCMAKE_C_COMPILER="C:\path\to\clang-cl.exe" `
  -DCMAKE_CXX_COMPILER="C:\path\to\clang-cl.exe"
cmake --build .\sample-project\build
ctest --test-dir .\sample-project\build --output-on-failure
```

The sample resolves WPILib through its own pinned `FetchContent` declaration. It requires network access on the first configure unless the CMake FetchContent source overrides are already populated.

Run `expression_sample.exe` for the REPL. Enter an expression such as `2 m + 3 ft`; results are shown as an SI magnitude and dimension exponents. Enter `:quit` to exit. The `--smoke` option runs a scalar and geometry check.

From the repository root, load context variables from a setup file before starting the REPL:

```powershell
.\sample-project\build\expression_sample.exe --setup .\sample-project\example-setup.chai
```

The file uses the restricted setup syntax described in the expression library documentation. For example, with the included setup file, evaluate `2 m + offset`.
Rotation methods such as `target.Rotation().Degrees()` return an angle displayed in SI radians; divide by `deg` to display a dimensionless degree value.
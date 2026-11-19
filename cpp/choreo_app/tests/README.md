# C++ App and Generation Tests

This inventory covers `cpp/choreo_app` and the shared document tests that can
be included in its build. It is not an inventory of all repository tests.
Test names below are CTest names; registration depends on the build options and
available targets. List the tests in a particular build with:

```powershell
ctest --test-dir .\cpp\choreo_app\build -N
```

## Available Tests

| CTest name | Source | Coverage |
| --- | --- | --- |
| `choreo_document_core.snapshots` | `document_test.cpp` | Expression evaluation and dimensions, document validation, snapshots, generated-result commits, revisions, notifications, variable renaming, undo/redo, file saves and reloads, dirty tracking, and failure handling. |
| `choreo_generation_process.handoff` | `generation_process_test.cpp` | Launches a test child. Checks one-update publication and undo/redo, premature and repeated commits, stale results, wrong store, child failure, malformed or tampered output, wrong drive type or UUID, missing output, and cancellation. |
| `choreo_generation_process.real_differential` | `generation_process_test.cpp` | Launches the real generator for a two-waypoint differential trajectory. Checks the returned document, samples, frozen inputs, waypoint and event timing, notification, and undo. |
| `choreo_generation_process.real_swerve` | `generation_process_test.cpp` | Launches the real generator for a two-waypoint swerve trajectory. Checks output type, frozen inputs, event timing, samples, and undo/redo. |
| `choreo_generation_process.real_split` | `generation_process_test.cpp` | Launches the real generator for a three-waypoint differential trajectory. Checks an intermediate split, sample and waypoint timing, a UUID-targeted event, and undo. |
| `choreo_application_api.integration` | `application_api_test.cpp` | Runs independent loopback tests for immediate operation acceptance, deferred picker open/cancel, new-session creation and replacement, session close/save and failed-save retention, snapshot and resource reads, project and trajectory patches, waypoint/constraint/event-marker create/update/delete, stale revisions, idempotent mutations, asynchronous save, generation persistence, undo/redo, and persistence-failure recovery. |
| `choreo_document_core.shared` | `../../../document/cpp/tests/shared_test.cpp` | Shared C++ document serialization, validation, trajectory output, and robot-loader behavior; also writes JSON fixtures for schema validation. |
| `choreolib.v4` | `../../../document/cpp/tests/choreolib_test.cpp` | Loads v4 swerve and differential samples through the C++ ChoreoLib string and file interfaces; checks missing-file errors. |

`document_test_fixture.hpp` supplies the common project and trajectory fixture
for the app and subprocess tests. The handoff test executable also acts as its
own fake generator child; the `real_*` cases use the actual generator instead.

The app tests require `BUILD_TESTING=ON`. The subprocess target and its tests
are registered only when the CMake `wpinet` target is available. The shared
document tests appear in the app build when the document project is included
with testing enabled; an installed-dependency configuration may not register
them. The real-generator cases return CTest's skip code when
`CHOREO_GENERATOR_TEST_EXECUTABLE` is unset or empty; a configured but invalid
executable is a failure, not a skip.

## Build and Run

Run these commands from the repository root with a C++23 toolchain supported
by WPILib. The first configure may fetch dependencies. Use a developer shell
with the compiler available on Windows. The paths shown are for Ninja's
single-configuration generator; multi-configuration builds need `--config` for
builds, `-C` for CTest, and the corresponding executable path.

```powershell
cmake -S .\cpp\choreo_app -B .\cpp\choreo_app\build -G Ninja
cmake --build .\cpp\choreo_app\build
ctest --test-dir .\cpp\choreo_app\build --output-on-failure
```

The default run exercises the test-child handoff if `wpinet` is available and
reports the real-generator cases as skipped unless the executable is configured.
To build the generator and run those cases on Windows:

```powershell
cmake -S .\generator -B .\generator\build -G Ninja
cmake --build .\generator\build --target generator
$env:CHOREO_GENERATOR_TEST_EXECUTABLE = (Resolve-Path .\generator\build\generator.exe).Path
ctest --test-dir .\cpp\choreo_app\build -R '^choreo_generation_process\.' --output-on-failure
```

On Linux or macOS, set `CHOREO_GENERATOR_TEST_EXECUTABLE` to the absolute path
of the built `generator` executable. To run one case, use its full CTest name
with `-R`, for example `-R '^choreo_generation_process\.real_split$'`.

## Schema Validation

The shared test emits `schema-project.json` and `schema-trajectory.json` in its
document build directory. The schema validator is a separate Python command,
not an automatically registered CTest. With Python's `jsonschema` package
installed, run it against fixtures from a standalone document build:

```powershell
cmake -S .\document\cpp -B .\document\cpp\build -G Ninja
cmake --build .\document\cpp\build
ctest --test-dir .\document\cpp\build -R '^choreo_document_core\.shared$' --output-on-failure
python .\document\cpp\tests\validate_schema.py .\document\cpp\build
```

See `../../../document/cpp/README.md` for document-test and installed-consumer
instructions. Expression-engine and TrajoptLib tests have their own builds and
are outside this app-test inventory.

## Coverage Limits

- The real-generator tests are opt-in and currently use fixed interval counts;
  they do not establish the automatic-interval snapshot invariant or the
  behavior of a split flag on the final waypoint.
- The application API test uses a fake generator to verify one diagnostic
  progress message and a successful result; it does not establish progress
  behavior with the real generator, graceful cancellation, crash recovery, or
  durable multi-file commit behavior.
- A successful test on one platform does not establish subprocess or filesystem
  behavior on the other supported operating systems.
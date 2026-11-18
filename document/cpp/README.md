# C++ Document Core

This project contains shared document types and a v4 robot trajectory loader.
The shared target uses WPILib and wpilib-extras; it does not depend on Immer or
wpilib-expression. The separate `cpp/choreo_app` project provides the app's
document store, expression evaluation, and history.

Configure with a C++23 compiler supported by WPILib, then build and test:

```powershell
cmake -S .\document\cpp -B .\document\cpp\build -G Ninja
cmake --build .\document\cpp\build
ctest --test-dir .\document\cpp\build --output-on-failure
```

The first configure fetches the WPILib dependencies. Use the CMake
`FETCHCONTENT_SOURCE_DIR_*` overrides to reuse local sources.

`LoadRobotTrajectory` accepts schema version 4 only and rejects missing or
empty generated output. It skips unresolved events and reports their UUIDs in
`warnings`. It normalizes timestamps to start at zero and stably sorts samples;
if sorting would move a sample to a different split, loading fails. Waypoint and
resolved event times are adjusted by the same timestamp origin. Sorting also
fails if it would change the identity of a split-boundary sample.
Circular constraints serialize as WPILib ellipses with equal positive semi-axes.

Scalar expressions use `Expression<dimension::Dimension<...>>` or a named alias
such as `Expression<dimension::Length>`. `dimension::Dimension` takes integer
exponents in LLNL order: metre, kilogram, second, ampere, kelvin, mole, candela,
and radian. The expression's `value` is the corresponding WPILib SI unit value.
The app-only `evaluation::RuntimeDimension` and `evaluation::FromMeasurement`
provide LLNL conversion and check dimensions and finiteness.

The shared package can be installed with the CMake component
`choreo-document-core`; its consumers must also provide WPILib and
wpilib-extras. See `cpp/choreo_app/README.md` for the separate app package.

To validate serialized fixtures against the JSON schemas, install Python's
`jsonschema` package, run the shared CTest, then run:

```powershell
python .\document\cpp\tests\validate_schema.py .\document\cpp\build
```

## Schema Change Notes

These notes are for maintainers and coding agents changing the document format.
The authoritative JSON shapes are the schemas in `document/`; see also
`document/changelog.md` and `docs/contributing/schema-upgrade.md`. The existing
Rust and TypeScript application models may describe an older format; do not
assume they already match the C++ schema or that this store is wired into the
application.

When changing a schema:

1. Update the relevant `*.schema.json` definitions and changelog. Preserve the
	distinction between required nullable fields, optional fields, and omitted
	fields. Coordinate version changes across languages before running
	`update_project_schema.py` or `update_traj_schema.py`: these scripts update
	multiple repository targets, not just C++.
2. Update the corresponding classes in `include/choreo/document/` and their
	`to_json`/`from_json` implementations in `src/`. Update
	`json_validation.hpp` and relevant `CheckProperties` calls when object
	fields or geometry rules change. Register new source files in
	`CMakeLists.txt`. Prefer WPILib and wpilib-extras types where they match the
	schema, and test the actual JSON shape rather than assuming a WPILib
	serializer matches it.
3. For constraint variants, update the variant in `constraint.hpp`, its tagged
	dispatch in `constraint.cpp`, and each variant's serializer. For variable
	types, keep the discriminator, `VariableValue` variant order, JSON mapping
	in `variable.cpp`, and the app's `RuntimeType` mapping in sync.
4. For every new editable expression, choose the correct scalar dimension or
	WPILib geometry type, then update both `cpp/choreo_app/src/evaluate_variables.cpp`
	and `cpp/choreo_app/src/rename_variable.cpp`. Add corresponding evaluation, unit mismatch,
	rename, and rollback tests. The app reevaluates editable fields on commit
	and reload; generation `config`, `snapshot`, and `output` are frozen and
	must not be rewritten or reevaluated against current variables.
5. Keep `choreo::document_core` robot-safe: its public headers and transitive
	dependencies must not require Immer, LLNL units, or `wpilib-expression`.
	Those belong in `choreo::document_app` and `cpp/choreo_app/`. Check both
	projects' CMake install components when adding app-only headers or sources.
	Robot code consumes WPILib samples, not the
	removed legacy ChoreoLib C++ sample classes.
6. Preserve atomic publication and history semantics in `DocumentStore`:
	rejected edits and failed reloads leave the prior snapshot unchanged;
	notifications follow successful changes; per-file saves can partially fail.
	The store is single-writer and does not provide synchronization.
7. Update `document/cpp/tests/shared_test.cpp`, `cpp/choreo_app/tests/document_test.cpp`,
	and `document/cpp/tests/choreolib_test.cpp` as applicable. Cover schema round trips, unknown
	and missing fields, nullable/empty values, both drive types, expression
	errors, frozen snapshots, undo/redo, and file failures. Run the schema
	validator above against fixtures emitted by the shared test.

Current readers accept only their configured schema versions; they do not
migrate older files. Changing that policy requires explicit migration and
compatibility tests. When changing trajectory samples, also test
`src/trajectory_output.cpp`: WPILib trajectories require a nonempty sequence
starting at zero, while the document represents empty trajectories with an
optional. Normalization adjusts waypoint and event times and rejects sorting
that changes split membership or split-boundary identity.

Run the shared build and CTest commands above, then build and test the app
project separately:

```powershell
cmake -S .\cpp\choreo_app -B .\cpp\choreo_app\build -G Ninja
cmake --build .\cpp\choreo_app\build
ctest --test-dir .\cpp\choreo_app\build --output-on-failure
```

If changing expression syntax or bindings, also build and run the tests in
`wpilib-expression/`. `tests/consumer/` is a separate installed-package
consumer for checking the exported robot and app targets; it needs the
installed dependencies described above.
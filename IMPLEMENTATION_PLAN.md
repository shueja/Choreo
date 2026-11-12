# ChaiScript WPILib Expression Parser

## Goal

Deliver two independent CMake projects under this directory:

- `wpilib-expression/`: an installable/exported C++ library for expression evaluation.
- `sample-project/`: a separate interactive sample app that consumes the installed library through its public CMake package.

Use ChaiScript for expression evaluation, a runtime quantity wrapper for dimensioned scalar arithmetic, and actual WPILib units and geometry types at validated boundaries. Support Math.js-like bare unit suffixes and WPILib-style geometry method calls. Do not integrate into Choreo or continue testing other parser candidates.

## Implementation Steps

1. **Define the public library contract.** Provide public headers, a namespaced CMake target, install/export rules, and a small API for creating an evaluation context, registering variables, evaluating expressions with an optional expected dimension or target unit, and retrieving typed scalar or geometry results. Keep ChaiScript and WPILib dependency requirements explicit. Use the existing `chai-units/` probe as behavioral reference, not as the consumer app.

2. **Implement runtime quantity semantics.** Represent runtime quantities with an SI-base magnitude and dimension signature. Derive and check dimensions for addition, subtraction, multiplication, division, comparison, supported powers, and trig. Parse units from a bounded registry of symbols and aliases needed by Choreo; use WPILib unit types for scales/conversions where practical. Reject unknown units, incompatible operations, non-finite results, and mismatches against caller-specified expected dimensions. Convert successful results to an explicitly requested unit and to concrete WPILib units such as `meter_t` and `radian_t`.

3. **Implement Math.js-like suffix syntax.** Add a focused lexical/preprocessing layer supporting `2 m`, `90 deg`, `6000 RPM`, `2 m/s^2`, and `0.018 N*m/A`, preserving grouping, scientific notation, explicit operators, and source locations. Do not globally rewrite whitespace to multiplication. Specify identifier/unit precedence and handle ambiguous names predictably.

4. **Bind actual WPILib geometry.** Register construction, properties, and useful methods for `wpi::math::Pose2d`, `Translation2d`, `Rotation2d`, `Transform2d`, and `Twist2d`. Add small Choreo-specific region/circle/polygon adapters only where WPILib has no matching geometry type. Expose WPILib method names and return WPILib objects so expressions can construct geometry, access coordinates, invoke methods, chain results, and return values to C++.

5. **Create a separate sample consumer.** Give `sample-project/` its own CMake project. It should use `find_package` on the installed/exported library target rather than compiling library sources directly. Provide a small REPL or command-line expression runner, example expressions, and setup instructions for building/installing the library and configuring the sample. The sample must not depend on Choreo application targets.

6. **Test and document the chosen stack.** Add CTest coverage to the library and smoke tests for the sample. Document syntax, public API, dependencies/build steps, unsupported operations, diagnostics, and unit-name ambiguities. Leave the other candidate experiment projects present but untouched; do not run or expand their suites.

## Acceptance Tests

- Configure, build, install, and run library CTest from a clean build directory using the available Windows MinGW/Ninja toolchain; record dependency revisions and exact commands.
- Configure and build the sample separately against the installed package using `CMAKE_PREFIX_PATH`. Confirm it does not depend on library source paths or Choreo targets.
- `2 m + 3 ft` returns `2.9144 m`; `2 m + 3 s` is rejected.
- `2 m * 3 m` succeeds as area but is rejected when length is requested.
- Bare suffix cases `2 m`, `90 deg`, `6000 RPM`, `2 m/s^2`, and `0.018 N*m/A` work, as do target-unit conversions. Test scientific notation, parentheses, explicit multiplication, malformed suffixes, unknown symbols, and unit/variable name collisions.
- Geometry expressions such as `Pose2d(3 m, 4 m, 90 deg)`, `pose.X()`, `pose.RotateBy(rotation).RelativeTo(origin)`, and `translation.Distance(other)` produce actual WPILib values and expected results.
- Invalid geometry argument dimensions produce useful errors; failed evaluations do not corrupt subsequent evaluations.
- No Choreo application/schema or allwpilib files are modified.

## Reference Files

- Existing probe: `chai-units/CMakeLists.txt`, `chai-units/main.cpp`.
- WPILib geometry APIs: `../allwpilib/wpimath/src/main/native/include/wpi/math/geometry/Pose2d.hpp`, `Translation2d.hpp`, `Rotation2d.hpp`, `Transform2d.hpp`, and `Twist2d.hpp`.
- WPILib unit aliases/conversions: `../allwpilib/wpiunits/` (trace generated aliases and build targets during implementation).
- Choreo dimensions and geometry shapes: `../document/changelog.md` and `../document/shared.schema.json`.

## Decisions

- ChaiScript is selected; no further candidate testing.
- Deliver a reusable, installable C++ library plus an independent sample consumer; no production integration.
- Parsed scalar intermediates use a runtime quantity wrapper with explicit dimension metadata, converted to WPILib compile-time units and geometry at checked boundaries.
- Use actual WPILib geometry types wherever available; add custom types only for Choreo concepts WPILib does not provide.
- Bare suffix syntax is Math.js-like and handled by a focused tokenizer/preprocessor, not indiscriminate whitespace multiplication.

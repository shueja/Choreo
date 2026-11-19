# WPILib Expression

`wpilib-expression` is an installable C++ expression-evaluation library backed by ChaiScript, runtime dimensional quantities, and native WPILib geometry objects.

## Dependencies

- WPILib `allwpilib` revision `f06afad7b7bd3fa2ef94a6c0c7d0174fe207750a`
- ChaiScript revision `f635b51e3f8e95868210edfc66fdbd8e475710f5`
- ChaiScript Extras revision `dd116d732159e38a70214b8dc9eefd243bab17e5`
- Eigen revision `b7f6aed1b9974fd56cce651b4ff059c3bfc67782`
- LLNL units revision `v0.13.1`
- CMake 3.21 or newer and a C++23 compiler

The CMake project fetches these dependencies. On Windows, configure from an environment with the compiler, Windows SDK resource compiler, and MSVC libraries available.

## Build and Install

```powershell
cmake -S .\wpilib-expression -B .\wpilib-expression\build -G Ninja `
  -DCMAKE_C_COMPILER="C:\path\to\clang-cl.exe" `
  -DCMAKE_CXX_COMPILER="C:\path\to\clang-cl.exe"
cmake --build .\wpilib-expression\build
ctest --test-dir .\wpilib-expression\build --output-on-failure
cmake --install .\wpilib-expression\build --prefix C:\path\to\expression-install
```

To build and run the variable-replacement benchmark, configure with
`-DWPILIB_EXPRESSION_BUILD_BENCHMARKS=ON`, build the
`wpilib_expression_setup_replacement_benchmark` target, then run that executable.
It reports variable replacement and evaluation times for batches of 100, 500,
and 1,000 expressions over three replacements.

The exported target is `wpilib::expression`. A consumer uses `find_package(wpilib-expression CONFIG REQUIRED)` and links that target. The consumer must also make WPILib's `wpimath::wpimath` target available or install WPILib and add its prefix to `CMAKE_PREFIX_PATH`.

## API

```cpp
#include <wpilib-expression/expression.hpp>
#include <wpi/units/length.hpp>

wpilib::expression::EvaluationContext context;
auto length = wpilib::expression::From(1.0, units::precise::m).units();
auto distance = context.Evaluate("2 m + 3 ft", length);
auto meters = wpilib::expression::In(distance, units::precise::m);
if (meters) {
  double valueInMeters = *meters;
}
```

Contexts register scalar and native WPILib geometry values with `SetVariable`. Translation arrays can be registered as `std::vector<wpi::math::Translation2d>` and indexed with `[index]`; they also work with `Nearest`. A variable's first value establishes its declared type: quantities are fixed to their base dimension, and geometry values to their concrete geometry type. Replacing a value through `SetVariable`, `SetSetupVariables`, or `SetVariables` must preserve that type; a rejected update leaves the prior context value intact. Namespace members follow the same rule by their qualified name. `Evaluate` returns an LLNL `units::precise_measurement` and can check an expected LLNL base unit; `EvaluateBoolean` evaluates comparisons. Typed evaluation methods return WPILib poses, translations, rotations, transforms, twists, rectangles, and ellipses.

For setup variables stored without an ordering, `SetSetupVariables` accepts a
span of `{id, name, initializer}` records. IDs must be unique non-empty strings;
names must be unique valid expression identifiers. Initializers may refer to other
setup variables in any input order. The library resolves dependencies and rejects
unknown names and cycles. `SetupVariableError`, derived from `std::invalid_argument`,
provides the associated ID through `Id()`. A failed replacement leaves the context
unchanged.

```cpp
std::vector<wpilib::expression::SetupVariable> variables{
  {"distance-id", "distance", "width * 2"},
  {"width-id", "width", "3 m"},
};
context.SetSetupVariables(variables);
auto result = context.Evaluate("distance");
```

After changing an initializer, build a replacement context with the updated
records. To rename a variable, `RenameSetupVariable` returns a new collection
with the matching ID renamed and references in all setup initializers rewritten.
Apply `RenameVariableReferences` to every expression stored in both application
trees, using the old and new names:

```cpp
auto renamedVariables = wpilib::expression::RenameSetupVariable(
  variables, "width-id", "trackWidth");
auto renamedExpression = wpilib::expression::RenameVariableReferences(
  "width * 2", "width", "trackWidth");
wpilib::expression::EvaluationContext renamedContext;
renamedContext.SetSetupVariables(renamedVariables);
```

These operations return new values and do not traverse or modify application
trees. The caller must rewrite every affected expression in both trees, validate
the replacement setup and expressions, then publish the new roots and context
together. A name collision is rejected; an existing unresolved reference to the
new name can acquire a meaning after the rename, so validate the resulting trees.
The source rewrite preserves whitespace, comments, strings, member names and
function names, and can operate on incomplete expressions. It is lexical and
does not validate the expression; only top-level setup variable names are
supported, not renaming individual namespace members.

Copied contexts share their engine cache, but their variables are copy-on-write:
changing one copy does not change the other. Concurrent evaluation of copies is
serialized through the shared engine cache. Do not mutate a context concurrently
with other accesses to that same context object.

Setup supports nested namespace blocks and the `namespace("name")` form. Namespace members are frozen after setup and can be accessed with `.` or `::`. Initializers referencing earlier members must use their qualified names.

Variables with stable caller-provided IDs, names, expressions, and optional namespace paths are declared with `SetVariables`. Expressions can reference one another regardless of input order; dependencies are evaluated first and cycles are rejected. Namespaces are metadata, and expressions refer to members by their qualified dotted names. Replacing definitions is atomic: an expression must retain the original variable's type (quantity base dimension or concrete geometry type), and a failed replacement leaves the prior values intact. `GetVariables` returns definitions, declared types, dimensions, and cached values. `FindReferences` returns the IDs of dependent variables, and `RenameVariable` updates a variable's name and rewrites references while preserving its ID and declared type.

`SetVariable` remains available for context values supplied directly by callers; those values do not have structured IDs or expressions. `Evaluate` returns an LLNL `units::precise_measurement` and can check an expected LLNL base unit; `EvaluateBoolean` evaluates comparisons. Typed evaluation methods return WPILib poses, translations, rotations, transforms, twists, rectangles, and ellipses.

```cpp
context.SetVariables({
    {"origin-id", "field.starting", "origin", "Pose2d(0 m, 0 m, 0 deg)"},
    {"position-id", "field.target", "position",
     "field.starting.origin.Translation()"},
    {"distance-id", "", "distance", "2 m"},
    {"target-id", "", "target", "Pose2d(3 m, 4 m, 90 deg)"},
});
auto origin = context.EvaluatePose2d("field.starting.origin");
auto result = context.Evaluate("distance + target.X()");
```

```cpp
context.RenameVariable("distance-id", "length");
auto references = context.FindReferences("distance-id");
```

`TryAs<UnitValue>(measurement)` converts to a compatible linear WPILib `unit_t` container and returns `std::optional<UnitValue>`; the output-parameter overload returns `false` on incompatibility:

```cpp
auto distance = context.Evaluate("1 m");
if (auto feet = wpilib::expression::TryAs<wpi::units::foot_t>(distance)) {
  double valueInFeet = feet->value();
}

wpi::units::radian_t angle;
bool converted = wpilib::expression::TryAs(context.Evaluate("90 deg"), angle);
```

The conversion checks the full WPILib base-dimension signature. Non-linear WPI scales, unsupported runtime dimensions, and dimension mismatches are not converted.

`In` returns `std::expected<double, Error>`; an incompatible dimension returns `IncompatibleDimensionError`. Use `ErrorMessage` to format an error.

## Expression Support

Implicit multiplication works with variables, units, and grouped expressions:
`2width`, `width height`, `(width + 1) m`, and `2(width + 1)` are products.
`width(...)` is still parsed as a function call. Powers bind more tightly than
implicit multiplication, which in turn binds more tightly than explicit
multiplication and division. As in math.js, a numeric fraction before a symbol
or opening parenthesis is the exception: `2/3 m` means `(2 / 3) * m`, while
`width/3 m` means `width / (3 * m)`.

Bare unit suffixes include `2 m`, `90 deg`, `6000 RPM`, `2 m/s^2`, and `0.018 N*m/A`. Parentheses, scientific notation, explicit multiplication, division, and integer powers are supported. Addition, subtraction, comparisons, and powers validate dimensions. The ChaiScript math extra supplies scalar math functions. Quantity overloads preserve dimensions for absolute value, rounding, same-dimension remainders and extrema, and `hypot`; `sqrt` and `cbrt` require exponents divisible by their root degree. Transcendental functions require dimensionless quantities, except trigonometric functions, which also accept angles, and `atan2`, which accepts matching dimensions and returns an angle. `min` and `max` are available through dimension-checking Quantity overloads.

The bounded unit registry includes common Choreo/FRC engineering units: `m`, `mm`, `cm`, `km`, `in`, `ft`, `yd`, `mi`; `s`, `ms`, `us`, `min`, `hr`; `kg`, `g`, `lb`; `A`, `mA`; `rad`, `deg`, `turn`, `rev`; `N`, `lbf`, `V`, `mV`, `kV`, `Hz`, `kHz`, `W`, `kW`, `J`, `kJ`, `C`, `Pa`, `kPa`, `psi`, `ohm`, `Ohm`, `Nm`, and `RPM`/`rpm`. Symbols are case-sensitive. Compound unit expressions accept `*`, `/`, and integer exponents with magnitude up to 16. Unit names are reserved and cannot be reused as variable names. Unknown unit suffixes, incompatible dimensions, divide-by-zero, and invalid geometry dimensions are rejected; non-finite scalar results are preserved.

Geometry bindings cover constructors and commonly used properties/methods for WPILib `Pose2d`, `Translation2d`, `Rotation2d`, `Transform2d`, `Twist2d`, `Rectangle2d`, and `Ellipse2d`. Shape operations include containment, intersection, distance, nearest point, transforms, and rotation. An ellipse's `FocalPoints()` returns two points accessible with `[0]` and `[1]`. Geometry arguments are checked at the scalar-to-WPILib boundary. Evaluation accepts only a single read-only expression using the library's supported operators and functions; assignments, declarations, and other script operations are rejected.

For convenience, `Pose2d` supports `Distance(otherPose)` using its translation; this ignores rotation. For a pose's norm, use `pose.Translation().Norm()` or `pose.Translation().SquaredNorm()`.

Planar geometry values support WPILib's arithmetic and equality operators. Scaling accepts finite dimensionless scalars; division by zero is rejected. `Translation2d::Angle()` returns a rotation and rejects translations whose direction is undefined. Pose and translation `Nearest()` accept non-empty inline lists, for example `origin.Nearest([Pose2d(), target])`, or a registered translation array such as `query.Nearest(waypoints)`.

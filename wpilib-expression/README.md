# WPILib Expression

`wpilib-expression` is an installable C++ expression-evaluation library backed by ChaiScript, runtime dimensional quantities, and native WPILib geometry objects.

## Dependencies

- WPILib `allwpilib` revision `f06afad7b7bd3fa2ef94a6c0c7d0174fe207750a`
- ChaiScript revision `f635b51e3f8e95868210edfc66fdbd8e475710f5`
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

To build and run the setup-replacement benchmark, configure with
`-DWPILIB_EXPRESSION_BUILD_BENCHMARKS=ON`, build the
`wpilib_expression_setup_replacement_benchmark` target, then run that executable.
It reports setup replacement and evaluation times for batches of 100, 500, and
1,000 expressions over three setup changes.

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

Contexts register scalar and native WPILib geometry values with `SetVariable`. `Evaluate` returns an LLNL `units::precise_measurement` and can check an expected LLNL base unit; `EvaluateBoolean` evaluates comparisons. Typed evaluation methods return WPILib poses, translations, rotations, transforms, twists, rectangles, and ellipses.

`SetSetupScript` atomically replaces context variables using initialized top-level `var`, `auto`, `const`, or `global` declarations. Initializers may use read-only expressions and previously declared variables. Values must be quantities, numbers, or supported geometry objects; numbers become dimensionless quantities. A failed setup leaves the previous variables unchanged. Other statements, user-defined functions, external scripts, module loading, and I/O are not permitted. Evaluation is not resource-limited.

Setup supports nested namespace blocks and the `namespace("name")` form. Namespace members are frozen after setup and can be accessed with `.` or `::`. Initializers referencing earlier members must use their qualified names.

```cpp
context.SetSetupScript(R"(
  namespace field::starting {
    var origin = Pose2d(0 m, 0 m, 0 deg);
  }
  namespace field::target {
    var position = field.starting.origin.Translation();
  }
)");
auto origin = context.EvaluatePose2d("field.starting.origin");
```

```cpp
context.SetSetupScript("var distance = 2 m; var target = Pose2d(3 m, 4 m, 90 deg);");
auto result = context.Evaluate("distance + target.X()");
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

Bare unit suffixes include `2 m`, `90 deg`, `6000 RPM`, `2 m/s^2`, and `0.018 N*m/A`. Parentheses, scientific notation, explicit multiplication, division, and integer powers are supported. Addition, subtraction, comparisons, and powers validate dimensions. Trigonometric functions accept angle or dimensionless inputs; inverse trigonometric functions return angles.

The bounded unit registry includes common Choreo/FRC engineering units: `m`, `mm`, `cm`, `km`, `in`, `ft`, `yd`, `mi`; `s`, `ms`, `us`, `min`, `hr`; `kg`, `g`, `lb`; `A`, `mA`; `rad`, `deg`, `turn`, `rev`; `N`, `lbf`, `V`, `mV`, `kV`, `Hz`, `kHz`, `W`, `kW`, `J`, `kJ`, `C`, `Pa`, `kPa`, `psi`, `ohm`, `Ohm`, `Nm`, and `RPM`/`rpm`. Symbols are case-sensitive. Compound unit expressions accept `*`, `/`, and integer exponents with magnitude up to 16. Unit names are reserved and cannot be reused as variable names. Unknown unit suffixes, incompatible dimensions, divide-by-zero, and invalid geometry dimensions are rejected; non-finite scalar results are preserved.

Geometry bindings cover constructors and commonly used properties/methods for WPILib `Pose2d`, `Translation2d`, `Rotation2d`, `Transform2d`, `Twist2d`, `Rectangle2d`, and `Ellipse2d`. Shape operations include containment, intersection, distance, nearest point, transforms, and rotation. An ellipse's `FocalPoints()` returns two points accessible with `[0]` and `[1]`. Geometry arguments are checked at the scalar-to-WPILib boundary. Evaluation accepts only a single read-only expression using the library's supported operators and functions; assignments, declarations, and other script operations are rejected.

For convenience, `Pose2d` supports `Distance(otherPose)` using its translation; this ignores rotation. For a pose's norm, use `pose.Translation().Norm()` or `pose.Translation().SquaredNorm()`.

Planar geometry values support WPILib's arithmetic and equality operators. Scaling accepts finite dimensionless scalars; division by zero is rejected. `Translation2d::Angle()` returns a rotation and rejects translations whose direction is undefined. Pose and translation `Nearest()` accept non-empty inline lists, for example `origin.Nearest([Pose2d(), target])`.

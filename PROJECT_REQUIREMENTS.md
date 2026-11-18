# Choreo Repository Requirements

## Purpose

Choreo is a cross-platform tool for authoring and generating time-optimized drivetrain trajectories for FRC robots. This document defines requirements for the whole repository.

The `document/` schemas define serialized data. [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md) covers the expression subsystem only. The 2027 architecture proposal is design context, not a complete specification.

## Scope

Included: graphical application; project and trajectory data; path editing and visualization; trajectory optimization and generation; CLI; ChoreoLib; expression evaluation; builds, releases, tests, and documentation.

Excluded: robot hardware control; a general-purpose scripting or plugin system; replacing WPILib; guarantees that every input has a feasible solution or that a robot can execute a trajectory safely.

## Requirements

### Application

- **APP-001** The application MUST provide a graphical interface to create, edit, generate, inspect, and save trajectories.
- **APP-002** Users MUST be able to create, open, and save projects and configure the robot and trajectories they contain.
- **APP-003** Users MUST be able to author paths with supported waypoint types, constraints, scopes, regions, and event markers.
- **APP-004** The application MUST visualize the field, path, constraints, and generated samples. It MUST support time-based playback or inspection.
- **APP-005** The application MUST report validation and generation failures with the affected object or setting when available.

### Data and Generation

- **DATA-001** Project and trajectory files MUST follow the `.chor` and `.traj` formats and schemas in `document/`.
- **DATA-002** The application MUST validate project data when it is loaded or changed. It MUST report invalid data rather than accept it as valid state.
- **GEN-001** The optimizer MUST seek to minimize trajectory time subject to the supported robot dynamics and path constraints. This requirement sets no numerical optimality tolerance.
- **GEN-002** The generator MUST support the drivetrain models, waypoint types, and constraints implemented and documented by the repository.
- **GEN-003** The generator MUST report infeasible or invalid inputs as failures, not successful trajectories.
- **GEN-004** Users MUST be able to cancel a running generation operation.
- **GEN-005** Successful generation MUST produce `.traj` data usable by ChoreoLib.
- **GEN-006** Split points and event markers MUST retain their documented relationship to trajectory segments and timing.

### CLI and Robot Integration

- **CLI-001** The CLI MUST generate named trajectories, or all trajectories when requested, from a project file.
- **CLI-002** The CLI and application MUST use compatible data and generation behavior.
- **CLI-003** The CLI MUST report invalid arguments, files, trajectory names, and generation inputs with a non-success status.
- **LIB-001** ChoreoLib MUST load and follow generated trajectories through the documented robot-side APIs.
- **LIB-002** ChoreoLib MUST expose the samples, segments, and event data required by documented robot workflows.

### Expressions

- **EXP-001** Supported expression-valued fields MUST be evaluated and checked against their required dimensions or geometry types.
- **EXP-002** The expression system MUST support documented units, math operations, and geometry operations.
- **EXP-003** Failed evaluation MUST report the affected field and MUST NOT partially update the project.
- **EXP-004** Evaluation MUST NOT provide arbitrary code execution, file access, or module loading.

### Quality and Delivery

- **QLT-001** Published application builds MUST support Windows x86_64 and aarch64, macOS x86_64 and arm64, and Linux x86_64 and aarch64.
- **QLT-002** Build and test instructions MUST cover the application, CLI, native libraries, and robot-side libraries.
- **QLT-003** Tests MUST cover schemas and data handling, expressions, generation, CLI behavior, and ChoreoLib behavior.
- **QLT-004** User documentation MUST cover installation, authoring, generation, saving, robot integration, and known limitations.
- **QLT-005** Documentation MUST state that users are responsible for validating trajectories and operating robots safely.

## Acceptance Workflows

- Create or open a project, configure a supported robot, author a constrained path, generate it, inspect it, and save the project and trajectory.
- Verify invalid project data and infeasible paths are reported as failures.
- Generate selected trajectories through the CLI and load a generated trajectory through ChoreoLib.
- Verify expressions accept compatible dimensions, reject mismatches, and identify the affected field on failure.
- Verify documented builds and tests for supported components and platform release targets.

## Not Specified Here

This document does not define schema fields, solver internals, numerical optimality tolerances, transport protocols, or unfinished server architecture. Compatibility and migration guarantees belong to the applicable file-format policy.

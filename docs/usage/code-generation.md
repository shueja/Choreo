# Code Generation (Java Only)
To enable or disable this feature, simply go to the "Code Generation" tab in the document settings and select a folder within your Java robot project's `src/main` directory where the files should be output.

!!! tip

    These generated Java files do not depend on ChoreoLib to work. They will be perfectly compatible with any Java project.



## Trajectory and Variable Naming Rules
### Character restrictions
To make the below code generation feature possible, and to simplify many aspects of the app, Choreo requires that trajectory and variable names are valid variable names (identifiers) in C++, Python, and Java. **Valid names can only contain letters (a-z, A-Z), numbers (0-9), and the underscore character (_). They cannot begin with a number.**
> Choreo's code generation will intentionally output errored code if this rule is broken, and the code will have a comment explaining the rule. The Choreo app will indicate any trajectories that need to be renamed.

### Reserved words
* Java keywords are not valid trajectory or variable names.
* There are many predefined terms (mostly units) in the Choreo expression parser (Math.JS), and inadvertently naming a variable one of these could have unexpected and hard-to-reverse consequences. Therefore, **variables cannot be named a Math.JS reserved word.** Choreo will give specific feedback if a name is invalid, but [here](https://mathjs.org/docs/datatypes/units.html#reference) is the list of units and constants for reference. 


## Variables

Choreo can output a Java file containing variables defined in the Choreo GUI. This allows information such as predefined poses and robot characteristics to be shared between the robot code and the GUI, while ensuring consistency. Values will use the Java units library when possible.

### Example:
```java
import frc.robot.wherever.ChoreoVars;

Pose2d pose = ChoreoVars.Poses.myPoseVariable;
Distance length = ChoreoVars.myLengthVariable;
double simpleNumber = ChoreoVars.myNumberVariable;
```

## Trajectory Names

Choreo can also output a Java file listing the name, total time, and blue-alliance start and end poses of each trajectory. Each trajectory is represented as a static constant of the `ChoreoTraj` record. The file is rewritten when the project is loaded, and when paths are generated, renamed, or deleted.

### Example: ChoreoLib
By creating trajectories with Java static constants instead of strings, references to deleted, misspelled, or nonexistent trajectories are caught at compile-time instead of runtime.
```java
import static frc.robot.wherever.ChoreoTraj.*;

AutoRoutine routine = factory.newRoutine("Three Piece");
// Instead of routine.trajectory("StationToReef4"), do:
AutoTrajectory traj = StationToReef4.asAutoTraj(routine);
// The static constants for trajectory segments use the naming scheme "overall$i", where i starts at 0.
AutoTrajectory firstSegment = StationToReef4$0.asAutoTraj(routine);
```

### Example: Fetching Metadata
The ```ChoreoTraj``` class allows for trajectory data to be accessed without
loading an ```AutoTrajectory```, which is costly due to JSON parsing overhead.
```java
import frc.robot.wherever.ChoreoTraj;
import static frc.robot.wherever.ChoreoTraj.*;

Pose2d station = StationToReef4.initialPoseBlue();
Pose2d reef4 = StationToReef4.endPoseBlue();
double stationToReef4Time = StationToReef4.totalTimeSecs();
// Furthermore, if you decide to compute trajectory names during runtime,
// You can fetch their metadata like so:
String computedTrajName = "StationToReef5";
ChoreoTraj metadata = ALL_TRAJECTORIES.get(computedTrajName);
Pose2d reef5 = metadata.initialPoseBlue();
double stationToReef5Time = metadata.totalTimeSecs();
// This will throw an error if the segment doesn't exist.
Pose2d timeOfThirdSegment = metadata.segment(3).totalTimeSecs();
```

### Example: PathPlannerLib
PathPlannerLib can also leverage the safety from the ```ChoreoTraj``` class.
```java
import static frc.robot.wherever.ChoreoTraj.*;

PathPlannerPath traj = PathPlannerPath.fromChoreoTrajectory(StationToReef4.name());
PathPlannerPath firstSegment = PathPlannerPath.fromChoreoTrajectory(StationToReef4.name(), 1);
```

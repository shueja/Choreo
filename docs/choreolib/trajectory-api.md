# Trajectory API

!!! tip
    Java users are recommended to use an [Auto Factory](./auto-factory.md) instead of the Trajectory API for loading and following trajectories, as it provides a more user-friendly experience for developing complex autonomous routines.

To begin utilizing trajectories created by Choreo, first load a trajectory from its corresponding file. Java uses [Trajectory](/api/choreolib/java/choreo/trajectory/Trajectory.html); C++ exposes WPILib trajectory and sample types.

=== "Java"

    ```java
    // Loads from deploy/choreo/myTrajectory.traj
    // Optional.empty() is returned if the file does not exist or is invalid
    var trajectory = Choreo.loadTrajectory("myTrajectory");
    ```

=== "C++"

    ```cpp
    // Load a v4 file; an exception is thrown if it is missing or invalid.
    auto path = std::filesystem::path{wpi::filesystem::GetDeployDirectory()} /
                "choreo" / "myTrajectory.traj";
    auto trajectory = choreo::Choreo::LoadTrajectory(path);
    const auto& samples = std::get<std::optional<wpi::math::HolonomicTrajectory>>(
        trajectory.output.trajectory);
    ```

    Use `wpi::math::DifferentialTrajectory` for a differential drive. Unresolved
    events are omitted and reported in `trajectory.warnings`.

=== "Python"

    ```py
    # Loads from deploy/choreo/myTrajectory.traj
    # ValueError is thrown if the file does not exist or is invalid
    try:
      trajectory = choreo.load_swerve_trajectory("myTrajectory") # (1)
    except ValueError:
      # If the trajectory is not found, ChoreoLib already prints to DriverStation
      pass
    ```

    1. Use `load_differential_trajectory` instead of `load_swerve_trajectory` if the robot uses a differential (tank) drive

!!! warning
    Trajectories should always be loaded at startup, not when the autonomous period begins. Loading trajectories is a blocking operation, and larger trajectories may take multiple seconds to load on a RoboRIO, cutting into the time a robot has to run its autonomous routine in a match.

Trajectories can be sampled at arbitrary timestamps using `sampleAt()`, which will return an interpolated sample that can be consumed by the user for trajectory following. Additionally, `getInitialPose()` and `getFinalPose()` can be used to retrieve a `Pose2d` representing the starting and ending point of the trajectory, respectively.

See [Getting Started](./getting-started.md/#setting-up-the-drive-subsystem) for more details on how to implement a trajectory follower in your drive subsystem.

=== "Java"

    ```java title="Robot.java"
    public class Robot extends TimedRobot {
        // Loads a swerve trajectory, alternatively use DifferentialSample if the robot is tank drive
        private final Optional<Trajectory<SwerveSample>> trajectory = Choreo.loadTrajectory("myTrajectory");

        private final Drive driveSubsystem = new Drive();
        private final Timer timer = new Timer();

        @Override
        public void autonomousInit() {
            if (trajectory.isPresent()) {
                // Get the initial pose of the trajectory
                Optional<Pose2d> initialPose = trajectory.get().getInitialPose(isRedAlliance());

                if (initialPose.isPresent()) {
                    // Reset odometry to the start of the trajectory
                    driveSubsystem.resetOdometry(initialPose.get());
                }
            }

            // Reset and start the timer when the autonomous period begins
            timer.restart();
        }

        @Override
        public void autonomousPeriodic() {
            if (trajectory.isPresent()) {
                // Sample the trajectory at the current time into the autonomous period
                Optional<SwerveSample> sample = trajectory.get().sampleAt(timer.get(), isRedAlliance());

                if (sample.isPresent()) {
                    driveSubsystem.followTrajectory(sample);
                }
            }
        }

        private boolean isRedAlliance() {
            return MatchState.getAlliance().orElse(Alliance.BLUE).equals(Alliance.RED);
        }
    }
    ```

=== "C++"

    === "Source"

        ```cpp title="Robot.cpp"
        void Robot::AutonomousInit() {
            const auto& path = std::get<std::optional<wpi::math::HolonomicTrajectory>>(
                trajectory.output.trajectory);
            drive.ResetOdometry(path->Start().pose);

            // Reset and start the timer when the autonomous period begins
            timer.Restart();
        }

        void Robot::AutonomousPeriodic() {
            const auto& path = std::get<std::optional<wpi::math::HolonomicTrajectory>>(
                trajectory.output.trajectory);
            drive.FollowTrajectory(path->SampleAt(timer.Get()));
        }
        ```

    === "Header"

        ```cpp title="Robot.h"
        class Robot : public wpi::TimedRobot {
            public:
                void AutonomousInit() override;
                void AutonomousPeriodic() override;

            private:
                choreo::Trajectory trajectory = choreo::Choreo::LoadTrajectory(
                    std::filesystem::path{wpi::filesystem::GetDeployDirectory()} /
                    "choreo" / "myTrajectory.traj");

                Drive drive;
                wpi::Timer timer;
        };
        ```

    This example uses the stored field coordinates directly. Apply the
    appropriate alliance transform before following the trajectory when needed.

=== "Python"

    ```py
    class MyRobot(wpilib.TimedRobot):
        def __init__(self):
            super().__init__()

            # Loads a swerve trajectory, alternatively use load_differential_trajectory if the robot is tank drive
            try:
                self.trajectory = choreo.load_swerve_trajectory("myTrajectory")
            except ValueError:
                self.trajectory = None

            self.drive_subsystem = Drive()
            self.timer = wpilib.Timer()

        def autonomous_init(self):
            if self.trajectory:
                # Get the initial pose of the trajectory
                initial_pose = self.trajectory.get_initial_pose(self.is_red_alliance())

                if initial_pose:
                    # Reset odometry to the start of the trajectory
                    self.drive_subsystem.reset_odometry(initial_pose)

            # Reset and start the timer when the autonomous period begins
            self.timer.restart()

        def autonomous_periodic(self):
            if self.trajectory:
                # Sample the trajectory at the current time into the autonomous period
                sample = self.trajectory.sample_at(self.timer.get(), self.is_red_alliance())

                if sample:
                    self.drive_subsystem.follow_trajectory(sample)

        def is_red_alliance(self):
            return wpilib.MatchState.getAlliance() == wpilib.MatchState.Alliance.RED
    ```

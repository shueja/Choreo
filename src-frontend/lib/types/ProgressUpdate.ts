import { DifferentialSample } from "./sample/DifferentialSample";
import { SwerveSample } from "./sample/SwerveSample";

export interface SwerveTrajectoryProgressUpdate {
  type: "swerveTrajectory";
  update: SwerveSample[];
}

export interface DifferentialTrajectoryProgressUpdate {
  type: "differentialTrajectory";
  update: DifferentialSample[];
}

export type ProgressUpdate = SwerveTrajectoryProgressUpdate | DifferentialTrajectoryProgressUpdate;
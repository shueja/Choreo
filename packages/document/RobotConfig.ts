import { ExprOrNumber } from "./math/Expr";

export interface Bumper<T extends ExprOrNumber> {
  front: T;
  back: T;
  side: T;
}

export interface Module<T extends ExprOrNumber> {
  x: T;
  y: T;
}

export interface RobotConfig<T extends ExprOrNumber> {
  frontLeft: Module<T>;
  backLeft: Module<T>;
  mass: T;
  inertia: T;
  gearing: T;
  radius: T;
  /// motor rad/s
  vmax: T;
  /// motor N*m
  tmax: T; // N*m
  cof: T;
  bumper: Bumper<T>;
  differentialTrackWidth: T;
}
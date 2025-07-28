import { Constraint } from "./constraint/Constraint";
import { ExprOrNumber } from "./math/Expr";
import { Waypoint } from "./waypoint/Waypoint";

export interface Params<T extends ExprOrNumber> {
  waypoints: Waypoint<T>[];
  constraints: Constraint[];
  targetDt: T;
}
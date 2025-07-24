import { DimensionName } from "./Dimensions";
import { Expr } from "./Expr";

export interface Variable {
  dimension: DimensionName;
  var: Expr;
}

export interface PoseVariable {
  x: Expr;
  y: Expr;
  heading: Expr;
}

export interface Variables {
  expressions: Record<string, Variable>;
  poses: Record<string, PoseVariable>;
}
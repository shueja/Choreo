import { DimensionName } from "./Dimensions";
import { Expr, ExprOrNumber } from "./Expr";

export interface Variable<T extends ExprOrNumber> {
  dimension: DimensionName;
  var: T;
}

export interface PoseVariable<T extends ExprOrNumber> {
  x: T;
  y: T;
  heading: T;
}

export interface Variables {
  expressions: Record<string, Variable<Expr>>;
  poses: Record<string, PoseVariable<Expr>>;
}
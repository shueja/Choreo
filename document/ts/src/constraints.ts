import type { Expr } from "./index";

export type ConstraintDataTypeMap = {
  StopPoint: {};
  MaxVelocity: { max: Expr };
  MaxAcceleration: { max: Expr };
  MaxAngularVelocity: { max: Expr };
  PointAt: {
    x: Expr;
    y: Expr;
    tolerance: Expr;
    flip: boolean;
  };
  KeepInCircle: {
    x: Expr;
    y: Expr;
    r: Expr;
  };
  KeepInRectangle: {
    x: Expr;
    y: Expr;
    w: Expr;
    h: Expr;
  };
  KeepInLane: {
    tolerance: Expr;
  };
  KeepOutCircle: {
    x: Expr;
    y: Expr;
    r: Expr;
  };
};

export type DataPropsList = { [key: string]: Expr | boolean };

export interface IConstraintData<
  Name extends keyof ConstraintDataTypeMap,
  Props extends DataPropsList = ConstraintDataTypeMap[Name]
> {
  readonly type: Name;
  props: Props;
}

export type DataMap = {
  [Name in keyof ConstraintDataTypeMap]: IConstraintData<
    Name,
    ConstraintDataTypeMap[Name]
  >;
};

export type ConstraintData = DataMap[keyof DataMap];

export type ConstraintKey = keyof DataMap;

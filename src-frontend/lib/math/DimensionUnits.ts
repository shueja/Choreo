import { Unit } from "mathjs";
import { DimensionName, DimensionNameExt } from "../types/math/Dimensions";
import { Units } from "./Units";

export default {

  Number: undefined,
  Length: Units.Meter,
  Angle: Units.Radian,
  LinVel: Units.MeterPerSecond,
  LinAcc: Units.MeterPerSecondSquared,
  AngVel: Units.RadianPerSecond,
  AngAcc: Units.RadianPerSecondSquared,
  Time: Units.Second,
  Mass: Units.Kg,
  MoI: Units.KgM2,
  Torque: Units.NewtonMeter,
  Pose: undefined
} as const satisfies {[key in DimensionNameExt]: Unit | undefined}
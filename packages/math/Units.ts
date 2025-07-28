import {math} from "./math"
export const Units = {
  Second: math.unit("s"),
  Meter: math.unit("m"),
  MeterPerSecond: math.unit("m/s"),
  MeterPerSecondSquared: math.unit("m/s^2"),
  Radian: math.unit("rad"),
  RadianPerSecond: math.unit("rad/s"),
  RadianPerSecondSquared: math.unit("rad/s^2"),
  KgM2: math.unit("kg m^2"),
  Newton: math.unit("N"),
  NewtonMeter: math.unit("N*m"),
  Kg: math.unit("kg"),
  RPM: math.createUnit("RPM", "1 cycle/min", { aliases: ["rpm"] })
};
// not sure why the alias above doesn't work
math.createUnit("rpm", "1 RPM");
import { Sample } from "./Sample";

export interface SwerveSample extends Sample {
  t: number;
  x: number;
  y: number;
  heading: number;
  vx: number;
  vy: number;
  omega: number;
  ax: number;
  ay: number;
  alpha: number;
  fx?: [number, number, number, number];
  fy?: [number, number, number, number];
}

export type SwerveSampleType = "Swerve";
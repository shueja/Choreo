import { Sample } from "./Sample";

export interface DifferentialSample extends Sample {
  t: number;
  x: number;
  y: number;
  heading: number;
  vl: number;
  vr: number;
  omega: number;
  al: number;
  ar: number;
  fl: number;
  fr: number;
}

export type DifferentialSampleType = "Differential";
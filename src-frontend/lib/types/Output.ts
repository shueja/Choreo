import { DifferentialSample } from "./sample/DifferentialSample";
import { SampleType } from "./sample/SampleType";
import { SwerveSample } from "./sample/SwerveSample";

export interface Output {
  sampleType: SampleType | undefined;
  waypoints: number[];
  samples: SwerveSample[] | DifferentialSample[];
  splits: number[];
}
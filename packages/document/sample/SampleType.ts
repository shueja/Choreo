import { DifferentialSample, DifferentialSampleType } from "./DifferentialSample";
import { SwerveSample, SwerveSampleType } from "./SwerveSample";

export type SampleType = DifferentialSampleType | SwerveSampleType;

export type SampleArray = DifferentialSample[] | SwerveSample[];
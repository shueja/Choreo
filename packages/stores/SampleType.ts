import { SampleType } from "@choreo/document/sample/SampleType";
import { types } from "mobx-state-tree";

export const ISampleType = types.enumeration<SampleType>([
  "Swerve",
  "Differential"
]);
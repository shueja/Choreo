import { ConstraintData } from "./ConstraintDefinitions";

export type WaypointIDX = number | "first" | "last";

export type WaypointUUID = "first" | "last" | { uuid: string };

export interface Constraint {
  from: WaypointIDX;
  to?: WaypointIDX;
  data: ConstraintData;
  enabled: boolean;
}
import { Command } from "./Command";
import { Expr } from "./math/Expr";
import { WaypointIndex } from "./waypoint/WaypointIndex";

export type EventMarkerData = {
  target: WaypointIndex | undefined;
  offset: Expr;
  /**
   * The timestamp along the trajectory of the waypoint this marker targeted on the last generation.
   */
  targetTimestamp: number | undefined;
};

export interface EventMarker {
  name: string;
  from: EventMarkerData;
  event: Command;
}
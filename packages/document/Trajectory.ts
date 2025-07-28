
import { Params } from "./Params";
import { EventMarker } from "./EventMarker";
import { Expr } from "./math/Expr";
import { Output } from "./Output";
import { TRAJ_SCHEMA_VERSION } from "./version/TrajSchemaVersion";

export interface Trajectory {
  name: string;
  version: typeof TRAJ_SCHEMA_VERSION;
  params: Params<Expr>;
  snapshot: Params<number>;
  trajectory: Output;
  events: EventMarker[];
}
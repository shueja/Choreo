import { Instance, types } from "mobx-state-tree";
import { EventMarkerStore } from "../EventMarkerStore";
import { ChoreoTrajectoryStore } from "./ChoreoTrajectoryStore";
import { ParamsStore } from "./ParamsStore";
import { Params } from "@choreo/document/Params";
import { TRAJ_SCHEMA_VERSION } from "@choreo/document/version/TrajSchemaVersion";
import { Trajectory } from "@choreo/document/Trajectory";

export const PathStore = types
  .model("PathStore", {
    snapshot: types.frozen<Params<number>>(),
    params: ParamsStore,
    trajectory: ChoreoTrajectoryStore,
    markers: types.array(EventMarkerStore),
    name: "",
    uuid: types.identifier
  })
    .views((self) => {
        return {
        canGenerate(): boolean {
            return self.params.waypoints.length >= 2;// && !self.ui.generating;
        },
        canExport(): boolean {
            return self.trajectory.samples.length >= 2;
        },
        get serialize(): Trajectory {
            const markers = self.markers.map((m) => m.serialize);
            return {
            name: self.name,
            version: TRAJ_SCHEMA_VERSION,
            params: self.params.serialize,
            trajectory: self.trajectory.serialize,
            snapshot: self.snapshot,
            events: markers
            };
        }
        };
    })
    .actions((self)=>({
        deserialize(ser: Trajectory) {
        self.name = ser.name;
        self.snapshot = ser.snapshot;
        self.params.deserialize(ser.params);
        self.trajectory.deserialize(ser.trajectory);
        self.markers.clear();
        // ser.events.forEach((m) => {
        //   self.addEventMarker(m);
        // });
        // ServerCommands.trajectoryUpToDate(self.serialize).then((upToDate) =>
        //   self.ui.setUpToDate(upToDate)
        // );
      }
    }))
export type IPathStore = Instance<typeof PathStore>;
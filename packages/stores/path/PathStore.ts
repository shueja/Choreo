import { destroy, Instance, types } from "mobx-state-tree";
import { EventMarkerStore, IEventMarkerStore } from "../EventMarkerStore";
import { ChoreoTrajectoryStore, createTrajectoryStore } from "./ChoreoTrajectoryStore";
import { createParamsStore, ParamsStore } from "./ParamsStore";
import { Params } from "@choreo/document/Params";
import { TRAJ_SCHEMA_VERSION } from "@choreo/document/version/TrajSchemaVersion";
import { Trajectory } from "@choreo/document/Trajectory";
import { IVariables, VariablesStore } from "../VariablesStore";
import { Waypoint } from "@choreo/document/waypoint/Waypoint";
import { Expr } from "@choreo/document/math/Expr";
import { DEFAULT_WAYPOINT, IWaypointStore ,createWaypointStore} from "../WaypointStore";
import { EventMarker } from "@choreo/document/EventMarker";
import { VariablesScopeGetter } from "@choreo/math/VariablesScope";

export const PathStore = types
  .model("PathStore", {
    snapshot: types.frozen<Params<number>>(),
    params: ParamsStore,
    trajectory: ChoreoTrajectoryStore,
    markers: types.array(EventMarkerStore),
    name: "",
    uuid: types.identifier
  })
  .volatile((self)=>({
    getVariables: ()=>new Map<string, any>()
  }))
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
        setGetVariables(getVariables: VariablesScopeGetter) {
          self.getVariables = getVariables
        },
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
    .actions((self) => {
    return {
      deleteMarkerUUID(uuid: string) {
        const index = self.markers.findIndex((m) => m.uuid === uuid);
        if (index >= 0 && index < self.markers.length) {
          destroy(self.markers[index]);
          // if (self.markers.length === 0) {
          //   return;
          // } else if (self.markers[index - 1]) {
          //   getEnv<Env>(self).select(self.markers[index - 1]);
          // } else if (self.markers[index + 1]) {
          //   getEnv<Env>(self).select(self.markers[index + 1]);
          // }
        }
      },
      // addEventMarker(marker?: EventMarker): IEventMarkerStore {
      //   const m = marker ?? DEFAULT_EVENT_MARKER;
      //   const toAdd = createEventMarkerStore(m);

      //   self.markers.push(toAdd);
      //   toAdd.deserialize(m, getEnv<Env>(self).create.CommandStore);
      //   return toAdd;
      // },
      setSnapshot(snap: Params<number>) {
        self.snapshot = snap;
      },
      setName(name: string) {
        self.name = name;
      }
    };
  })
export type IPathStore = Instance<typeof PathStore>;
export function createPathStore(variables: IVariables) : IPathStore {
const usedName = "No Path";
        const newUUID = crypto.randomUUID();
        const params = createParamsStore(variables);
        params.setGetVariables(()=>variables.scope);
        const path = PathStore.create({
          uuid: newUUID,
          name: usedName,
          params,

          snapshot: {
            waypoints:[],
            constraints:[],
            targetDt: params.targetDt.value
          },
          trajectory: createTrajectoryStore(),
          markers: []
        });
        path.setGetVariables(()=>variables.scope);
        return path;
}
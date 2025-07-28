import { types } from "mobx-state-tree";
import { IChoreoTrajectoryStore } from "./path/ChoreoTrajectoryStore";
import { ExpressionStore } from "./ExpressionStore";
import { IWaypointStore } from "./WaypointStore";
import { findUUIDIndex, getByWaypointID, toWaypointIndex, toWaypointUUID } from "./path/utils";
import { WaypointUUID } from "@choreo/document/waypoint/WaypointUUID";
import { EventMarkerData } from "@choreo/document/EventMarker";
import { WaypointScope } from "./ConstraintStore";
import { WaypointIndex } from "@choreo/document/waypoint/WaypointIndex";

export const EventMarkerDataStore = types
  .model("EventMarkerData", {
    target: types.maybe(WaypointScope),
    targetTimestamp: types.maybe(types.number),
    offset: ExpressionStore,
    uuid: types.identifier
  })
  .volatile((self) => ({
    /** Just used to preserve the index of the target during generation */
    trajectoryTargetIndex: undefined as number | undefined
  }))
  .views((self) => ({
    get timestamp(): number | undefined {
      if (self.targetTimestamp === undefined) {
        return undefined;
      }
      return self.targetTimestamp + self.offset.value;
    },
    getTargetIndex(points: IWaypointStore[]): number | undefined {
      const startScope = self.target;
      if (startScope === undefined) {
        return undefined;
      }
      const waypoint = getByWaypointID(startScope, points);
      if (waypoint === undefined) return undefined;
      return findUUIDIndex(waypoint.uuid, points);
    }
  }))
  .views((self) => ({
    get serialize(): EventMarkerData {
      const points:IWaypointStore[] = [];
      //const points = self.getPath().params.waypoints;
      return {
        target: toWaypointIndex(self.target, points),
        offset: self.offset.serialize,
        targetTimestamp: self.targetTimestamp
      };
    }
  }))
  .actions((self) => ({
    deserialize(ser: EventMarkerData, getWaypointUUID: (index: WaypointIndex | undefined)=>WaypointUUID) {
      //const points = self.getPath().params.waypoints;
      self.target = getWaypointUUID(ser.target);
      self.targetTimestamp = self.targetTimestamp ?? undefined;
      self.offset.deserialize(ser.offset);
    },
    setTarget(target: WaypointUUID) {
      self.target = target;
    },
    setTargetTimestamp(timestamp: number | undefined) {
      self.targetTimestamp = timestamp;
    },
    setTrajectoryTargetIndex(index: number | undefined) {
      self.trajectoryTargetIndex = index;
    }
  }))
  .views((self) => ({
    /**
     *
     * @returns Returns undefined if the marker does not have both a timestamp and a target timestamp.
     * Otherwise, returns whether the target waypoint and the marker timestamp are on the same split part.
     */
    isInSameSegment(traj: IChoreoTrajectoryStore): boolean | undefined {
      let retVal: boolean | undefined = true;
      const targetTimestamp = self.targetTimestamp;
      const timestamp = self.timestamp;
      if (targetTimestamp === undefined || timestamp === undefined) {
        retVal = undefined;
        return undefined;
      } else if (self.offset.value == 0) {
        return true;
      } else {
        const splitTimes = traj.splits.map((idx) => traj.samples[idx]?.t);
        [0, ...splitTimes, traj.getTotalTimeSeconds()].forEach(
          (stopTimestamp) => {
            if (
              (targetTimestamp < stopTimestamp && timestamp > stopTimestamp) ||
              (targetTimestamp > stopTimestamp && timestamp < stopTimestamp)
            ) {
              retVal = false;
            }
          }
        );
      }
      return retVal;
    }
  }));
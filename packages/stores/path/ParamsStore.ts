

// When adding new fields, consult
// https://choreo.autos/contributing/schema-upgrade/

import { destroy, Instance, types} from "mobx-state-tree";
import {moveItem} from "mobx-utils";
import { createWaypointStore, DEFAULT_WAYPOINT, IWaypointStore, WaypointStore } from "../WaypointStore";
import { ConstraintStore, createConstraintStore, IConstraintStore, IWaypointScope } from "../ConstraintStore";
import { ExpressionStore } from "../ExpressionStore";
import { Expr } from "@choreo/document/math/Expr";
import { toWaypointIndex, toWaypointUUID } from "./utils";
import { ConstraintKey, DataMap } from "@choreo/document/constraint/ConstraintDefinitions";
import { Waypoint } from "@choreo/document/waypoint/Waypoint";
import { Params } from "@choreo/document/Params";
import { Constraint } from "@choreo/document/constraint/Constraint";
import { IVariables } from "../VariablesStore";
import { VariablesScopeGetter } from "@choreo/math/VariablesScope";

// to see all the places that change with every schema upgrade.
export const ParamsStore = types
  .model("ChoreoPathStore", {
    waypoints: types.array(WaypointStore),
    constraints: types.array(ConstraintStore),
    targetDt: ExpressionStore
  })
  .volatile((self)=>({
    getVariables: ()=>new Map<string, any>()
  }))
  .views((self) => ({
    get nonGuessPoints() {
      return self.waypoints.filter((waypoint) => !(waypoint.type == 2));
    },
    get nonGuessOrEmptyPoints() {
      return self.waypoints.filter((waypoint) => waypoint.type == 2);
    }
  }))
  .views((self) => ({
    get serialize(): Params<Expr> {
      return {
        waypoints: self.waypoints.map((w) => w.serialize),
        constraints: self.constraints.flatMap((constraint) => {
          const con = constraint;
          const from = toWaypointIndex(con.from, self.waypoints)!;
          const to = toWaypointIndex(con.to, self.waypoints);
          const toReturn: Constraint = {
            data: con.data.serialize,
            enabled: con.enabled,
            from,
            to
          };
          return toReturn;
        }),
        targetDt: self.targetDt.serialize
      };
    }
  }))
  .actions((self) => ({
    setGetVariables(getVariables: VariablesScopeGetter) {
      self.getVariables = getVariables
    },
    addConstraint<K extends ConstraintKey>(
      key: K,
      enabled: boolean,
      from: IWaypointScope,
      to?: IWaypointScope,
      data: Partial<DataMap[K]["props"]> = {}
    ): Instance<typeof ConstraintStore> | undefined {
      self.constraints.push(
        createConstraintStore(key, data, enabled, self.getVariables, from, to)
      );
      const store = self.constraints[self.constraints.length - 1];
      store.data.deserPartial(data);
      return store;
    },
    // selectOnly(selectedIndex: number) {
    //   self.waypoints.forEach((point, index) => {
    //     point.setSelected(selectedIndex == index);
    //   });
    // },
    reorderWaypoint(startIndex: number, endIndex: number) {
      moveItem(self.waypoints, startIndex, endIndex);
    },
    addWaypoint(waypoint?: Partial<Waypoint<Expr>>): IWaypointStore {
        const w =           createWaypointStore(
            Object.assign({ ...DEFAULT_WAYPOINT }, waypoint),
            self.getVariables
          )
        self.waypoints.push(
w
        );

        return w;
      },
    deleteWaypoint(id: number | string) {
      let index = 0;
      if (typeof id === "string") {
        index = self.waypoints.findIndex((point) => point.uuid === id);
        if (index == -1) return;
      } else {
        index = id;
      }
      if (self.waypoints[index] === undefined) {
        return;
      }
      const uuid = self.waypoints[index]?.uuid;
      //getEnv<Env>(self).select(undefined);

      // clean up constraints
      self.constraints = self.constraints.flatMap(
        (constraint: IConstraintStore) => {
          const from = constraint.from;
          const to = constraint.to;
          // delete waypoint-scope referencing deleted point directly.
          if (
            to === undefined &&
            from instanceof Object &&
            Object.hasOwn(from, "uuid") &&
            from.uuid === uuid
          ) {
            return [];
          }
          // delete zero-segment-scope referencing deleted point directly.
          if (to !== undefined) {
            const deletedIndex = index;
            const firstIsUUID =
              from instanceof Object && Object.hasOwn(from, "uuid");
            const secondIsUUID =
              to instanceof Object && Object.hasOwn(to, "uuid");
            let startIndex = constraint.getStartWaypointIndex(self.waypoints);
            let endIndex = constraint.getEndWaypointIndex(self.waypoints);
            // start/end index being undefined, given that scope is length2, means that
            // the constraint refers to an already-missing waypoint. Skip these and let the user
            // retarget them.

            if (startIndex === undefined || endIndex === undefined) {
              return constraint;
            }
            // Delete zero-length segments that refer directly and only to the waypoint
            if (
              startIndex == deletedIndex &&
              endIndex == deletedIndex &&
              (firstIsUUID || secondIsUUID)
            ) {
              return [];
            }
            // deleted start? move new start forward till first constrainable waypoint
            if (deletedIndex == startIndex && firstIsUUID) {
              startIndex++;
            } else if (deletedIndex == endIndex && secondIsUUID) {
              endIndex--;
              // deleted end? move new end backward till first constrainable waypoint
            }
            // if we shrunk to a single point and the constraint can't be wpt scope, delete constraint
            if (
              !constraint.data.def.wptScope &&
              endIndex == startIndex &&
              firstIsUUID &&
              secondIsUUID
            ) {
              return [];
            } else {
              // update
              constraint.setFrom(
                firstIsUUID ? { uuid: self.waypoints[startIndex].uuid } : from
              );
              constraint.setTo(
                secondIsUUID ? { uuid: self.waypoints[endIndex].uuid } : to
              );
              return constraint;
            }
          }
          return constraint;
        }
      ) as typeof self.constraints;

      destroy(self.waypoints[index]);
      if (self.waypoints.length === 0) {
        return;
      } else if (self.waypoints[index - 1]) {
        //self.waypoints[index - 1].setSelected(true);
      } else if (self.waypoints[index + 1]) {
        //self.waypoints[index + 1].setSelected(true);
      }
    },
    deleteConstraint(id: string | number) {
      let index = 0;
      if (typeof id === "string") {
        index = self.constraints.findIndex((point) => point.uuid === id);
        if (index == -1) return;
      } else {
        index = id;
      }
      //getEnv<Env>(self).select(undefined);

      if (self.constraints.length === 1) {
        // no-op
      } else if (self.constraints[index - 1]) {
        //self.constraints[index - 1].setSelected(true);
      } else if (self.constraints[index + 1]) {
        //self.constraints[index + 1].setSelected(true);
      }
      destroy(self.constraints[index]);
    }
  }))
  .actions((self) => ({
    deserialize(ser: Params<Expr>) {
      self.waypoints.clear();
      ser.waypoints.forEach((point: Waypoint<Expr>, _index: number): void => {
        const waypoint = self.addWaypoint();
        waypoint.deserialize(point);
      });
      self.constraints.clear();
      ser.constraints.forEach((saved: Constraint) => {
        const from = toWaypointUUID(saved.from, self.waypoints);
        if (from === undefined) {
          return;
        }
        const to = toWaypointUUID(saved.to, self.waypoints);
        self.addConstraint(
          saved.data.type,
          saved.enabled,
          from,
          to,
          saved.data.props
        );
      });
      self.targetDt.deserialize(ser.targetDt);
    }
  }));

export type IParamsStore = Instance<typeof ParamsStore>;

export function createParamsStore(variables: IVariables) {
  return ParamsStore.create(
         {
            constraints: [],
            waypoints: [],
            targetDt: variables.createExpression("0.05 s", "Time")
          }
  );
}
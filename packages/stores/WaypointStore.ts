import { Instance, getEnv, getParent, isAlive, types } from "mobx-state-tree";
import { Expr } from "@choreo/document/math/Expr"
import {Waypoint} from "@choreo/document/waypoint/Waypoint"
//import { Env } from "./Env";
import { createExpressionStore, ExpressionStore } from "./ExpressionStore";

export const DEFAULT_WAYPOINT: Waypoint<number> = {
  x: 0,
  y: 0,
  heading: 0,
  fixTranslation: true,
  fixHeading: true,
  intervals: 40,
  split: false,
  overrideIntervals: false
};
// When adding new fields, consult
// https://choreo.autos/contributing/schema-upgrade/
// to see all the places that change with every schema upgrade.
export const WaypointStore = types
  .model("WaypointStore", {
    x: ExpressionStore,
    y: ExpressionStore,
    heading: ExpressionStore,
    fixTranslation: true,
    fixHeading: true,
    intervals: 40,
    overrideIntervals: false,
    split: false,
    uuid: types.identifier
  })
  .views((self) => {
    return {
      get type(): number {
        if (self.fixHeading) {
          return 0; // Full
        } else if (self.fixTranslation) {
          return 1; // Translation
        } else {
          return 2; // Empty
        }
      },
      get serialize(): Waypoint<Expr> {
        return {
          x: self.x.serialize,
          y: self.y.serialize,
          heading: self.heading.serialize,
          fixTranslation: self.fixTranslation,
          fixHeading: self.fixHeading,
          intervals: self.intervals,
          overrideIntervals: self.overrideIntervals,
          split: self.split
        };
      }
    };
  })
//   .views((self) => ({
//     // isLast(): boolean {
//     //   try {
//     //     const list = getParent<IWaypointStore[]>(self);
//     //     return list[list.length - 1]?.uuid === self.uuid;
//     //   } catch (e) {
//     //     console.error(e);
//     //     return false;
//     //   }
//     // }
//   }))
  .actions((self) => {
    return {
      deserialize(point: Waypoint<Expr>) {
        self.x.deserialize(point.x);
        self.y.deserialize(point.y);
        self.heading.deserialize(point.heading);
        self.fixTranslation = point.fixTranslation;
        self.fixHeading = point.fixHeading;
        self.intervals = point.intervals;
        self.overrideIntervals = point.overrideIntervals;
        self.split = point.split;
      },
      setFixTranslation(fixTranslation: boolean) {
        self.fixTranslation = fixTranslation;
      },
      setFixHeading(fixHeading: boolean) {
        self.fixHeading = fixHeading;
      },
    //   setSelected(selected: boolean) {
    //     if (selected && !self.selected) {
    //       getEnv<Env>(self).select(
    //         getParent<IWaypointStore[]>(self)?.find(
    //           (point) => self.uuid == point.uuid
    //         )
    //       );
    //     }
    //   },
      setIntervals(count: number) {
        self.intervals = count;
      },
      setOverrideIntervals(override: boolean) {
        self.overrideIntervals = override;
      },
      setSplit(split: boolean) {
        self.split = split;
      }
    };
  })
  .actions((self) => ({
    setType(type: number) {
      self.setFixHeading(type == 0);
      self.setFixTranslation(type == 0 || type == 1);
    }
  }))
  .views((self) => ({
    copyToClipboard(evt: ClipboardEvent) {
      console.info("copying waypoint to", evt.clipboardData);
      const content = JSON.stringify({
        dataType: "choreo/waypoint",
        ...self.serialize
      });
      evt.clipboardData?.setData("text/plain", content);
      console.log(evt.clipboardData);
    }
  }));
export type IWaypointStore = Instance<typeof WaypointStore>;
export function createWaypointStore(
  waypoint: Waypoint<Expr>,
  getScope: ()=>Map<string, any>
) {
  const w = WaypointStore.create({
    ...waypoint,
    x: createExpressionStore(waypoint.x, "Length", getScope),
    y: createExpressionStore(waypoint.y, "Length", getScope),
    heading: createExpressionStore(waypoint.heading, "Angle", getScope),
    uuid: crypto.randomUUID()
  });
  return w;
}

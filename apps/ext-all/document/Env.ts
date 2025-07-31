import {IWaypointStore, WaypointStore} from "@choreo/stores/WaypointStore"
import {IConstraintStore, ConstraintStore, IWaypointScope, constraintStoreConstructor} from "@choreo/stores/ConstraintStore"
import {IEventMarkerStore, EventMarkerStore, createEventMarkerStore} from "@choreo/stores/EventMarkerStore"

import { Instance, types } from "mobx-state-tree";
import { UndoManager } from "mst-middlewares";
import { SampleType } from "@choreo/document/sample/SampleType";
import { Expr } from "@choreo/document/math/Expr";
import { ConstraintKey, DataMap } from "@choreo/document/constraint/ConstraintDefinitions";
import { constraintDataConstructors, ConstraintDataConstructors } from "@choreo/stores/ConstraintDataStore";
import { EventMarker } from "@choreo/document/EventMarker";
import { UnionCommand } from "@choreo/document/Command";
import { createCommandStore, ICommandStore } from "@choreo/stores/CommandStore";
import { Waypoint } from "@choreo/document/waypoint/Waypoint";
import { IVariables } from "@choreo/stores/VariablesStore";
import {RobotConfig} from "@choreo/document/RobotConfig";
import {IRobotConfigStore, createRobotConfigStore} from "@choreo/stores/RobotConfigStore"
export type SelectableItemTypes =
  | ((IWaypointStore | IConstraintStore | IEventMarkerStore) & {
      uuid: string;
    })
  | undefined;
export const SelectableItem = types.union(
  {
    dispatcher: (snapshot): any => {
      if (Object.hasOwn(snapshot, "target")) return EventMarkerStore;
      if (Object.hasOwn(snapshot, "from")) return ConstraintStore;
      return WaypointStore;
    }
  },
  WaypointStore,
  EventMarkerStore,
  ConstraintStore
);
export function itemType(
  item: SelectableItemTypes
): "marker" | "constraint" | "waypoint" | undefined {
  if (item === undefined) {
    return undefined;
  }
  if (Object.hasOwn(item, "name")) {
    return "marker";
  }
  if (Object.hasOwn(item, "from")) {
    return "constraint";
  }
  if (Object.hasOwn(item, "fixTranslation")) {
    return "waypoint";
  }
  return undefined;
}
export const ISampleType = types.enumeration<SampleType>([
  "Swerve",
  "Differential"
]);

export type EnvConstructors = {
  RobotConfigStore: (config: RobotConfig<Expr>) => IRobotConfigStore;
  WaypointStore: (config: Waypoint<Expr>) => IWaypointStore;
  CommandStore: (command: UnionCommand) => ICommandStore;
  EventMarkerStore: (marker: EventMarker) => IEventMarkerStore;
  ConstraintData: ConstraintDataConstructors;
  ConstraintStore: <K extends ConstraintKey>(
    type: K,
    data: Partial<DataMap[K]["props"]>,
    enabled: boolean,
    vars: IVariables,
    from: IWaypointScope,
    to?: IWaypointScope
  ) => IConstraintStore;
};
export function getConstructors(vars: () => IVariables): EnvConstructors {
  const dataConstructors = constraintDataConstructors(vars);
  const commandConstructor = (c: UnionCommand) => createCommandStore(c, ()=>vars().scope);
  return {
    RobotConfigStore: (config: RobotConfig<Expr>) =>
      createRobotConfigStore(config, vars()),
    WaypointStore: (waypoint: Waypoint<Expr>) => {
      const w = WaypointStore.create({
        ...waypoint,
        x: vars().createExpression(waypoint.x, "Length"),
        y: vars().createExpression(waypoint.y, "Length"),
        heading: vars().createExpression(waypoint.heading, "Angle"),
        uuid: crypto.randomUUID()
      });
      return w;
    },
    CommandStore: commandConstructor,
    EventMarkerStore: (marker: EventMarker): IEventMarkerStore =>
      createEventMarkerStore(marker, vars()),
    ConstraintData: dataConstructors,
    ConstraintStore: constraintStoreConstructor(dataConstructors)
  };
}

export type Env = {
  selectedSidebar: () => string | null | undefined;
  hoveredItem: () => string | null | undefined;
  select: (item: SelectableItemTypes) => void;
  selected: (item: SelectableItemTypes) => boolean;
  hover: (item: SelectableItemTypes) => void;
  hovered: (item: SelectableItemTypes) => boolean;
  withoutUndo: (callback: any) => void;
  startGroup: (callback: any) => void;
  stopGroup: () => void;
  history: () => Instance<typeof UndoManager>;
  vars: () => IVariables;
  renameVariable: (find: string, replace: string) => void;
  exporter: (uuid: string) => Promise<void>;
  create: EnvConstructors;
};

import {
  EventMarker,
  Expr,
  RobotConfig,
  SampleType,
  UnionCommand,
  Waypoint
} from "$src/document/2025/DocumentTypes";
import { createCommandStore, ICommandStore } from "$src/document/CommandStore";
import {
  constraintDataConstructors,
  ConstraintDataConstructors
} from "$src/document/ConstraintDataStore";
import { ConstraintKey, DataMap } from "$src/document/ConstraintDefinitions";
import {
  ConstraintStore,
  constraintStoreConstructor,
  IConstraintStore,
  IWaypointScope
} from "$src/document/ConstraintStore";

import {
  EventMarkerStore,
  IEventMarkerStore,
  createEventMarkerStore
} from "$src/document/EventMarkerStore";
import { IVariables } from "$src/document/ExpressionStore";
import {
  HolonomicWaypointStore,
  IHolonomicWaypointStore
} from "$src/document/HolonomicWaypointStore";
import {
  IRobotConfigStore,
  createRobotConfigStore
} from "$src/document/RobotConfigStore";
import { Instance, types } from "mobx-state-tree";
import { UndoManager } from "mst-middlewares";
export type SelectableItemTypes =
  | ((IHolonomicWaypointStore | IConstraintStore | IEventMarkerStore) & {
      uuid: string;
    })
  | undefined;
export const SelectableItem = types.union(
  {
    dispatcher: (snapshot): any => {
      if (Object.hasOwn(snapshot, "target")) return EventMarkerStore;
      if (Object.hasOwn(snapshot, "from")) return ConstraintStore;
      return HolonomicWaypointStore;
    }
  },
  HolonomicWaypointStore,
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
  WaypointStore: (config: Waypoint<Expr>) => IHolonomicWaypointStore;
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
  const commandConstructor = (c: UnionCommand) => createCommandStore(c, vars());
  return {
    RobotConfigStore: (config: RobotConfig<Expr>) =>
      createRobotConfigStore(config, vars()),
    WaypointStore: (waypoint: Waypoint<Expr>) => {
      const w = HolonomicWaypointStore.create({
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

import {Instance, types} from "mobx-state-tree"
import { EventMarkerDataStore } from "./EventMarkerDataStore";
import { CommandStore, createCommandStore } from "./CommandStore";
import { EventMarker } from "@choreo/document/EventMarker";
import { IVariables } from "./VariablesStore";

// When adding new fields, consult
// https://choreo.autos/contributing/schema-upgrade/
// to see all the places that change with every schema upgrade.

// When adding new fields, consult
// https://choreo.autos/contributing/schema-upgrade/
// to see all the places that change with every schema upgrade.
export const EventMarkerStore = types
  .model("GeneralMarker", {
    name: types.string,
    from: EventMarkerDataStore,
    uuid: types.identifier,
    event: CommandStore
  })
  .views((self) => ({
    get serialize(): EventMarker {
      return {
        name: self.name,
        from: self.from.serialize,
        event: self.event.serialize
      };
    },
    // get selected(): boolean {
    //   if (!isAlive(self)) {
    //     return false;
    //   }
    //   return self.uuid === getEnv<Env>(self).selectedSidebar();
    // }
  }))
  .actions((self) => ({
    setName(name: string) {
      self.name = name;
    },
    // deserialize(
    //   ser: EventMarker,
    //   commandConstructor: EnvConstructors["CommandStore"]
    // ) {
    //   self.name = ser.name;
    //   self.from.deserialize(ser.from);
    //   self.event.deserialize(ser.event, commandConstructor);
    // },
    // setSelected(selected: boolean) {
    //   if (selected && !self.selected) {
    //     getEnv<Env>(self).select(
    //       getParent<IEventMarkerStore[]>(self)?.find(
    //         (point) => self.uuid == point.uuid
    //       )
    //     );
    //   }
    // }
  }));
export type IEventMarkerStore = Instance<typeof EventMarkerStore>;
export function createEventMarkerStore(
  marker: EventMarker,
  vars: IVariables
): IEventMarkerStore {
  const m = EventMarkerStore.create({
    name: marker.name,
    from: {
      uuid: crypto.randomUUID(),

      target: undefined,
      targetTimestamp: marker.from.targetTimestamp ?? undefined,
      offset: vars.createExpression(marker.from.offset, "Time")
    },
    event: createCommandStore(marker.event, ()=>vars.scope),
    uuid: crypto.randomUUID()
  });
  return m;
}

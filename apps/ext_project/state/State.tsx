import { types } from "mobx-state-tree";
import {
  createRobotConfigStore,
  EXPR_DEFAULTS,
  RobotConfigStore
} from "@choreo/stores/RobotConfigStore";
import { VariablesStore } from "@choreo/stores/VariablesStore";
import { Instance } from "mobx-state-tree";

import { createContext } from "react";
import {Project} from "@choreo/document/Project";
import { PROJECT_SCHEMA_VERSION } from "@choreo/document/version/ProjectSchemaVersion";
import {ISampleType} from "@choreo/stores/SampleType"
import { SampleType } from "@choreo/document/sample/SampleType";

const StateStore = types
  .model("ChorViewerState", {
    config: RobotConfigStore,
    variables: VariablesStore,
    type: ISampleType
  })
  .views((self) => ({
    get serialize(): Project {
      return {
        config: self.config.serialize,
        name: "",
        type: self.type,
        version: PROJECT_SCHEMA_VERSION,
        variables: self.variables.serialize
      };
    }
  }))
  .actions((self) => ({
    deserialize(ser: Project) {
      self.variables.deserialize(ser.variables);
      self.config.deserialize(ser.config);
      self.type = ser.type;
    },
    setType(type: SampleType) {
      self.type = type;
    }
  }));

export type IStateStore = Instance<typeof StateStore>;
export const StateStoreContext = createContext<null | IStateStore>(null);
export const StateStoreProvider = StateStoreContext.Provider;
export const createStateStore = () => {
  const variables = VariablesStore.create({ expressions: {}, poses: {} });
  return StateStore.create({
    config: createRobotConfigStore(EXPR_DEFAULTS, variables),
    variables,
    type: "Swerve"
  });
};

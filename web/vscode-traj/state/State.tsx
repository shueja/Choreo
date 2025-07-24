import { types } from "mobx-state-tree";
import {
  createRobotConfigStore,
  EXPR_DEFAULTS,
  RobotConfigStore
} from "$src/document/RobotConfigStore";
import { Variables } from "$src/document/ExpressionStore";
import { Instance } from "mobx-state-tree";
import {
  Project,
  PROJECT_SCHEMA_VERSION,
  Trajectory
} from "$src/document/2025/DocumentTypes";
import { createContext } from "react";
import {
  createPathStore,
  HolonomicPathStore
} from "$src/document/path/HolonomicPathStore";

const StateStore = types
  .model("ChorViewerState", {
    config: RobotConfigStore,
    variables: Variables,
    chorIsDefault: true,
    path: HolonomicPathStore
  })
  .views((self) => ({
    // get serialize() : Trajectory {
    //     return {
    //         config: self.config.serialize,
    //         name: "",
    //         type: "Swerve",
    //         version: PROJECT_SCHEMA_VERSION,
    //         variables: self.variables.serialize
    //     }
    // }
  }))
  .actions((self) => ({
    deserializeProject(ser: Project) {
      self.variables.deserialize(ser.variables);
      self.config.deserialize(ser.config);
      self.chorIsDefault = false;
    }
  }));

export type IStateStore = Instance<typeof StateStore>;
export const StateStoreContext = createContext<null | IStateStore>(null);
export const StateStoreProvider = StateStoreContext.Provider;
export const createStateStore = () => {
  const variables = Variables.create({ expressions: {}, poses: {} });
  return StateStore.create({
    config: createRobotConfigStore(EXPR_DEFAULTS, variables),
    variables,
    path: createPathStore(variables)
  });
};

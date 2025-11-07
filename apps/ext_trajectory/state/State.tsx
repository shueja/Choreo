import { types } from "mobx-state-tree";
import {
  createRobotConfigStore,
  EXPR_DEFAULTS,
  RobotConfigStore
} from "@choreo/stores/RobotConfigStore";
import { VariablesStore } from "@choreo/stores/VariablesStore";
import { Instance } from "mobx-state-tree";
import {
  Project
} from "@choreo/document/Project";
import { createContext } from "react";
import {
  createPathStore,
  PathStore
} from "@choreo/stores/path/PathStore";

const StateStore = types
  .model("ChorViewerState", {
    config: RobotConfigStore,
    variables: VariablesStore,
    chorIsDefault: true,
    path: PathStore
  })
  .views((self) => ({
    get serialize() {
      return  {
        config: self.config.serialize,
        variables: self.variables.serialize,
        path: self.path.serialize,
      }
    }
  }))
  .actions((self) => ({
    deserializeProject(ser: Project) {
      self.variables.deserialize(ser.variables);
      console.log(ser.config.bumper)
      self.config.deserialize(ser.config);
      self.chorIsDefault = false;
    },
    
  }))
  .actions((self)=>({
    reloadFromState(state) {
      self.config.deserialize(state.config);
      self.variables.deserialize(state.variables);
      self.path.deserialize(state.path);
    },
    afterCreate(){

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
    path: createPathStore(variables)
  });
};

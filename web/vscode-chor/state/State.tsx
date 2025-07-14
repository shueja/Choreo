import { types } from "mobx-state-tree"
import {createRobotConfigStore, EXPR_DEFAULTS, RobotConfigStore} from "$src/document/RobotConfigStore"
import { Variables } from "$src/document/ExpressionStore"
import { Instance } from "mobx-state-tree";
import { Project, PROJECT_SCHEMA_VERSION } from "$src/document/2025/DocumentTypes";
import { createContext } from "react";

const StateStore = types.model("ChorViewerState", {
    config: RobotConfigStore,
    variables: Variables
}).views(self=>({
    get serialize() : Project {
        return {
            config: self.config.serialize,
            name: "",
            type: "Swerve",
            version: PROJECT_SCHEMA_VERSION,
            variables: self.variables.serialize
        }
    }
}))
.actions(self=>({
    deserialize(ser: Project) {
        self.variables.deserialize(ser.variables);
        self.config.deserialize(ser.config);
    }
}));

export type IStateStore = Instance<typeof StateStore>;
export const StateStoreContext = createContext<null | IStateStore>(null);
export const StateStoreProvider = StateStoreContext.Provider;
export const createStateStore = () => {
    const variables = Variables.create({ expressions: {}, poses: {} });
    return StateStore.create({
        config: createRobotConfigStore(EXPR_DEFAULTS, variables),
        variables
    });
}
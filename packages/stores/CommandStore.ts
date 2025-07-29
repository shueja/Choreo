import {
  IAnyType,
  Instance,
  destroy,
  detach,
  getEnv,
  types
} from "mobx-state-tree";
import { moveItem } from "mobx-utils";
import { Command, CommandGroupType, commandIsGroup, commandIsNamed, commandIsWait, CommandType, commandTypeIsGroup, UnionCommand } from "@choreo/document/Command";
import {createExpressionStore, ExpressionStore} from "./ExpressionStore";
import { Expr } from "@choreo/document/math/Expr";
import { VariablesScopeGetter } from "@choreo/math/VariablesScope";


// When adding new fields, consult
// https://choreo.autos/contributing/schema-upgrade/
// to see all the places that change with every schema upgrade.
export const CommandStore = types
  .model("CommandStore", {
    type: types.union(
      types.literal("parallel"),
      types.literal("sequential"),
      types.literal("deadline"),
      types.literal("race"),
      types.literal("wait"),
      types.literal("named"),
      types.literal("none")
    ),
    commands: types.array(types.late((): IAnyType => CommandStore)),
    time: ExpressionStore,
    name: types.maybeNull(types.string),
    uuid: types.identifier
  })
  .views((self) => ({
    get isGroup(): boolean {
      return commandTypeIsGroup(self.type);
    },
    get isNone(): boolean {
      return self.type === "none";
    },
    get isNamed(): boolean {
      return self.type === "named";
    },
    get isWait(): boolean {
      return self.type === "wait";
    }
  }))
  .views((self) => ({
    get serialize(): Command {
      if (self.isNamed) {
        return {
          type: "named",
          data: {
            name: self.name
          }
        };
      } else if (self.isWait) {
        return {
          type: "wait",
          data: {
            waitTime: self.time.serialize
          }
        };
      } else if (self.isNone) {
        return undefined;
      } else {
        return {
          type: self.type as CommandGroupType,
          data: {
            commands: self.commands.map((c) => c.serialize)
          }
        };
      }
    }
  }))
  .actions((self) => ({
    deserialize(ser: Command, getScope: VariablesScopeGetter) {
      self.commands.clear();
      self.name = "";
      if (ser === undefined || ser === null) {
        self.type = "none";
        return;
      }
      self.type = ser.type;
      if (commandIsNamed(ser)) {
        self.name = ser.data.name;
      } else if (commandIsWait(ser)) {
        self.time.deserialize(ser.data.waitTime);
      } else {
        ser.data.commands.forEach((c) => {
          const command: ICommandStore = createCommandStore(c,getScope);
          command.deserialize(c, getScope);
          self.commands.push(command);
        });
      }
    },
    reorderCommands(startIndex: number, endIndex: number) {
      moveItem(self.commands, startIndex, endIndex);
    },
    setType(type: CommandType) {
      self.type = type;
    },
    setName(name: string) {
      self.name = name;
    },
    addSubCommand(getScope: VariablesScopeGetter) {
      // TODO add subcommand
      const newCommand = createCommandStore({
        type: "named",
        data: {
          waitTime: { exp: "0 s", val: 0 } as Expr,
          name: "",
          commands: []
        }
      }, getScope);
      self.commands.push(newCommand);
      return undefined;
    },
    pushCommand(subcommand: IAnyType) {
      self.commands.push(subcommand);
    },
    detachCommand(index: number) {
      return detach(self.commands[index]);
    },
    deleteSubCommand(uuid: string) {
      const toDelete = self.commands.find((c) => c.uuid === uuid);
      if (toDelete !== undefined) {
        destroy(toDelete);
      }
    }
  }));

export type ICommandStore = Instance<typeof CommandStore>;
export function createCommandStore(
  command: UnionCommand,
  getScope: VariablesScopeGetter
): ICommandStore {
  return CommandStore.create({
    type: command?.type ?? "none",
    name: commandIsNamed(command) ? command.data.name : "",
    commands: commandIsGroup(command)
      ? command.data.commands.map((c) => createCommandStore(c, getScope))
      : [],
    time: createExpressionStore(
      commandIsWait(command) ? command.data.waitTime : 0,
      "Time",
      getScope
    ),
    uuid: crypto.randomUUID()
  });
}

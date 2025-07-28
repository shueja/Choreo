import { Expr } from "./math/Expr";

export type GroupCommand = {
  type: CommandGroupType;
  data: {
    commands: PplibCommand[];
  };
};
export type WaitCommand = {
  type: "wait";
  data: {
    waitTime: Expr;
  };
};
export type NamedCommand = {
  type: "named";
  data: {
    name: string | null;
  };
};

export type PplibCommand = WaitCommand | GroupCommand | NamedCommand;
export type Command = PplibCommand | undefined | null;
export type UnionCommand =
  | (PplibCommand &
      (
        | {
            data: WaitCommand["data"] &
              GroupCommand["data"] &
              NamedCommand["data"];
          }
        | object
      ))
  | undefined
  | null;

export function commandIsNamed(command: Command): command is NamedCommand {
  return command?.type === "named";
}
export function commandTypeIsGroup(type: CommandType | undefined | null) {
  return (
    type === "deadline" ||
    type === "race" ||
    type === "parallel" ||
    type === "sequential"
  );
}
export function commandIsGroup(command: Command): command is GroupCommand {
  return commandTypeIsGroup(command?.type);
}
export function commandIsWait(command: Command): command is WaitCommand {
  return command?.type === "wait";
}

export type CommandGroupType = "sequential" | "parallel" | "deadline" | "race";
export type CommandType = CommandGroupType | "wait" | "named" | "none";
export const CommandTypeNames = {
  sequential: { id: "sequential", name: "Sequence" },
  parallel: { id: "parallel", name: "Parallel" },
  deadline: { id: "deadline", name: "Deadline" },
  race: { id: "race", name: "Race" },
  wait: { id: "wait", name: "Wait" },
  named: { id: "named", name: "Named" },
  none: { id: "none", name: "None" }
} as const satisfies {
  [key in CommandType]: {id: key, name: string}
};
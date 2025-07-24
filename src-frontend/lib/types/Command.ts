import { Expr } from "./math/Expr";

export type GroupCommand = {
  type: "deadline" | "parallel" | "race" | "sequential";
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
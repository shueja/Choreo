import { Instance, types, detach } from "mobx-state-tree";
import { createExpressionStore, Evaluated, ExpressionStore, IExpressionStore } from "./ExpressionStore";
import { createPose as createPoseStore, PoseVariableStore, IExprPose } from "./PoseVariableStore";
import {PoseVariable, Variables} from "@choreo/document/math/Variable"
import { Expr } from "@choreo/document/math/Expr";
import { DimensionName } from "@choreo/document/math/Dimensions";
import {math} from "@choreo/math/math";

export const VariablesStore = types
  .model("Variables", {
    expressions: types.map(ExpressionStore),
    poses: types.map(PoseVariableStore)
  })
  .views((self) => ({
    get serialize(): Variables {
      const out: Variables = {
        expressions: {},
        poses: {}
      };
      for (const entry of self.expressions.entries()) {
        out.expressions[entry[0]] = {
          dimension: entry[1].dimension,
          var: (entry[1] as IExpressionStore).serialize
        };
      }

      for (const entry of self.poses.entries()) {
        out.poses[entry[0]] = entry[1].serialize;
      }
      return out;
    },
    get scope() {
      const vars: Map<string, any> = new Map();
      //vars.set("m", math.unit("m"));
      for (const [key, val] of self.expressions.entries()) {
        vars.set(key, val.asScope);
      }
      for (const [key, val] of self.poses.entries()) {
        vars.set(key, val.asScope);
      }
      return vars;
    },
    get sortedExpressions(): Array<[string, IExpressionStore]> {
      return Array.from(self.expressions.entries()).sort((a, b) =>
        a[0].toLocaleUpperCase() > b[0].toLocaleUpperCase() ? 1 : -1
      );
    },
    get sortedExpressionKeys(): Array<string> {
      return this.sortedExpressions.map(([key, _]) => key);
    },
    get sortedPoses(): Array<[string, IExprPose]> {
      return Array.from(self.poses.entries()).sort((a, b) =>
        a[0].toLocaleUpperCase() > b[0].toLocaleUpperCase() ? 1 : -1
      );
    },
    get sortedPoseKeys(): Array<string> {
      return this.sortedPoses.map(([key, _]) => key);
    }
  }))
  .views((self) => ({
    // criteria according to https://mathjs.org/docs/expressions/syntax.html#constants-and-variables
    validateName(name: string, selfName: string): boolean {
      const notAlreadyExists =
        name === selfName ||
        (!self.poses.has(name) && !self.expressions.has(name));
      return (
        notAlreadyExists &&
        name.length != 0 &&
        math.parse.isAlpha(name[0], "", name[1]) &&
        name
          .split("")
          .every(
            (_c, i, arr) =>
              math.parse.isAlpha(arr[i], arr[i - 1], arr[i + 1]) ||
              math.parse.isDigit(arr[i])
          ) &&
        !["mod", "to", "in", "and", "xor", "or", "not", "end"].includes(name)
      );
    },
    createExpression(expr: string | number | Expr, defaultUnit: DimensionName) {
        return createExpressionStore(expr, defaultUnit, ()=>self.scope);
    },
    createPose(pose: PoseVariable<Expr> | PoseVariable<number>) {
        return createPoseStore(pose, ()=>self.scope);
    }
  }))
  .actions((self) => ({
    deletePose(key: string) {
      self.poses.delete(key);
    },
    deleteExpression(key: string) {
      self.expressions.delete(key);
    },
    renameExpression(cur: string, next: string) {
      const current = self.expressions.get(cur);
      if (current === undefined) return;
      //getEnv<Env>(self).renameVariable(cur, next);
      self.expressions.set(next, detach(current));
    },
    renamePose(cur: string, next: string) {
      const current = self.poses.get(cur);
      if (current === undefined) return;
      //getEnv<Env>(self).renameVariable(cur, next);
      self.poses.set(next, detach(current));
    },
    addPose(key: string, pose: PoseVariable<Expr> | PoseVariable<number>) {
      self.poses.set(key, self.createPose(pose));
    },
    add(key: string, expr: string | number | Expr, defaultUnit: DimensionName) {
      self.expressions.set(key, self.createExpression(expr, defaultUnit));
    },
    deserialize(vars: Variables) {
      self.expressions.clear();
      self.poses.clear();
      for (const entry of Object.entries(vars.expressions)) {
        this.add(entry[0], entry[1].var, entry[1].dimension);
      }

      for (const entry of Object.entries(vars.poses)) {
        this.addPose(entry[0], entry[1]);
      }
    }
  }));
export type IVariables = Instance<typeof VariablesStore>;
import { Instance, types } from "mobx-state-tree";
import { createExpressionStore, ExpressionStore } from "./ExpressionStore";
import { PoseVariable } from "@choreo/document/math/Variable";
import { Expr, ExprOrNumber } from "@choreo/document/math/Expr";
import { VariablesScopeGetter } from "./VariablesStore";

export const PoseVariableStore = types
  .model({
    x: ExpressionStore,
    y: ExpressionStore,
    heading: ExpressionStore
  })
  .views((self) => ({
    get asScope() {
      const node = {
        x: self.x.asScope,
        y: self.y.asScope,
        heading: self.heading.asScope,
        isPose2d: true
      };
      return node;
    },
    get serialize(): PoseVariable<Expr> {
      return {
        x: self.x.serialize,
        y: self.y.serialize,
        heading: self.heading.serialize
      };
    }
  }))
  .actions((self) => ({
    deserialize(pose: PoseVariable<Expr>) {
      self.x.deserialize(pose.x);
      self.y.deserialize(pose.y);
      self.heading.deserialize(pose.heading);
    }
  }));
export type IExprPose = Instance<typeof PoseVariableStore>;
export function createPose(pose: PoseVariable<Expr> | PoseVariable<number>, getScope: VariablesScopeGetter) {
    return PoseVariableStore.create({
    x: createExpressionStore(pose.x, "Length", getScope),
    y: createExpressionStore(pose.y, "Length", getScope),
    heading: createExpressionStore(pose.heading, "Angle", getScope)
    });
}
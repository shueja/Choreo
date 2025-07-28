import { observer } from "mobx-react";
import { Component } from "react";
import ExpressionInput from "../../input/ExpressionInput";
import ExpressionInputList from "../../input/ExpressionInputList";
import { IExpressionStore } from "@choreo/stores/ExpressionStore";

type Props = { rowGap: number, radius: IExpressionStore, cof: IExpressionStore, gearing: IExpressionStore, vmax: IExpressionStore, tmax: IExpressionStore };

type State = object;

function ModuleConfigPanel(props: Props) {
    return (
      <ExpressionInputList rowGap={props.rowGap}>
        <ExpressionInput
          title="Wheel Radius"
          enabled={true}
          roundingPrecision={3}
          number={props.radius}
          maxWidthCharacters={8}
          titleTooltip="Radius of swerve wheels"
        />
        <ExpressionInput
          title="Wheel COF"
          enabled={true}
          roundingPrecision={3}
          number={props.cof}
          maxWidthCharacters={8}
          titleTooltip="Coefficient of friction between wheel and ground"
        />
        <ExpressionInput
          title="Motor Rev/Wheel Rev"
          enabled={true}
          roundingPrecision={3}
          number={props.gearing}
          maxWidthCharacters={8}
          titleTooltip="Gearing between motor shaft and wheel axle (>1)"
        />
        <ExpressionInput
          title="Motor Max Speed"
          enabled={true}
          roundingPrecision={0}
          number={props.vmax}
          maxWidthCharacters={8}
          titleTooltip="Actual motor speed at 12V"
        />

        <ExpressionInput
          title="Motor Max Torque"
          enabled={true}
          roundingPrecision={3}
          number={props.tmax}
          maxWidthCharacters={8}
          titleTooltip="Motor torque as current-limited"
        />
      </ExpressionInputList>
    );
  }
export default observer(ModuleConfigPanel);

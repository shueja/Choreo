import { observer } from "mobx-react";
import ExpressionInput from "../../input/ExpressionInput";
import ExpressionInputList from "../../input/ExpressionInputList";
import { IExpressionStore } from "@choreo/stores/ExpressionStore";
import { IBumperStore } from "@choreo/stores/RobotConfigStore";

type Props = { rowGap: number, mass: IExpressionStore, inertia:IExpressionStore, bumper:IBumperStore };

function DimensionConfigPanel(props: Props) {
    return (
      <ExpressionInputList rowGap={props.rowGap}>
        <ExpressionInput
          title="Mass"
          enabled={true}
          number={props.mass}
          maxWidthCharacters={8}
          titleTooltip={"Total robot mass"}
        />

        <ExpressionInput
          title="MOI"
          enabled={true}
          number={props.inertia}
          maxWidthCharacters={8}
          titleTooltip={"Robot moment of inertia around center vertical axis"}
        />

        <ExpressionInput
          title="Bumper Front"
          enabled={true}
          roundingPrecision={3}
          number={props.bumper.front}
          maxWidthCharacters={8}
          titleTooltip="Distance from robot center to front bumper edge"
        />

        <ExpressionInput
          title="Bumper Back"
          enabled={true}
          roundingPrecision={3}
          number={props.bumper.back}
          maxWidthCharacters={8}
          titleTooltip="Distance from robot center to back bumper edge"
        />

        <ExpressionInput
          title="Bumper Side"
          enabled={true}
          roundingPrecision={3}
          number={props.bumper.side}
          maxWidthCharacters={8}
          titleTooltip="Distance from robot center to bumper side edge"
        />
      </ExpressionInputList>
    );
  }
export default observer(DimensionConfigPanel);

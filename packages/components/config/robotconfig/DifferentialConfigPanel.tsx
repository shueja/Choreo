import { observer } from "mobx-react";
import ExpressionInput from "../../input/ExpressionInput";
import ExpressionInputList from "../../input/ExpressionInputList";
import { IExpressionStore } from "@choreo/stores/ExpressionStore";

type Props = { rowGap: number, differentialTrackWidth: IExpressionStore };



  function DifferentialConfigPanel(props: Props) {
    return (
      <ExpressionInputList rowGap={props.rowGap}>
        <ExpressionInput
          title="Trackwidth"
          enabled={true}
          roundingPrecision={3}
          number={props.differentialTrackWidth}
          maxWidthCharacters={8}
          titleTooltip="Distance between wheel sides"
        />
      </ExpressionInputList>
    );
  }
export default observer(DifferentialConfigPanel);

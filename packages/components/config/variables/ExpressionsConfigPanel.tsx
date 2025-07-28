import { Divider } from "@mui/material";
import { observer } from "mobx-react";
import PoseVariablesConfigPanel from "./PoseVariablesConfigPanel";
import VariablesConfigPanel, {
  GeneralVariableAddPanel
} from "./VariablesConfigPanel";
import { IVariables } from "@choreo/stores/VariablesStore";

type Props = {
  variables: IVariables
};

const rowGap = 4;

function ExpressionsConfigPanel(props: Props) {
    props.variables.expressions.keys();
    return (
      <div
        style={{
          rowGap: `${1 * rowGap}px`,
          columnGap: rowGap,
          fontSize: "1rem",
          margin: `${1 * rowGap}px`,
          display: "grid",
          gridTemplateColumns:
            "max-content max-content max-content max-content",
          width: "max-content"
        }}
      >
        <VariablesConfigPanel variables={props.variables}></VariablesConfigPanel>
        {props.variables.poses.size > 0 && (
          <Divider sx={{ color: "gray", gridColumn: "1 / -1" }}>
            POSE VARIABLES
          </Divider>
        )}
        <PoseVariablesConfigPanel variables={props.variables}></PoseVariablesConfigPanel>
        <Divider sx={{ color: "gray", gridColumn: "1 / -1" }}>
          ADD NEW VARIABLE
        </Divider>
        <GeneralVariableAddPanel variables={props.variables}></GeneralVariableAddPanel>
      </div>
    );
  }
export default observer(ExpressionsConfigPanel);

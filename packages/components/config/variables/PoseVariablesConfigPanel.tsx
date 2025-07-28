import { Add, ArrowDropDown, ArrowDropUp, Delete } from "@mui/icons-material";
import { Tooltip } from "@mui/material";
import { observer } from "mobx-react";
import React, { useState } from "react";

import ExpressionInput from "../../input/ExpressionInput";
import VariableRenamingInput from "./VariableRenamingInput";
import { Pose } from "../../../util/MathUtil";
import { PoseVariable } from "@choreo/document/math/Variable";
import { Expr } from "@choreo/document/math/Expr";
import { IVariables } from "@choreo/stores/VariablesStore";
import { IExprPose } from "@choreo/stores/PoseVariableStore";
import { DimensionDescriptions, DimensionDescriptionsExt } from "@choreo/document/math/Dimensions";
import Waypoint from "@choreo/icons/Waypoint";

type PoseVariablePanelProps = {
  entry: [string, IExprPose];
  setName: (name: string) => void;
  actionButton: () => React.JSX.Element;
  logo: () => React.JSX.Element;
  validateName: (name: string) => boolean;
};

const PoseVariablePanel = observer(
  (
    props: PoseVariablePanelProps & {
      open: boolean;
      setOpen: undefined | ((open: boolean) => void);
    }
  ) => {
    const entry = props.entry;

    return (
      <>
        {props.logo()}

        <VariableRenamingInput
          width="7ch"
          key={entry[0] + ".name"}
          name={entry[0]}
          setName={(name) => props.setName(name)}
          validateName={(name) => props.validateName(name)}
        ></VariableRenamingInput>
        {/* The part that stays visible when closed. Clicking it opens/closes the panel if setOpen is defined*/}
        <span
          style={{
            display: "flex",
            flexDirection: "row",
            justifyContent: "space-between"
          }}
          onClick={() => {
            if (props.setOpen !== undefined) {
              props.setOpen(!props.open);
            }
          }}
        >
          <span>
            {`(${entry[1].x.value.toFixed(2)} m, ${entry[1].y.value.toFixed(2)} m, ${entry[1].heading.value.toFixed(2)} rad)`}
          </span>
          <Tooltip
            disableInteractive
            title={!props.open ? "Edit Pose" : "Close Panel"}
          >
            <span>
              {props.open && props.setOpen !== undefined ? (
                <ArrowDropUp></ArrowDropUp>
              ) : (
                <ArrowDropDown></ArrowDropDown>
              )}
            </span>
          </Tooltip>
        </span>
        {props.actionButton()}
        {props.open && (
          <>
            <span></span>
            <ExpressionInput
              enabled
              maxWidthCharacters={6}
              key={entry[0] + ".x"}
              title={".x"}
              number={entry[1].x}
            ></ExpressionInput>

            <span></span>
            <span></span>
            <ExpressionInput
              enabled
              maxWidthCharacters={6}
              key={entry[0] + ".y"}
              title={".y"}
              number={entry[1].y}
            ></ExpressionInput>
            <span></span>
            <span></span>

            <ExpressionInput
              key={entry[0] + ".heading"}
              enabled
              maxWidthCharacters={6}
              title={".heading"}
              number={entry[1].heading}
            ></ExpressionInput>
            <span></span>
          </>
        )}
      </>
    );
  }
);
const OpenablePoseVariablePanel = observer(
  (props: Omit<PoseVariablePanelProps, "open" | "logo">) => {
    const [open, setOpen] = useState(false);
    return (
      <PoseVariablePanel
        open={open}
        setOpen={setOpen}
        {...props}
        logo={() => (
          <span>
            <Tooltip disableInteractive title={DimensionDescriptionsExt.Pose}>
              <Waypoint></Waypoint>
            </Tooltip>
          </span>
        )}
      ></PoseVariablePanel>
    );
  }
);
export type AddPoseVariablePanelProps = {
  logo: () => React.JSX.Element;
  name: string;
  setName: (name: string) => void;
  pose: IExprPose;
  validateName: (name: string, currentName: string)=>boolean;
  addPose: (name: string, pose: PoseVariable<Expr>)=>void;
};
export const AddPoseVariablePanel = observer(
  (props: AddPoseVariablePanelProps) => {
    return (
      <PoseVariablePanel
        validateName={(name) => props.validateName(name, "")}
        logo={props.logo}
        open={true}
        setOpen={undefined}
        entry={[props.name, props.pose]}
        setName={(name) => props.setName(name)}
        actionButton={() => (
          <Add
            sx={{ color: "var(--accent-purple)" }}
            onClick={(_) => {
              if (props.validateName(props.name, "")) {
                const pose = {
                  x: props.pose.x.serialize,
                  y: props.pose.y.serialize,
                  heading: props.pose.heading.serialize
                };
                props.addPose(props.name, pose);
                props.setName("");
              }
            }}
          ></Add>
        )}
      ></PoseVariablePanel>
    );
  }
);

type PoseVariableConfigPanelProps = {
  variables: IVariables;
}
const PoseVariablesConfigPanel = observer((props: PoseVariableConfigPanelProps) => {
  props.variables.poses.keys();
  return (
    <>
      {props.variables.sortedPoses.map((entry) => (
        <OpenablePoseVariablePanel
          entry={entry}
          setName={(name) => props.variables.renamePose(entry[0], name)}
          validateName={(name) => props.variables.validateName(name, entry[0])}
          actionButton={() => (
            <Delete onClick={() => props.variables.deletePose(entry[0])}></Delete>
          )}
        ></OpenablePoseVariablePanel>
      ))}
    </>
  );
});
export default PoseVariablesConfigPanel;

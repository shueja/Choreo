import { Add, Delete } from "@mui/icons-material";
import { MenuItem, Select, SelectChangeEvent, Tooltip } from "@mui/material";
import { observer } from "mobx-react";
import React, { useMemo, useState } from "react";
import ExpressionInput from "../../input/ExpressionInput";
import { AddPoseVariablePanel } from "./PoseVariablesConfigPanel";
import VariableRenamingInput from "./VariableRenamingInput";
import { IExpressionStore } from "@choreo/stores/ExpressionStore";
import { DimensionDescriptionsExt, DimensionName, DimensionNameExt, DimensionNamesExt } from "@choreo/document/math/Dimensions";
import Waypoint from "@choreo/icons/Waypoint";
import { Expr } from "@choreo/document/math/Expr";
import { PoseVariable } from "@choreo/document/math/Variable";
import { IVariables } from "@choreo/stores/VariablesStore";

const VariablePanel = observer(
  (props: {
    entry: [string, IExpressionStore];
    setName: (name: string) => void;
    actionButton: () => React.JSX.Element;
    logo: () => React.JSX.Element;
    validateName: (name: string) => boolean;
  }) => {
    const entry = props.entry;
    return (
      <>
        {props.logo()}
        <ExpressionInput
          key={`${entry[0]}-expr`}
          enabled
          title={() => (
            <VariableRenamingInput
              validateName={(name) => props.validateName(name)}
              width="7ch"
              name={entry[0]}
              setName={(name) => props.setName(name)}
            ></VariableRenamingInput>
          )}
          number={entry[1]}
        ></ExpressionInput>
        {props.actionButton()}
      </>
    );
  }
);

type AddVariablePanelProps = {
  logo: () => React.JSX.Element;
  name: string;
  setName: (name: string) => void;
  expr: IExpressionStore;
  validateName: (name: string, currentName:string)=>boolean,
  add: (name: string, exp: string, dimension: DimensionName)=>void;
};

const AddVariablePanel = observer((props: AddVariablePanelProps) => {
  return (
    <VariablePanel
      logo={props.logo}
      entry={[props.name, props.expr]}
      setName={(name) => props.setName(name)}
      validateName={(name) => props.validateName(name, "")}
      actionButton={() => (
        <Add
          sx={{ color: "var(--accent-purple)" }}
          onClick={(_) => {
            if (props.validateName(props.name, "")) {
              props.add(
                props.name,
                props.expr.serialize.exp,
                props.expr.dimension
              );
              props.setName("");
            }
          }}
        ></Add>
      )}
    ></VariablePanel>
  );
});

type GeneralVariableAddPanelProps = {
  variables: IVariables
}
export const GeneralVariableAddPanel = observer((props: GeneralVariableAddPanelProps) => {
  const [name, setName] = useState("");
  const [type, setType] = useState<DimensionNameExt>("Number");
  const localExpression = useMemo(
    () => props.variables.createExpression("0", "Number"),
    []
  );
  const localPose = useMemo(
    () =>
      props.variables.createPose({
        x: 0,
        y: 0,
        heading: 0
      }),
    []
  );
  const logo = () => (
    <Select
      size="small"
      variant="standard"
      value={type}
      renderValue={(value) => <Waypoint></Waypoint>}
      sx={{
        ".MuiSelect-select": {
          padding: "0px !important",
          height: "0px !important",
          paddingRight: "24px !important"
        }
      }}
      onChange={(e: SelectChangeEvent<DimensionNameExt>) => {
        setType(e.target.value as DimensionNameExt);
        if (e.target.value !== "Pose") {
          localExpression.setDimension(e.target.value as DimensionName);
        }
      }}
    >
      {DimensionNamesExt.map((entry) => (
        <MenuItem value={entry}>
          {/* {DimensionsExt[entry].icon()} */}
          <Waypoint></Waypoint>
          <span style={{ width: "4px" }}></span>
          {DimensionDescriptionsExt[entry]}
        </MenuItem>
      ))}
    </Select>
  );
  if (type === "Pose") {
    return (
      <AddPoseVariablePanel
        logo={logo}
        name={name}
        setName={setName}
        pose={localPose} validateName={(newName)=>props.variables.validateName(newName, name)} addPose={(name, pose)=>props.variables.addPose(name, pose)}      ></AddPoseVariablePanel>
    );
  } else {
    return (
      <AddVariablePanel
        logo={logo}
        name={name}
        setName={setName}
        expr={localExpression} validateName={(newName)=>props.variables.validateName(newName, name)} add={(name, pose, dimension)=>props.variables.add(name, pose, dimension)}      ></AddVariablePanel>
    );
  }
});
type VariablesConfigPanelProps = {
  variables: IVariables;
}
const VariablesConfigPanel = observer((props: VariablesConfigPanelProps) => {
  props.variables.expressions.keys();
  return (
    <>
      {props.variables.sortedExpressions.map((entry) => (
        <VariablePanel
          validateName={(name) => props.variables.validateName(name, entry[0])}
          logo={() => (
            <Tooltip
              disableInteractive
              title={DimensionDescriptionsExt[entry[1].dimension as DimensionNameExt]}
            >
              {/* {DimensionsExt[entry[1].dimension].icon()} */}
              <Waypoint></Waypoint>
            </Tooltip>
          )}
          entry={entry}
          setName={(name) => props.variables.renameExpression(entry[0], name)}
          actionButton={() => (
            <Delete
              onClick={() => props.variables.deleteExpression(entry[0])}
            ></Delete>
          )}
        ></VariablePanel>
      ))}
    </>
  );
});

export default VariablesConfigPanel;

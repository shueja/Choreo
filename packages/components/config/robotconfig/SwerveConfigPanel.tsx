import { observer } from "mobx-react";
import { Component } from "react";
import { doc } from "../../../document/DocumentManager";
import ExpressionInput from "../../input/ExpressionInput";
import ExpressionInputList from "../../input/ExpressionInputList";
import { IModuleStore } from "@choreo/stores/RobotConfigStore";

type Props = { rowGap: number, frontLeft: IModuleStore, backLeft: IModuleStore };

type State = object;

function SwerveConfigPanel(props: Props){
    return (
      <ExpressionInputList rowGap={props.rowGap}>
        <ExpressionInput
          title="Front Mod X"
          enabled={true}
          roundingPrecision={3}
          number={props.frontLeft.x}
          maxWidthCharacters={8}
          titleTooltip="X coordinate of front modules"
        />

        <ExpressionInput
          title="Front Left Y"
          enabled={true}
          roundingPrecision={3}
          number={props.frontLeft.y}
          maxWidthCharacters={8}
          titleTooltip="Y coordinate of front left module"
        />
        <ExpressionInput
          title="Back Mod X"
          enabled={true}
          roundingPrecision={3}
          number={props.backLeft.x}
          maxWidthCharacters={8}
          titleTooltip="X coordinate of back modules (negative)"
        />

        <ExpressionInput
          title="Back Left Y"
          enabled={true}
          roundingPrecision={3}
          number={props.backLeft.y}
          maxWidthCharacters={8}
          titleTooltip="Y coordinate of back left module"
        />
      </ExpressionInputList>
    );
  }

export default observer(SwerveConfigPanel);

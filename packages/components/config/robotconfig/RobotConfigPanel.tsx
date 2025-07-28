import { Divider, FormHelperText, Switch } from "@mui/material";
import { observer } from "mobx-react";
import { useState } from "react";
import inputStyles from "../../input/InputList.module.css";
import DimensionsConfigPanel from "./DimensionsConfigPanel";
import ModuleConfigPanel from "./ModuleConfigPanel";
import TheoreticalPanel from "./TheoreticalPanel";

import DifferentialConfigPanel from "./DifferentialConfigPanel";
import SwerveConfigPanel from "./SwerveConfigPanel";
import { SampleType } from "@choreo/document/sample/SampleType";
import { IRobotConfigStore } from "@choreo/stores/RobotConfigStore";

type Props = {
  driveType: SampleType,
  setDriveType: (type: SampleType)=>void,
  config: IRobotConfigStore
};

const rowGap = 16;
function RobotConfigPanel(props: Props) {
    const [imperial, setImperial] = useState(false);
    const [bottomHalf, setBottomHalf] = useState(true);
    const imp = imperial;
    return (
      <div
        style={{
          display: "grid",
          gridTemplateColumns: "repeat(3, minmax(275px, 1fr))",
          gridGap: `${2 * rowGap}px`,
          rowGap: `${1 * rowGap}px`,
          fontSize: "2rem",
          margin: `${1 * rowGap}px`
        }}
      >
        {/* Left Column */}
        <div style={{ gridRow: 1, gridColumn: 1 }}>
          <Divider sx={{ color: "gray", marginBottom: `${rowGap}px` }}>
            DIMENSIONS
          </Divider>
          <DimensionsConfigPanel rowGap={rowGap}
          mass={props.config.mass}
          inertia={props.config.inertia}
          bumper={props.config.bumper}
          ></DimensionsConfigPanel>
        </div>
        {/* Middle Column */}
        <div style={{ gridRow: 1, gridColumn: 2 }}>
          <Divider sx={{ color: "gray", marginBottom: `${rowGap}px` }}>
            DRIVE MOTOR
          </Divider>
          <ModuleConfigPanel rowGap={rowGap}
          gearing={props.config.gearing}
          cof={props.config.cof}
          vmax={props.config.vmax}
          tmax={props.config.tmax}
          radius={props.config.radius}></ModuleConfigPanel>
        </div>
        {/* Right Column */}
        <div
          style={{
            gridColumn: 3,
            gridRow: 1
          }}
        >
          <Divider sx={{ color: "gray", marginBottom: `${rowGap}px` }}>
            DRIVE TYPE
          </Divider>
          <div
            style={{
              height: 24,
              display: "flex",
              flexDirection: "row",
              justifyContent: "space-evenly",
              marginBottom: `${rowGap}px`
            }}
          >
            <span className={inputStyles.Title} style={{ gridColumn: "1" }}>
              Swerve
            </span>

            <Switch
              size="small"
              sx={{
                gridColumn: 2,
                ".MuiSwitch-track": { backgroundColor: "black" },
                ".Mui-checked+.MuiSwitch-track": { backgroundColor: "black" }
              }}
              checked={props.driveType === "Differential"}
              onChange={(_e, checked) =>
                props.setDriveType(checked ? "Differential" : "Swerve")
              }
            ></Switch>
            <span className={inputStyles.Title} style={{ gridColumn: "1" }}>
              Differential
            </span>
          </div>
          {props.driveType === "Differential" ? (
            <DifferentialConfigPanel
              rowGap={rowGap}
              differentialTrackWidth={props.config.differentialTrackWidth}
            ></DifferentialConfigPanel>
          ) : (
            <SwerveConfigPanel rowGap={rowGap}
            frontLeft={props.config.frontLeft}
            backLeft={props.config.backLeft}></SwerveConfigPanel>
          )}
        </div>
        {/* Theoreticals */}
        <div
          style={{
            gridColumn: "1 / 4",
            gridRow: 2
          }}
        >
          <Divider sx={{ color: "gray" }}>THEORETICAL</Divider>
          <FormHelperText
            sx={{
              textAlign: "center",
              display: bottomHalf ? "block" : "none"
            }}
          >
            Calculated robot metrics, for reference and validation.
          </FormHelperText>
        </div>
        <TheoreticalPanel
          rowGap={rowGap}
          imperial={imp}
          config={props.config}
        ></TheoreticalPanel>
      </div>
    );
  }

export default observer(RobotConfigPanel);

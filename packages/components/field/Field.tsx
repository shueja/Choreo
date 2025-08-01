import { observer } from "mobx-react";
import { Component } from "react";
// import WaypointPanel from "../config/";

import { Close } from "@mui/icons-material";
import ShapeLineIcon from "@mui/icons-material/ShapeLine";
import { Box, CircularProgress, IconButton, Tooltip } from "@mui/material";
import { IConstraintStore } from "@choreo/stores/ConstraintStore";
import { IEventMarkerStore } from "@choreo/stores/EventMarkerStore";
import { IWaypointStore } from "@choreo/stores/WaypointStore";
// import ConstraintsConfigPanel from "../config/ConstraintsConfigPanel";
// import ViewOptionsPanel from "../config/ViewOptionsPanel";
// import WaypointVisibilityPanel from "../config/WaypointVisibilityPanel";
// import EventMarkerConfigPanel from "../config/eventmarker/EventMarkerConfigPanel";
import styles from "./Field.module.css";
import FieldOverlayRoot from "./svg/FieldOverlayRoot";
import { IStateStore } from "../../../apps/ext_trajectory/state/State";

type Props = {doc: IStateStore};

type State = object;

function Field({doc}: Props) {
    const selectedSidebar = undefined;
    const activePath = doc.path;
    let indexIfWaypoint = -1;
    if (selectedSidebar !== undefined && "heading" in selectedSidebar) {
      indexIfWaypoint = activePath.params.waypoints.findIndex(
        (point: IWaypointStore) =>
          point.uuid == (selectedSidebar as IWaypointStore)?.uuid
      );
    }

    return (
      <div className={styles.Container}>
        <FieldOverlayRoot stateStore={doc}></FieldOverlayRoot>
        {selectedSidebar !== undefined &&
          "heading" in selectedSidebar &&
          indexIfWaypoint !== -1 && (
            <></>
            // <WaypointPanel
            //   waypoint={selectedSidebar as IWaypointStore}
            //   index={indexIfWaypoint}
            // ></WaypointPanel>
          )}
        {selectedSidebar !== undefined &&
          "from" in selectedSidebar &&
          activePath.params.constraints.find(
            (constraint) =>
              constraint.uuid == (selectedSidebar as IConstraintStore)!.uuid
          ) && (
            <></>
            // <ConstraintsConfigPanel
            //   points={activePath.params.waypoints}
            //   constraint={selectedSidebar as IConstraintStore}
            // ></ConstraintsConfigPanel>
          )}
        {selectedSidebar !== undefined &&
          "event" in selectedSidebar &&
          activePath.markers.find(
            (marker) =>
              marker.uuid == (selectedSidebar as IEventMarkerStore)!.uuid
          ) && (
            <></>
            // <EventMarkerConfigPanel
            //   points={activePath.params.waypoints}
            //   marker={selectedSidebar as IEventMarkerStore}
            // ></EventMarkerConfigPanel>
          )}

        {/* <ViewOptionsPanel />
        <WaypointVisibilityPanel /> */}

        <Tooltip
          disableInteractive
          placement="top-start"
          title={
            false //activePath.ui.generating
              ? "Cancel Generation"
              : activePath.canGenerate()
                ? "Generate Path"
                : "Generate Path (needs 2 waypoints)"
          }
        >
          <Box
            sx={{
              position: "absolute",
              bottom: 16,
              right: 16,
              width: 48,
              height: 48
            }}
          >
            {/* cancel button */}
            <IconButton
              aria-label="add"
              size="large"
              style={{ pointerEvents: "all" }}
              sx={{
                position: "absolute",
                bottom: 0,
                right: 0,
                width: "100%",
                height: "100%",
                transformOrigin: "100% 100%",
                transform: "scale(1.3)",
                borderRadius: "50%",
                boxShadow: "3px",
                marginInline: 0,
                zIndex: /*activePath.ui.generating*/ false ? 10 : -1,
                backgroundColor: "red",
                "&:hover": {
                  backgroundColor: "darkred"
                }
              }}
              onClick={(_event) => {
                // ServerCommands.cancel(
                //   activePath.uuid
                //     .split("")
                //     .reduce((a, b) => ((a << 5) - a + b.charCodeAt(0)) | 0, 0)
                // );
              }}
              disabled={activePath.canGenerate()}
            >
              <Close> </Close>
            </IconButton>
            <IconButton
              color="primary"
              aria-label="add"
              size="large"
              style={{ pointerEvents: "all" }}
              sx={{
                position: "absolute",
                bottom: 0,
                right: 0,
                width: "100%",
                height: "100%",
                transformOrigin: "100% 100%",
                transform: "scale(1.3)",
                borderRadius: "50%",
                boxShadow: "3px",
                marginInline: 0,
                visibility: activePath.canGenerate() ? "visible" : "hidden"
              }}
              onClick={() => {}}//generateWithToastsAndExport(activePathUUID)}
              disabled={!activePath.canGenerate()}
            >
              <ShapeLineIcon></ShapeLineIcon>
            </IconButton>
          </Box>
        </Tooltip>
        {false && (
          <CircularProgress
            size={48 * 1.3}
            sx={{
              color: "var(--select-yellow)",
              position: "absolute",
              bottom: 16,
              right: 16
            }}
          />
        )}
      </div>
    );
  }


export default observer(Field);

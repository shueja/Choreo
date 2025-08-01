import { Room } from "@mui/icons-material";
import DeleteIcon from "@mui/icons-material/Delete";
import { IconButton, Tooltip } from "@mui/material";
import { observer } from "mobx-react";
import { getParent } from "mobx-state-tree";
import React, { Component } from "react";
import { waypointIDToText, WaypointUUID } from "@choreo/document/waypoint/WaypointUUID";
// import { doc } from "../../document/DocumentManager";
import { IEventMarkerStore } from "@choreo/stores/EventMarkerStore";
import {
  IPathStore,
  // waypointIDToText
} from "@choreo/stores/path/PathStore";
import styles from "./Sidebar.module.css";

type Props = {
  marker: IEventMarkerStore;
  path: IPathStore;
};

type State = { selected: boolean };

class SidebarMarker extends Component<Props, State> {
  id: number = 0;
  waypointIDToText(id: WaypointUUID | undefined) {
    const points = getParent<IPathStore>(
      getParent<IEventMarkerStore[]>(this.props.marker)
    ).params.waypoints;
    return waypointIDToText(id, points);
  }

  render() {
    const marker = this.props.marker;
    const selected = false;//this.props.marker.selected;
    return (
      <div
        className={styles.SidebarItem + (selected ? ` ${styles.Selected}` : "")}
        onClick={() => {
          //doc.setSelectedSidebarItem(marker);
        }}
      >
        {React.cloneElement(<Room></Room>, {
          className: styles.SidebarIcon,
          htmlColor: selected ? "var(--select-yellow)" : "var(--accent-purple)"
        })}
        <span
          className={styles.SidebarLabel}
          style={{ display: "grid", gridTemplateColumns: "1fr auto auto" }}
        >
          <Tooltip disableInteractive title={this.props.marker.name}>
            <span style={{ overflow: "hidden", textOverflow: "ellipsis" }}>
              {this.props.marker.name}
            </span>
          </Tooltip>
          {/* {!isInSameSegment || marker.data.getTargetIndex() === undefined ? (
            <Tooltip disableInteractive title={issueTitle}>
              <PriorityHigh
                className={styles.SidebarIcon}
                style={{ color: "red" }}
              ></PriorityHigh>
            </Tooltip>
          ) : (
            <span></span>
          )} */}
          <span>
            <span>{this.waypointIDToText(this.props.marker.from.target)} </span>
            <span style={{}}>
              (
              {(this.props.marker.from.offset.value < 0 ? "" : "+") +
                this.props.marker.from.offset.value.toFixed(2) +
                " s"}
              )
            </span>
          </span>
        </span>
        <Tooltip disableInteractive title="Delete Marker">
          <IconButton
            className={styles.SidebarRightIcon}
            onClick={(e) => {
              e.stopPropagation();
              this.props.path.deleteMarkerUUID(marker?.uuid || "");
            }}
          >
            <DeleteIcon />
          </IconButton>
        </Tooltip>
      </div>
    );
  }
}
export default observer(SidebarMarker);

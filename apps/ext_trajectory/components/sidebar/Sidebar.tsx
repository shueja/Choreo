import { Component } from "react";
import { observer } from "mobx-react";
import styles from "./Sidebar.module.css";
import { Divider, IconButton, Tooltip } from "@mui/material";
import WaypointList from "./WaypointList";
import MenuIcon from "@mui/icons-material/Menu";
import { ContentCopy, Redo, ShapeLine, Undo } from "@mui/icons-material";
import Add from "@mui/icons-material/Add";
import SidebarConstraint from "./SidebarConstraint";
import SidebarEventMarker from "./SidebarEventMarker";
import { IEventMarkerStore } from "@choreo/stores/EventMarkerStore";

import { IPathStore } from "@choreo/stores/path/PathStore";

type Props = {path: IPathStore};

type State = object;

function Sidebar( props: Props) {
    return (
      <div className={styles.Container}>        
        <Divider></Divider>
        <div className={styles.SidebarHeading}>FEATURES</div>
        <Divider flexItem></Divider>
        <div className={styles.Sidebar}>
          <Divider className={styles.SidebarDivider} textAlign="left" flexItem>
            <span>WAYPOINTS</span>
          </Divider>

          <WaypointList path={props.path}></WaypointList>
          <Divider className={styles.SidebarDivider} textAlign="left" flexItem>
            <span>CONSTRAINTS</span>
          </Divider>
          <div className={styles.WaypointList}>
            {props.path.params.constraints.map((constraint) => {
              return (
                <SidebarConstraint
                  path={props.path}
                  key={constraint.uuid}
                  constraint={constraint}
                ></SidebarConstraint>
              );
            })}
          </div>
          {props.path.params.constraints.length == 0 && (
            <div className={styles.SidebarItem + " " + styles.Noninteractible}>
              <span></span>
              <span style={{ color: "gray", fontStyle: "italic" }}>
                No Constraints
              </span>
            </div>
          )}
          <Divider className={styles.SidebarDivider} textAlign="left" flexItem>
            <span>MARKERS</span>
          </Divider>
          <div className={styles.WaypointList}>
            {props.path.markers.map(
              (marker: IEventMarkerStore, index: number) => {
                return (
                  <SidebarEventMarker
                    path={props.path}
                    marker={marker}
                    key={marker.uuid}
                  ></SidebarEventMarker>
                );
              }
            )}
          </div>
          {props.path.markers.length == 0 && (
            <div className={styles.SidebarItem + " " + styles.Noninteractible}>
              <span></span>
              <span style={{ color: "gray", fontStyle: "italic" }}>
                No Event Markers
              </span>
            </div>
          )}
          <Divider></Divider>
        </div>
      </div>
    );
  }

export default observer(Sidebar);

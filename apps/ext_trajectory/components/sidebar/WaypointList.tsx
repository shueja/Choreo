import { observer } from "mobx-react";
import { Component } from "react";
import { DragDropContext, Droppable } from "@hello-pangea/dnd";
import { IWaypointStore } from "@choreo/stores/WaypointStore";
import styles from "./Sidebar.module.css";
import SidebarWaypoint from "./SidebarWaypoint";
import { IPathStore } from "@choreo/stores/path/PathStore";

const getListStyle = (isDraggingOver: boolean) => ({
  outline: isDraggingOver ? "2px solid var(--darker-purple)" : "transparent"
});

type Props = {path: IPathStore};

type State = object;

class WaypointList extends Component<Props, State> {
  state = {};
  constructor(props: Props) {
    super(props);

    this.onDragEnd = this.onDragEnd.bind(this);
  }

  reorder(startIndex: number, endIndex: number) {
    this.props.path.params.reorderWaypoint(startIndex, endIndex);
  }

  onDragEnd(result: any) {
    // dropped outside the list
    if (!result.destination) {
      return;
    }
    this.reorder(result.source.index, result.destination.index);
  }

  newWaypoint(): void {
    this.props.path.params.addWaypoint();
  }
  // Normally you would want to split things out into separate components.
  // But in this example everything is just done in one place for simplicity
  render() {
    const waypoints = this.props.path.params.waypoints;
    const waypointsLength = waypoints.length;
    if (waypointsLength == 0) {
      return (
        <div className={styles.SidebarItem + " " + styles.Noninteractible}>
          <span></span>
          <span style={{ color: "gray", fontStyle: "italic" }}>
            No Waypoints
          </span>
        </div>
      );
    }
    const waypointElements = waypoints.map(
      (holonomicWaypoint: IWaypointStore, index: number) => {
        let issue = "";
        if (holonomicWaypoint.type == 2) {
          if (index == 0) {
            issue = "Cannot start with an empty waypoint.";
          } else if (index == waypoints.length - 1) {
            issue = "Cannot end with an empty waypoint.";
          }
        }
        return (
          <SidebarWaypoint
            path={this.props.path}
            delete={() => {
              this.props.path.params.deleteWaypoint(
                holonomicWaypoint.uuid
              );
            }}
            waypoint={holonomicWaypoint}
            index={index}
            issue={issue}
            pathLength={waypoints.length}
            key={holonomicWaypoint.uuid}
          ></SidebarWaypoint>
        );
      }
    );

    return (
      <DragDropContext onDragEnd={this.onDragEnd}>
        <Droppable droppableId="droppable">
          {(provided, snapshot) => (
            <div
              {...provided.droppableProps}
              ref={provided.innerRef}
              className={styles.WaypointList}
              style={getListStyle(snapshot.isDraggingOver)}
            >
              {waypointElements}
              {provided.placeholder}
            </div>
          )}
        </Droppable>
      </DragDropContext>
    );
  }
}
export default observer(WaypointList);

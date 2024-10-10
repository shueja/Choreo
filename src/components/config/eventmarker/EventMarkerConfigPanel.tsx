import { TextField, Tooltip } from "@mui/material";
import { observer } from "mobx-react";
import { resolveIdentifier } from "mobx-state-tree";
import React, { Component } from "react";
import { DragDropContext } from "@hello-pangea/dnd";
import { IEventMarkerStore } from "../../../document/EventMarkerStore";
import ExpressionInput from "../../input/ExpressionInput";
import ExpressionInputList from "../../input/ExpressionInputList";
import InputStyles from "../../input/InputList.module.css";
import ScopeSlider from "../ScopeSlider";
import styles from "../WaypointConfigPanel.module.css";
import CommandDraggable from "./CommandDraggable";
import { CommandStore } from "../../../document/CommandStore";
import { IHolonomicWaypointStore } from "../../../document/HolonomicWaypointStore";
import { waypointIDToText } from "../../../document/path/HolonomicPathStore";

type Props = { marker: IEventMarkerStore; points: IHolonomicWaypointStore[] };

type State = object;

class EventMarkerConfigPanel extends Component<Props, State> {
  nameInputRef: React.RefObject<HTMLInputElement> =
    React.createRef<HTMLInputElement>();
  state = {};

  onDragEnd(result: any) {
    // dropped outside the list
    if (!result.destination) {
      return;
    }
    const sourceCommandList = result.source?.droppableId;
    const targetCommandList = result.destination?.droppableId;
    if (targetCommandList === null) return;
    const targetCommand = resolveIdentifier(
      CommandStore,
      this.props.marker,
      targetCommandList
    );
    if (targetCommand === undefined) {
      return;
    }
    if (targetCommand === undefined) {
      return;
    }
    if (sourceCommandList === targetCommandList) {
      targetCommand.reorderCommands(
        result.source.index,
        result.destination.index
      );
    } else {
      // This section is dead code until we have a DnD library that allows dragging
      // between layers of nested drop areas
      const sourceCommand = resolveIdentifier(
        CommandStore,
        this.props.marker,
        sourceCommandList
      );
      if (sourceCommand === undefined) {
        return;
      }
      targetCommand.pushCommand(
        sourceCommand.detachCommand(result.source.index)
      );
      targetCommand.reorderCommands(
        targetCommand.commands.length - 1,
        result.destination.index
      );
    }
  }
  render() {
    const marker = this.props.marker;
    const points = this.props.points;
    const pointcount = points.length;

    const start = marker.from;
    let startIndex = (start.getTargetIndex() ?? -0.5) + 1;
    if (start.target === "first") {
      startIndex = 0;
    }
    if (start.target === "last") {
      startIndex = pointcount + 1;
    }

    return (
      <div
        className={styles.WaypointPanel}
        style={{
          width: `min(80%, max(400px, calc(${pointcount} * 3ch + 8ch)))`,
          display: "flex",
          gap: "4px"
        }}
      >
        <ScopeSlider
          isRange={false}
          startIndex={startIndex}
          endIndex={startIndex}
          setRange={(selection) => {
            const lastIdx = pointcount + 1;
            const scope = selection.map((idx) => {
              if (idx == 0) {
                return "first";
              } else if (idx == lastIdx) {
                return "last";
              } else {
                return { uuid: points[idx - 1]?.uuid ?? "" };
              }
            });
            start.setTarget(scope[0]);
          }}
          sliderProps={{
            color: start.getTargetIndex() === undefined ? "error" : "primary"
          }}
          points={points}
        ></ScopeSlider>
        <span style={{ width: "100%", fontSize: "0.8em", opacity: 0.8 }}>
          Changes to waypoint target will not take effect until regeneration.
        </span>
        <ExpressionInputList>
          <Tooltip
            disableInteractive
            title="Name of the marker. Used for triggers, but not for Named Commands"
          >
            <span className={InputStyles.Title + " " + InputStyles.Tooltip}>
              Name
            </span>
          </Tooltip>

          <TextField
            inputRef={this.nameInputRef}
            value={marker.name}
            onChange={(e) => marker.setName(e.target.value)}
            variant="standard"
            size="small"
            placeholder="Event Name"
            sx={{
              ".MuiInput-input": {
                paddingBottom: "0px"
              }
            }}
            onKeyDown={(e) => {
              if (e.key == "Enter") {
                this.nameInputRef.current?.blur();
              }
            }}
          ></TextField>
          <span></span>
          <span></span>
          <ExpressionInput
            key={"fromOffset"}
            title={waypointIDToText(start.target, points) + " +"}
            enabled={true}
            number={start.offset}
            titleTooltip={
              "The marker's time offset before or after the start waypoint"
            }
          />
          <span></span>
          <span></span>
          <ExpressionInput
            key={"toOffset"}
            title={"To Offset"}
            enabled={true}
            number={end.offset}
            titleTooltip={
              "The marker's time offset before or after the end waypoint"
            }
          />
        </ExpressionInputList>

        <DragDropContext onDragEnd={(result) => this.onDragEnd(result)}>
          <CommandDraggable
            command={marker.event}
            index={0}
            isDraggable={false}
            isRoot
          ></CommandDraggable>
        </DragDropContext>
      </div>
    );
  }
}
export default observer(EventMarkerConfigPanel);

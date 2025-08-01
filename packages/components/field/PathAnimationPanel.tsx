import { observer } from "mobx-react";
import { Component } from "react";
import PathAnimationSlider from "./PathAnimationSlider";
import IconButton from "@mui/material/IconButton";
import PlayIcon from "@mui/icons-material/PlayArrow";
import PauseIcon from "@mui/icons-material/Pause";
import { autorun } from "mobx";
import { Tooltip } from "@mui/material";
import hotkeys from "hotkeys-js";
import { IPathStore } from "@choreo/stores/path/PathStore";

type Props = {
  setTimestamp:(time: number)=>void,
  timestamp:number,
  path: IPathStore
};

type State = {
  running: boolean;
};

class PathAnimationPanel extends Component<Props, State> {
  state = {
    running: false
  };

  timerId = 0;
  totalTime = 0;
  i = 0;
  then = Date.now();
  step = (dt: number) => this.incrementTimer(dt);

  onStart() {
    this.then = Date.now();
    this.setState({ running: true });
    if (Math.abs(this.totalTime - this.props.timestamp) < 0.05) {
      this.props.setTimestamp(0);
    }
    window.cancelAnimationFrame(this.timerId);
    this.timerId = requestAnimationFrame(this.step);
  }

  incrementTimer(_: number) {
    const dt = Date.now() - this.then;
    this.then = Date.now();
    if (this.state.running) {
      const pathAnimationTimestamp = this.props.timestamp;
      if (pathAnimationTimestamp > this.totalTime) {
        this.props.setTimestamp(0);
      } else {
        this.props.setTimestamp(pathAnimationTimestamp + dt / 1e3);
      }

      this.timerId = requestAnimationFrame(this.step);
    }
  }

  onStop() {
    this.setState({ running: false });
    if (this.timerId !== 0) {
      window.cancelAnimationFrame(this.timerId);
    }
  }
  componentDidMount(): void {
    hotkeys("space", "all", (e) => {
      e.preventDefault();
      if (this.state.running) {
        this.onStop();
      } else {
        this.onStart();
      }
    });
    autorun(() => {
      const _ = this.props.path.uuid;
      this.onStop();
    });
  }
  render() {
    const activePath = this.props.path;
    this.totalTime = activePath.trajectory.getTotalTimeSeconds();
    return (
      <div
        style={{
          color: "white",
          gap: "1rem",
          width: "100%",
          height: "2rem",
          backgroundColor: "var(--background-dark-gray)",
          paddingLeft: "10px",
          paddingRight: "10px",
          boxSizing: "border-box",
          display: "block",
          borderTop: "thin solid var(--divider-gray)"
        }}
      >
        <span
          style={{
            display:
              activePath.trajectory.fullTrajectory.length >= 2
                ? "flex"
                : "none",
            flexDirection: "row",
            width: "100%",
            height: "100%",
            gap: "10px"
          }}
        >
          <Tooltip
            disableInteractive
            title={
              this.state.running
                ? "Pause Path Animation"
                : "Play Path Animation"
            }
          >
            <IconButton
              color="default"
              onClick={() => {
                if (this.state.running) {
                  this.onStop();
                } else {
                  this.onStart();
                }
              }}
            >
              {this.state.running ? (
                <PauseIcon></PauseIcon>
              ) : (
                <PlayIcon></PlayIcon>
              )}
            </IconButton>
          </Tooltip>
          <PathAnimationSlider path={this.props.path} timestamp={this.props.timestamp}></PathAnimationSlider>
        </span>
      </div>
    );
  }
}
export default observer(PathAnimationPanel);

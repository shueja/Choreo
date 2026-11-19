import { Component } from "react";
import { doc } from "../../../document/DocumentManager";

import { observer } from "mobx-react";
import FieldSampleLayer from "./FieldSampleLayer";

type Props = object;

type State = object;

class FieldSamples extends Component<Props, State> {
  state = {};
  LINE_LENGTH = 0.15;
  render() {
    const path = doc.pathlist.activePath;
    const trajectory = path.ui.generating
      ? path.ui.generationProgress
      : path.trajectory.fullTrajectory;
    // preserve the access of generationIterationNumber
    // to trigger rerenders when mutating the in-progress trajectory in place
    const _ = path.ui.generationIterationNumber;
    const points = trajectory.map(({ x, y }) => ({ x, y }));
    return <FieldSampleLayer points={points} />;
  }
}
export default observer(FieldSamples);

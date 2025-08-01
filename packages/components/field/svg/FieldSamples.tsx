import { Component } from "react";

import { observer } from "mobx-react";
import { IChoreoTrajectoryStore } from "@choreo/stores/path/ChoreoTrajectoryStore";
import { SampleArray } from "@choreo/document/sample/SampleType";

type Props = {trajectory: {x:number, y:number}[]};

type State = object;

  function FieldSamples(props: Props) {
    const trajectory = props.trajectory;
    // preserve the access of generationIterationNumber
    // to trigger rerenders when mutating the in-progress trajectory in place
    // const _ = path.ui.generationIterationNumber;
    return (
      <>
        {trajectory.map((point, idx) => (
          <circle
            cx={point.x}
            cy={point.y}
            r={0.02}
            fill="black"
            key={idx}
          ></circle>
        ))}
      </>
    );
  }
export default observer(FieldSamples);

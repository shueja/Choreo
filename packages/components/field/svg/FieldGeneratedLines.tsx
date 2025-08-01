import { observer } from "mobx-react";
import {
  PathGradientArgs,
  PathGradients
} from "../../config/robotconfig/PathGradient";
import { IPathStore } from "@choreo/stores/path/PathStore";

function FieldGeneratedLines(props: {path: IPathStore, gradient: keyof typeof PathGradients}) {
  const {path, gradient} = props;
  let generatedPathString = "";
  const trajectory = path.trajectory.fullTrajectory;
  // preserve the access of generationIterationNumber
  // to trigger rerenders when mutating the in-progress trajectory in place
  // const _ = path.ui.generationIterationNumber;
  trajectory.forEach((sample) => {
    generatedPathString += `${sample.x},${sample.y} `;
  });
  const pathGradient = PathGradients[gradient];
  if (
    pathGradient === undefined ||
    gradient == "None"
  ) {
    return (
      <polyline
        points={generatedPathString}
        stroke="var(--select-yellow)"
        strokeWidth={0.05}
        fill="transparent"
        style={{ pointerEvents: "none" }}
      ></polyline>
    );
  }
  return (
    <>
      <g>
        {trajectory.length > 1 &&
          trajectory.map((sample, i, samples) => {
            if (i == samples.length - 1) {
              return undefined;
            }
            const nextSample = samples[i + 1];

            // const args: PathGradientArgs<any> = {
            //   samples: path.ui.generating
            //     ? path.ui.generationProgress
            //     : path.trajectory.samples,
            //   index: i,
            //   section: path.ui.generating
            //     ? 0
            //     : (path.trajectory.getIdxOfFullTrajectory(i) ?? [0, 0])[0],
            //   documentModel: doc
            // };
            // 0 t = red, 1 t = green
            return (
              <line
                key={i}
                x1={sample.x}
                y1={sample.y}
                x2={nextSample.x}
                y2={nextSample.y}
                strokeWidth={0.05}
                stroke={"var(--select-yellow)"}//pathGradient.function(args)}
              ></line>
            );
          })}
      </g>
    </>
  );
}
export default observer(FieldGeneratedLines);

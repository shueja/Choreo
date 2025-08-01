import { Component } from "react";

import { observer } from "mobx-react";
import { Waypoint } from "@choreo/document/waypoint/Waypoint";
import { Expr } from "@choreo/document/math/Expr";

type Props = {points: Waypoint<number>[]};

type State = object;

function FieldSamples({points}: Props) {
    return (
      <>
        {points.map((point, idx) => {
          let color = "white";
          if (idx === 0) {
            color = "green";
          } else if (idx === points.length - 1) {
            color = "red";
          }
          if (point.fixHeading) {
            return (
              <g
                key={idx}
                transform={` translate(${point.x}, ${point.y}) rotate(${
                  (point.heading * 180) / Math.PI
                })`}
              >
                <circle cx={0} cy={0} r={0.04} fill={color}></circle>
                <circle cx={0.1} cy={0} r={0.04} fill={color}></circle>
                <rect
                  x={-0.1}
                  y={-0.1}
                  width={0.2}
                  height={0.2}
                  stroke={color}
                  strokeWidth={0.04}
                  fill="none"
                ></rect>
              </g> // Full
            );
          } else if (point.fixTranslation) {
            return (
              <circle cx={point.x} cy={point.y} r={0.08} fill={color}></circle>
            );
            // Translation
          } else {
            return <></>;
            // Empty
          }
        })}
      </>
    );
  }
export default observer(FieldSamples);

import { SampleArray } from "@choreo/document/sample/SampleType";
import { sample } from "@choreo/math/MathUtil";
import { IRobotConfigStore } from "@choreo/stores/RobotConfigStore";
import { observer } from "mobx-react";
import { Component } from "react";

type Props = {
  timestamp: number;
  robotConfig: IRobotConfigStore;
  trajectory: SampleArray
};

type State = object;

const targetRadius = 0.1;

// Find the side length that makes an equilateral triangle have the same area as
// a circle.
//
//   triangle area = circle area
//   1/2 bh = πr²
//
// An equilateral triangle with side length l has a height of √3/2 l.
//
//   1/2 (l)(√3/2 l) = πr²
//   √3/4 l² = πr²
//   l² = 4πr²/√3
//   l = √(4πr²/√3)
//   l = 2r√(π/√3)
//   l = 2r√(π√3/3)
const targetSideLength =
  2 * targetRadius * Math.sqrt((Math.PI * Math.sqrt(3)) / 3);

function InterpolatedRobot({robotConfig, trajectory, timestamp}: Props) {
    
    if (trajectory.length < 2) {
      return <></>;
    }
    const pose1 = sample(timestamp, trajectory);
    if (pose1 === undefined) return <></>;
    const headingPointSideLength =
      targetSideLength *
      Math.min(robotConfig.bumper.length, robotConfig.bumper.width);
    const headingPointHeight = (Math.sqrt(3) / 2) * headingPointSideLength;

    return (
      <g
        transform={`translate(${pose1.x}, ${pose1.y}) rotate(${
          (pose1.heading * 180) / Math.PI
        })`}
        style={{ pointerEvents: "none" }}
      >
        <defs>
          <path
            id={"robot-bumpers"}
            d={robotConfig.bumperSVGElement()}
          ></path>
          <clipPath id={"robot-clip"}>
            <use xlinkHref={"#robot-bumpers"} />
          </clipPath>
        </defs>

        <use
          xlinkHref={"#robot-bumpers"}
          clipPath={"url(#robot-clip)"}
          stroke={"white"}
          strokeWidth={0.0005}
          fill={"transparent"}
          vectorEffect={"non-scaling-stroke"}
          style={{ pointerEvents: "none" }}
        />
        {/* Heading point */}
        <polygon
          transform={`translate(${robotConfig.bumper.length / 2},0)`}
          fill="white"
          points={
            `${-headingPointHeight / 2},${headingPointSideLength / 2} ` +
            `${-headingPointHeight / 2},${-headingPointSideLength / 2} ` +
            `${headingPointHeight / 2},${0} `
          }
        ></polygon>
        {/* Wheel locations */}
        {robotConfig.moduleTranslations.map((mod, idx) => (
          <circle
            key={idx}
            cx={mod.x}
            cy={mod.y}
            r={robotConfig.radius.value}
            fill="white"
          ></circle>
        ))}
      </g>
    );
  }

export default observer(InterpolatedRobot);

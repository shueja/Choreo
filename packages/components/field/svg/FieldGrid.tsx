import { observer } from "mobx-react";
import { Component } from "react";

type Props = object;

type State = object;

const DRAW_BOUND = 100;
const GRID_STROKE = 0.01;

  function FieldGrid() {
    return (
      <>
        <defs>
          <pattern id="grid" width="1" height="1" patternUnits="userSpaceOnUse">
            <path
              d="M 1 0 L 0 0 0 1"
              fill="none"
              stroke="silver"
              strokeWidth={GRID_STROKE}
            />
          </pattern>
        </defs>
        <circle
          cx={0}
          cy={0}
          r={DRAW_BOUND}
          fill="url(#grid)"
          style={{ pointerEvents: "none" }}
        ></circle>
      </>
    );
  }

export default observer(FieldGrid);

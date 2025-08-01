import { Component } from "react";

import { observer } from "mobx-react";
import { IPathStore } from "@choreo/stores/path/PathStore";

type Props = object;

type State = object;

function FieldConstraintsAddLayer(props: {path:IPathStore}) {
    const activePath = props.path;
    const waypoints = activePath.params.waypoints;
    return (
      <>
        {/* Draw circles on each waypoint */}
        {waypoints.map((point, index) => {
          return (
            <circle
              key={index}
              cx={point.x.value}
              cy={point.y.value}
              r={0.2}
              fill={"black"}
              fillOpacity={0.2}
              stroke="white"
              strokeWidth={0.05}
              onClick={() => {
                // const newMarker = activePath.addEventMarker();

                // newMarker.from.setTarget({ uuid: point.uuid });
                // // TODO set direct timestamp if trajectory not stale
                // doc.setSelectedSidebarItem(newMarker);
              }}
            ></circle>
          );
        })}
      </>
    );
  }
export default observer(FieldConstraintsAddLayer);

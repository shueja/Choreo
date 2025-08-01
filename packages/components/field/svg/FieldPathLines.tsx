import { observer } from "mobx-react";
import { IWaypointStore } from "@choreo/stores/WaypointStore";

type Props = {
  waypoints: IWaypointStore[]
}
function FieldPathLines(props: Props) {
    let pathString = "";
    props.waypoints.forEach((point, _index) => {
      pathString += `${point.x.value}, ${point.y.value} `;
    });
    return (
      <>
        <polyline
          points={pathString}
          stroke="grey"
          strokeWidth={0.05}
          fill="transparent"
          style={{ pointerEvents: "none" }}
        ></polyline>
      </>
    );
  }
export default observer(FieldPathLines);

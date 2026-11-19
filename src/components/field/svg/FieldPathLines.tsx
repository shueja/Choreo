import { Component } from "react";
import { doc } from "../../../document/DocumentManager";

import { observer } from "mobx-react";
import FieldPathLine from "./FieldPathLine";

type Props = object;

type State = object;

class FieldPathLines extends Component<Props, State> {
  state = {};

  render() {
    const points = doc.pathlist.activePath.params.waypoints.map((point) => ({
      x: point.x.value,
      y: point.y.value
    }));
    return <FieldPathLine points={points} />;
  }
}
export default observer(FieldPathLines);

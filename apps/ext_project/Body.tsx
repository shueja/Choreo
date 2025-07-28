import { observer } from "mobx-react";
import RobotConfigPanel from "@choreo/components/config/robotconfig/RobotConfigPanel";
import { SampleType } from "@choreo/document/sample/SampleType";
import { IStateStore } from "./state/State";
import ExpressionsConfigPanel from "@choreo/components/config/variables/ExpressionsConfigPanel";
// import Navbar from "./components/navbar/Navbar";
// import Field from "./components/field/Field";
// import Sidebar from "./components/sidebar/Sidebar";
// import AppMenu from "./AppMenu";
// import PathAnimationPanel from "./components/field/PathAnimationPanel";

type Props = {
  doc: IStateStore
};

type State = object;

function Body(props:Props) {
  let doc = props.doc;

    return (
      <>
        <div className="App">
          <div className="Page">
            <RobotConfigPanel driveType={doc?.type} setDriveType={doc.setType} config={doc.config}></RobotConfigPanel>
            <ExpressionsConfigPanel variables={doc.variables}></ExpressionsConfigPanel>
            {JSON.stringify(doc?.serialize)}
          </div>
        </div>
      </>
    );
  }
export default observer(Body);

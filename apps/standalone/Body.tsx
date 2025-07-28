import { Component, useContext } from "react";
import { observer } from "mobx-react";
import PathAnimationPanel from "./components/field/PathAnimationPanel";
import { DocumentStoreContext } from "./document/DocumentModel";
import { PathStoreContext } from "./document/PathStoreContext";
import RobotConfigPanel from "@choreo/components/config/robotconfig/RobotConfigPanel";
import { SampleType } from "@choreo/document/sample/SampleType";
import { getContext } from "./util/ContextUtils";
import { IDocumentStore } from "./document/DocumentModel";
import ExpressionsConfigPanel from "@choreo/components/config/variables/ExpressionsConfigPanel";
// import Navbar from "./components/navbar/Navbar";
// import Field from "./components/field/Field";
// import Sidebar from "./components/sidebar/Sidebar";
// import AppMenu from "./AppMenu";
// import PathAnimationPanel from "./components/field/PathAnimationPanel";

type Props = {
  doc: IDocumentStore
};

type State = object;

function Body(props:Props) {
  let doc = props.doc;

    return (
      <>
        <div className="App">
          <div className="Page">
            <PathStoreContext.Provider value={doc?.pathlist.activePath}>
            {/* <AppMenu></AppMenu>
            <span
              style={{
                display: "flex",
                flexDirection: "row",
                flexGrow: 1,
                height: 0,
                width: "100%"
              }}
            >
              <Sidebar></Sidebar>
              <span
                style={{
                  display: "flex",
                  flexDirection: "column",
                  flexGrow: 1,
                  width: 0
                }}
              >
                <Navbar></Navbar>
                <Field></Field>
              </span>
            </span>*/}
            {/* <PathAnimationPanel ></PathAnimationPanel> */}
            <RobotConfigPanel driveType={doc?.type} setDriveType={doc.setType} config={doc.robotConfig}></RobotConfigPanel>
            <ExpressionsConfigPanel variables={doc.variables}></ExpressionsConfigPanel>
            {JSON.stringify(doc?.serializeChor())}
            </PathStoreContext.Provider>
          </div>
        </div>
      </>
    );
  }
export default observer(Body);

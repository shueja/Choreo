import React, { useEffect, useMemo } from "react";
import { observer } from "mobx-react";
import { createStateStore, StateStoreProvider } from "./state/State";
import { reaction, untracked } from "mobx";
import { vscode } from "./vscode";
import "./App.css"
import { ChoreoThemeProvider } from "@choreo/components/theme/ChoreoThemeProvider"
import "@vscode-elements/elements/dist/vscode-tabs";
import "@vscode-elements/elements/dist/vscode-tab-header";
import "@vscode-elements/elements/dist/vscode-tab-panel";
// if (import.meta.env.DEV) {
//   await import("@vscode-elements/webview-playground");
// }
import { configure } from "mobx"
import Sidebar from "./components/sidebar/Sidebar";
import Navbar from "./components/navbar/Navbar";
import PathAnimationPanel from "@choreo/components/field/PathAnimationPanel";
import Field from "@choreo/components/field/Field";

configure({
  disableErrorBoundaries: true
})
const State = createStateStore();
const previousSerialize = vscode.getState();
if (previousSerialize !== undefined) {
  console.log("previousSerialize", previousSerialize);
  State.reloadFromState(previousSerialize);
} else {
  vscode.postMessage({ type: "init-view" });
  console.log("init-view sent");
}

let reactingToBackendUpdate = false;
window.addEventListener("message", (event) => {
  const message = event.data; // The JSON data our extension sent
  console.log(JSON.stringify(event));
  switch (message.type) {
    case "updateChor":
    case "initChor":
      reactingToBackendUpdate = true;
      State.deserializeProject(JSON.parse(message.text));
      setTimeout(() => (reactingToBackendUpdate = false), 0);
      break;
    case "updateTraj":
    case "initTraj":
      console.error(message);
      reactingToBackendUpdate = true;
      State.path.deserialize(JSON.parse(message.text));
      setTimeout(() => (reactingToBackendUpdate = false), 0);
      break;
  }

});
reaction(
  () => {
    try {
      return State.path.serialize
    } catch (e) {
      console.error(e);
      throw e;
    }
  },
  (traj) => {
    console.log(traj);
    if (!reactingToBackendUpdate) {
      vscode.postMessage({
        type: "updateTraj",
        data: JSON.stringify(traj)
      });
    }
  }
);
reaction(
  () => {
    try {
      return State.serialize
    } catch (e) {
      console.error(e);
      throw e;
    }
  },
  (traj) => {
    console.log(traj);
  }
);
function App() {

  console.log("app rerender");

  return (
    <>
      <ChoreoThemeProvider>
        {/* {import.meta.env.DEV ? <vscode-dev-toolbar></vscode-dev-toolbar> : null} */}
        <StateStoreProvider value={State}>
          <div className="App">
            <div className="Page">
              <span
                style={{
                  display: "flex",
                  flexDirection: "column",
                  flexGrow: 1,
                  width: "100%"
                }}
              >
                <Navbar></Navbar>
                <span
                  style={{
                    display: "flex",
                    flexDirection: "row",
                    flexGrow: 1,
                    height: 0,
                    width: "100%"
                  }}
                >
                  <Sidebar path={State.path}></Sidebar>


                  <Field doc={State}></Field>
                </span>
                <PathAnimationPanel setTimestamp={function (time: number): void {
                  
                } } timestamp={0} path={State.path}></PathAnimationPanel>
              </span>
            </div></div>
        </StateStoreProvider>
      </ChoreoThemeProvider>
    </>
  );
}

export default observer(App);

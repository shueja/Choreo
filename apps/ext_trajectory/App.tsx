import React, { useEffect, useMemo } from "react";
import { observer } from "mobx-react";
import { createStateStore, StateStoreProvider } from "./state/State";
import { reaction, untracked } from "mobx";
import { vscode } from "./vscode";
import "./App.css"
import "@vscode-elements/elements/dist/vscode-tabs";
import "@vscode-elements/elements/dist/vscode-tab-header";
import "@vscode-elements/elements/dist/vscode-tab-panel";
// if (import.meta.env.DEV) {
//   await import("@vscode-elements/webview-playground");
// }
import { configure } from "mobx"

configure({
  disableErrorBoundaries: true
})
const State = createStateStore();
      const previousState = vscode.getState();
      if (previousState !== undefined) {
        console.log("previousState", previousState);
        State.reloadFromState(previousState);
      } else {
        vscode.postMessage({ type: "init-view" });
        console.log("init-view sent");
      }

let reactingToBackendUpdate = false;
window.addEventListener("message", (event) => {
  const message = event.data; // The JSON data our extension sent

  switch (message.type) {
    case "updateChor":
    case "initChor":
      reactingToBackendUpdate = true;
      State.deserializeProject(JSON.parse(message.text));
      setTimeout(() => (reactingToBackendUpdate = false), 0);
      break;
    case "updateTraj":
    case "initTraj":
      console.log(message);
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
    if (!reactingToBackendUpdate) {
      vscode.setState(traj)
    }
  }
);
function App() {

  console.log("app rerender");

  return (
    <>
      {/* {import.meta.env.DEV ? <vscode-dev-toolbar></vscode-dev-toolbar> : null} */}
      <StateStoreProvider value={State}>
        {JSON.stringify(State.path.serialize)}
        {/* <vscode-tabs
          selected-index="0"
          style={{ overflow: "hidden", height: "100%" }}
        >
          <vscode-tab-header>Traj</vscode-tab-header>
          <vscode-tab-panel>
            
          </vscode-tab-panel>
          <vscode-tab-header>Variables</vscode-tab-header>
          <vscode-tab-panel></vscode-tab-panel>
        </vscode-tabs> */}
      </StateStoreProvider>
    </>
  );
}

export default observer(App);

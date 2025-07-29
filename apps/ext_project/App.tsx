import React, { useEffect } from "react";
import { observer } from "mobx-react";
import { createStateStore, StateStoreProvider } from "./state/State";
import RobotConfigView from "./view/RobotConfigView";
import { reaction } from "mobx";
import { vscode } from "./main";

import "@vscode-elements/elements/dist/vscode-tabs";
import "@vscode-elements/elements/dist/vscode-tab-header";
import "@vscode-elements/elements/dist/vscode-tab-panel";
import "./App.css"
import Body from "./Body";
import {ChoreoThemeProvider} from "@choreo/components/theme/ChoreoThemeProvider"
// if (import.meta.env.DEV) {
//   await import("@vscode-elements/webview-playground");
// }
function App() {
  const State = createStateStore();
  let reactingToBackendUpdate = false;
  reaction(
    () => {
      try{
        return State.serialize} catch (e) {
          console.error(e);
          throw e;
        }},
    (project) => {
      console.log(project);
      if (!reactingToBackendUpdate) {
        vscode.postMessage({
          type: "update",
          data: JSON.stringify(project)
        });
      }
    }
  );
  window.addEventListener("message", (event) => {
    const message = event.data; // The JSON data our extension sent

    switch (message.type) {
      case "update":
      case "init":
        reactingToBackendUpdate = true;
        State.deserialize(JSON.parse(message.text));
        setTimeout(() => (reactingToBackendUpdate = false), 0);
        break;
    }
  });
  vscode.postMessage({ type: "init-view" });

  return (
    <>
      {import.meta.env.DEV ? <vscode-dev-toolbar></vscode-dev-toolbar> : null}
      <StateStoreProvider value={State}>
        <ChoreoThemeProvider>
        {/* <vscode-tabs
          selected-index="0"
          style={{ overflow: "hidden", height: "100%" }}
        >
          <vscode-tab-header>Robot Config</vscode-tab-header>
          <vscode-tab-panel style={{ overflow: "hidden" }}>
            <RobotConfigView></RobotConfigView>
          </vscode-tab-panel>
          <vscode-tab-header>Variables</vscode-tab-header>
          <vscode-tab-panel></vscode-tab-panel>
        </vscode-tabs> */}
        <Body doc={State}></Body>
        </ChoreoThemeProvider>
      </StateStoreProvider>
    </>
  );
}

export default observer(App);

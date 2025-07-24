import React, { useEffect } from "react";
import { observer } from "mobx-react";
import { createStateStore, StateStoreProvider } from "./state/State";
import { reaction } from "mobx";
import { vscode } from "./main";

import "@vscode-elements/elements/dist/vscode-tabs";
import "@vscode-elements/elements/dist/vscode-tab-header";
import "@vscode-elements/elements/dist/vscode-tab-panel";
if (import.meta.env.DEV) {
  await import("@vscode-elements/webview-playground");
}
function App() {
  const State = createStateStore();
  let reactingToBackendUpdate = false;
  reaction(
    () => State.path.serialize,
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
        reactingToBackendUpdate = true;
        State.path.deserialize(JSON.parse(message.text));
        setTimeout(() => (reactingToBackendUpdate = false), 0);
        break;
    }
  });
  vscode.postMessage({ type: "init-view" });

  return (
    <>
      {import.meta.env.DEV ? <vscode-dev-toolbar></vscode-dev-toolbar> : null}
      <StateStoreProvider value={State}>
        <vscode-tabs
          selected-index="0"
          style={{ overflow: "hidden", height: "100%" }}
        >
          <vscode-tab-header>Traj</vscode-tab-header>
          <vscode-tab-panel>
            <pre>{JSON.stringify(State.path.serialize)}</pre>
          </vscode-tab-panel>
          <vscode-tab-header>Variables</vscode-tab-header>
          <vscode-tab-panel></vscode-tab-panel>
        </vscode-tabs>
      </StateStoreProvider>
    </>
  );
}

export default observer(App);

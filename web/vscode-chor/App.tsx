import React, { useEffect } from "react";
import { observer } from "mobx-react";
import { createStateStore, StateStoreProvider } from "./state/State";
import RobotConfigView from "./view/RobotConfigView";
import { reaction } from "mobx";
import { vscode } from "./main";
if (import.meta.env.DEV) {

  await import("@vscode-elements/webview-playground");

}
function App() {
  const State = createStateStore();
  let reactingToBackendUpdate=false;
  reaction(()=>State.serialize, (project)=>{
    console.log(project);
    if (!reactingToBackendUpdate) {
    vscode.postMessage({
      type:"update",
      data: JSON.stringify(project)
    })
  }
  })
  window.addEventListener('message', event => {

      const message = event.data; // The JSON data our extension sent

      switch (message.type) {
          case 'update':
          case 'init':
              reactingToBackendUpdate = true;
              State.deserialize(JSON.parse(message.text));
              setTimeout(()=>reactingToBackendUpdate = false, 0);
              break;
      }
  });
  vscode.postMessage({type: "init-view"});

  return (<>
  {import.meta.env.DEV ? <vscode-dev-toolbar></vscode-dev-toolbar> : null}
    <StateStoreProvider value={State}>
      <h3>Countries List</h3>
      <RobotConfigView></RobotConfigView>
    </StateStoreProvider>
  </>

  );
}

export default observer(App);
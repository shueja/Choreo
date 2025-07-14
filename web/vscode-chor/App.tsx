import React, { useEffect } from "react";
import { observer } from "mobx-react";
import { createStateStore, StateStoreProvider } from "./state/State";
import RobotConfigView from "./view/RobotConfigView";
import { reaction } from "mobx";
if (import.meta.env.DEV) {

  await import("@vscode-elements/webview-playground");

}
function App() {
  const State = createStateStore();
  reaction(()=>State.serialize, (project)=>{
    console.log(project);
  })
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
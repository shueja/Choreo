import "@fontsource-variable/roboto-mono/wght-italic.css";
import "@fontsource-variable/roboto-mono";
import "@fontsource/roboto";
import "./App.css";
import { observer } from "mobx-react";
import { ThemeOptions, ThemeProvider, createTheme } from "@mui/material/styles";
import CssBaseline from "@mui/material/CssBaseline";
import Body from "./Body";
import { OverridesStyleRules } from "@mui/material/styles/overrides";
import { ButtonClasses, Theme } from "@mui/material";
// import DocumentModel from "./document/DocumentModel";
import { EXPR_DEFAULTS, createRobotConfigStore } from "@choreo/stores/RobotConfigStore";
import { VariablesStore } from "@choreo/stores/VariablesStore";
import DocumentStore, { DocumentStoreContext } from "./document/DocumentModel";
import {castToSnapshot} from"mobx-state-tree"
import {ChoreoThemeProvider} from "@choreo/components/theme/ChoreoThemeProvider"

function App() {


  const variables = VariablesStore.create({});
  const doc = DocumentStore.create(
      {
        robotConfig: createRobotConfigStore(EXPR_DEFAULTS, variables),
        type: "Swerve",
        pathlist: {
          defaultPath: undefined
        },
        name: "Untitled",
        variables: castToSnapshot(variables),
        selectedSidebarItem: undefined
      }
  );
  
  return (
    <DocumentStoreContext.Provider value={doc}>
      <ChoreoThemeProvider>
        <Body doc={doc}></Body>
      </ChoreoThemeProvider>
    </DocumentStoreContext.Provider>
  );
}
export default observer(App);

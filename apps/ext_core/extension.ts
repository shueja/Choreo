import * as vscode from "vscode";
import AppPanel from "./AppPanel";
import SidebarProvider from "./SidebarProvider";
import ProjectEditorProvider from "./chor/ProjectEditorProvider";
import TrajectoryEditorProvider from "./traj/TrajectoryEditorProvider";
import {PathListProvider} from "./paths/PathListProvider";
import { getStartTrajectoryViewHandler } from "./traj/TrajectoryViewProvider";
export function activate(context: vscode.ExtensionContext) {
  //const sidebarProvider = new SidebarProvider(context.extensionUri);
  const chorEditorProvider = new ProjectEditorProvider(
    context.extensionUri,
    context
  );
  const trajEditorProvider = new TrajectoryEditorProvider(
    context.extensionUri,
    context
  );
  // context.subscriptions.push(
  //   vscode.window.registerWebviewViewProvider("ext-sidebar", sidebarProvider)
  // );
  context.subscriptions.push(
    vscode.window.registerCustomEditorProvider(
      "choreo-chor-editor",
      chorEditorProvider,
      {
        "webviewOptions": {
          retainContextWhenHidden:true
        }
      }
    ),
    vscode.window.registerCustomEditorProvider(
      "choreo-traj-editor",
      trajEditorProvider,
      {
        "webviewOptions": {
          retainContextWhenHidden:true
        }
      }
    ),
    vscode.window.createTreeView('choreo-paths', {
      treeDataProvider: new PathListProvider(vscode.workspace.workspaceFolders![0]!.uri)
    }),
    vscode.commands.registerCommand("choreo-paths.generate", (name:string)=>{vscode.window.showInformationMessage(name)}),
  );
  const trajViewHandler = getStartTrajectoryViewHandler(context);
  context.subscriptions.push(
    vscode.commands.registerCommand("choreo-paths.open", trajViewHandler)
  );

  // context.subscriptions.push(
  //   vscode.commands.registerCommand("vscode-svelte-template.start", () => {
  //     vscode.window.showInformationMessage("hi");
  //   })
  // );
}

export function deactivate() {}

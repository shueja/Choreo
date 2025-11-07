import * as vscode from "vscode";
import ProjectEditorProvider from "./chor/ProjectEditorProvider";
import TrajectoryEditorProvider from "./traj/TrajectoryEditorProvider";
import {PathListProvider} from "./paths/PathListProvider";
import { getStartTrajectoryViewHandler } from "./traj/AppViewProvider";
import { ProjectDirectory, ProjectDirectoryList } from "./ProjectDirectoryManager";
import { ProjectListProvider } from "./projects/ProjectListProvider";
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

  const projectDirectoryManager = new ProjectDirectoryList();
  const projectListProvider = new ProjectListProvider(projectDirectoryManager);
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
      treeDataProvider: new PathListProvider(projectDirectoryManager)
    }),
    vscode.window.createTreeView('choreo-project-selector', {
      treeDataProvider: projectListProvider
    }),
    vscode.commands.registerCommand("choreo-paths.generate", (name:vscode.Uri)=>{console.log(name), vscode.window.showInformationMessage(name.toString())}),
    vscode.commands.registerCommand("choreo-paths.searchForProjects", ()=>projectDirectoryManager.rescanWorkspace())
  );
  const appViewHandler = getStartTrajectoryViewHandler(context);
  // context.subscriptions.push(
  //   vscode.commands.registerCommand("choreo-paths.open", appViewHandler)
  // );

  // context.subscriptions.push(
  //   vscode.commands.registerCommand("vscode-svelte-template.start", () => {
  //     vscode.window.showInformationMessage("hi");
  //   })
  // );
}

export function deactivate() {}

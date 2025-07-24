import * as vscode from "vscode";
import AppPanel from "./AppPanel";
import SidebarProvider from "./SidebarProvider";
import ProjectEditorProvider from "./chor/ProjectEditorProvider";
import TrajectoryEditorProvider from "./traj/TrajectoryEditorProvider";

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
      chorEditorProvider
    ),
    vscode.window.registerCustomEditorProvider(
      "choreo-traj-editor",
      trajEditorProvider
    )
  );

  // context.subscriptions.push(
  //   vscode.commands.registerCommand("vscode-svelte-template.start", () => {
  //     vscode.window.showInformationMessage("hi");
  //   })
  // );
}

export function deactivate() {}

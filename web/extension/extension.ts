import * as vscode from "vscode";
import AppPanel from "./AppPanel";
import SidebarProvider from "./SidebarProvider";
import EditorProvider from "./EditorProvider";

export function activate(context: vscode.ExtensionContext) {
  //const sidebarProvider = new SidebarProvider(context.extensionUri);
  const editorProvider = new EditorProvider(context.extensionUri, context);

  // context.subscriptions.push(
  //   vscode.window.registerWebviewViewProvider("ext-sidebar", sidebarProvider)
  // );
  context.subscriptions.push(
    vscode.window.registerCustomEditorProvider("ext-editor", editorProvider)
  );

  // context.subscriptions.push(
  //   vscode.commands.registerCommand("vscode-svelte-template.start", () => {
  //     vscode.window.showInformationMessage("hi");
  //   })
  // );
}

export function deactivate() {}

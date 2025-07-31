import * as vscode from "vscode";
import { getNonce } from "../Utils";
let currentPanel: vscode.WebviewPanel | undefined = undefined;
export const getStartTrajectoryViewHandler = (context: vscode.ExtensionContext) => () => {
      const columnToShowIn = vscode.window.activeTextEditor
        ? vscode.window.activeTextEditor.viewColumn
        : undefined;

      if (currentPanel) {
        // If we already have a panel, show it in the target column
        currentPanel.reveal(columnToShowIn);
      } else {
        // Otherwise, create a new panel
        currentPanel = vscode.window.createWebviewPanel(
          'choreo-traj-view',
          'Choreo',
          columnToShowIn || vscode.ViewColumn.One,
          {
            retainContextWhenHidden: true
          }
        );
        currentPanel.webview.html = getHtmlForWebview(currentPanel.webview, context);

        // Reset when the current panel is closed
        currentPanel.onDidDispose(
          () => {
            currentPanel = undefined;
          },
          null,
          context.subscriptions
        );
      }}
    ;

    export function setTrajectoryForPanel(trajText: string){           currentPanel?.webview.postMessage({
            type: "initTraj",
            text: trajText
          });}
function getHtmlForWebview(webview: vscode.Webview, context: vscode.ExtensionContext) {
      console.log("getting HTML");
  
      const scriptUri = webview.asWebviewUri(
        vscode.Uri.joinPath(
          context.extensionUri,
          "out",
          "vscode-traj",
          "assets",
          "index.js"
        )
      );
      const styleMainUri = webview.asWebviewUri(
        vscode.Uri.joinPath(context.extensionUri,
          "out",
          "vscode-traj",
          "assets",
          "main.css")
      );
  
      // Use a nonce to only allow a specific script to be run.
      const nonce = getNonce();
  
      return `<!DOCTYPE html>
        <html lang="en">
        <head>
          <meta charset="UTF-8">
          <!--
                      Use a content security policy to only allow loading images from https or from our extension directory,
                      and only allow scripts that have a specific nonce.
          -->
          <meta http-equiv="Content-Security-Policy" content="img-src https: data:; style-src 'unsafe-inline' ${webview.cspSource}; script-src 'nonce-${nonce}'">
          <meta name="viewport" content="width=device-width, initial-scale=1.0">
          <link rel="stylesheet" href="${styleMainUri}"/>
  
        </head>
        <body>
          ${nonce}
         <script nonce="${nonce}" type="module" src="${scriptUri}"></script>
          <div id="root"></div>
        </body>
        </html>`;
    }
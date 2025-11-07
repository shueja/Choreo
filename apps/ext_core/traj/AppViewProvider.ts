import * as vscode from "vscode";
import { getNonce } from "../Utils";
let currentPanel: vscode.WebviewPanel | undefined = undefined;

function addListenersToPanel(context: vscode.ExtensionContext, trajectoryUri: vscode.Uri, chorUri: vscode.Uri) {
  if (currentPanel === undefined) return;
currentPanel.webview.html = getHtmlForWebview(currentPanel.webview, context);

        // Reset when the current panel is closed
        currentPanel.onDidDispose(
          () => {
            currentPanel = undefined;
          },
          null,
          context.subscriptions
        );
        currentPanel.webview.onDidReceiveMessage(async (data: any) => {
          if (currentPanel !== undefined){
              switch (data.type) {
                case "onInfo": {
                  if (!data.value) {
                    return;
                  }
                  vscode.window.showInformationMessage(data.value);
                  break;
                }
                case "onError": {
                  if (!data.value) {
                    return;
                  }
                  vscode.window.showErrorMessage(data.value);
                  break;
                }
                case "init-view": //added this route
                  sendTrajectoryToWebview(trajectoryUri);
                  sendChorToWebview(chorUri);
                  return;
                case "updateTraj":
                  console.log(data);
                  if (!data.data) {
                    return;
                  }
                  vscode.workspace.fs.writeFile(trajectoryUri, new Uint8Array([...data.data].map((c) => c.charCodeAt(0))));
                  return;
              }}});
}

function sendTrajectoryToWebview(trajectoryUri: vscode.Uri): Thenable<void> {

  return  vscode.workspace.openTextDocument(trajectoryUri).then((document) => {
                          currentPanel?.webview.postMessage({
          type: "initTraj",
          text: document.getText()
        });
      });
}

function sendChorToWebview(chorUri: vscode.Uri): Thenable<void> {

  return  vscode.workspace.openTextDocument(chorUri).then((document) => {
                          currentPanel?.webview.postMessage({
          type: "initChor",
          text: document.getText()
        });
      });
}
export const getStartTrajectoryViewHandler = (context: vscode.ExtensionContext) => (trajectoryUri:vscode.Uri, chorUri:vscode.Uri) => {
      const columnToShowIn = vscode.window.activeTextEditor
        ? vscode.window.activeTextEditor.viewColumn
        : undefined;
      
      if (currentPanel) {
        // If we already have a panel, show it in the target column
        currentPanel.reveal(columnToShowIn);
        sendTrajectoryToWebview(trajectoryUri);
        sendChorToWebview(chorUri);

      } else {
        // Otherwise, create a new panel
        currentPanel = vscode.window.createWebviewPanel(
          'choreo-traj-view',
          'Choreo',
          columnToShowIn || vscode.ViewColumn.One,
          {
            retainContextWhenHidden: true,
            enableScripts:true
          }
        );
        addListenersToPanel(context, trajectoryUri, chorUri);
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
         <script nonce="${nonce}" type="module" src="${scriptUri}"></script>
          <div id="root"></div>
        </body>
        </html>`;
    }

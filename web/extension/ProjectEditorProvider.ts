import * as vscode from "vscode";
import { getNonce } from "./Utils";
    //import { readFileSync } from "fs";
import path from "path";

export default class EditorProvider implements vscode.CustomTextEditorProvider {
  _view?: vscode.WebviewPanel;
  _doc?: vscode.TextDocument;

  constructor(private readonly _extensionUri: vscode.Uri, private readonly _context: vscode.ExtensionContext) {}


  public resolveCustomTextEditor(document: vscode.TextDocument, webviewPanel: vscode.WebviewPanel) {
    this._view = webviewPanel;

    webviewPanel.webview.options = {
      // Allow scripts in the webview
      enableScripts: true,

      localResourceRoots: [this._extensionUri],
    };

    webviewPanel.webview.html = this._getHtmlForWebview(webviewPanel.webview);
    function updateWebview() {
			webviewPanel.webview.postMessage({
				type: 'update',
				text: document.getText(),
			});
		}
    let reactingToFrontendUpdate = false;
    const changeDocumentSubscription = vscode.workspace.onDidChangeTextDocument((e:vscode.TextDocumentChangeEvent) => {
			if (e.document.uri.toString() === document.uri.toString() && ! reactingToFrontendUpdate) {
				updateWebview();
			}
		});
    webviewPanel.onDidDispose(() => {
			changeDocumentSubscription.dispose();
		});
    webviewPanel.webview.onDidReceiveMessage(async (data:any) => {
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
        case 'init-view': //added this route
            webviewPanel.webview.postMessage({
                type: 'init',
                text: document.getText(),
            });
            return;
        case 'update': //added in this route
            reactingToFrontendUpdate = true;
            this.updateTextDocument(document, data.data);
            reactingToFrontendUpdate = false;
            return;
      }
    });
  }

  public revive(panel: vscode.WebviewPanel) {
    this._view = panel;
  }

  private _getHtmlForWebview(webview: vscode.Webview) {
    console.log("getting HTML")

    const scriptUri = webview.asWebviewUri(
      vscode.Uri.joinPath(this._extensionUri, "out", "vscode-chor", "assets", "index.js")
    );
    // const styleMainUri = webview.asWebviewUri(
    //   vscode.Uri.joinPath(this._extensionUri, "out", "compiled/editor.css")
    // );

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

        
			</head>
      <body>
        
       <script nonce="${nonce}" type="module" src="${scriptUri}"></script>
        <div id="root" style="position:fixed;height:100%;width:100%;top:0;left:0;overflow:hidden"></div>
			</body>
			</html>`;
  }
  // 

  /**
	 * Try to get a current document as json text.
	 */
	private getDocumentAsJson(document: vscode.TextDocument): any {
		const text = document.getText();
		if (text.trim().length === 0) {
			return {};
		}

		try {
			return JSON.parse(text);
		} catch {
			throw new Error('Could not get document as json. Content is not valid json');
		}
	}

	/**
	 * Write out the json to a given document.
	 */
	private updateTextDocument(document: vscode.TextDocument, json: any) {
		const edit = new vscode.WorkspaceEdit();

		// Just replace the entire document every time for this example extension.
		// A more complete extension should compute minimal edits instead.
		edit.replace(
			document.uri,
			new vscode.Range(0, 0, document.lineCount, 0),
			JSON.stringify(JSON.parse(json), null, 2));

		return vscode.workspace.applyEdit(edit);
	}
}

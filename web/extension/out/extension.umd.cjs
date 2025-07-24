(function(global, factory) {
  typeof exports === "object" && typeof module !== "undefined" ? factory(exports, require("vscode")) : typeof define === "function" && define.amd ? define(["exports", "vscode"], factory) : (global = typeof globalThis !== "undefined" ? globalThis : global || self, factory(global.extension = {}, global.vscode));
})(this, function(exports2, vscode) {
  "use strict";var __defProp = Object.defineProperty;
var __defNormalProp = (obj, key, value) => key in obj ? __defProp(obj, key, { enumerable: true, configurable: true, writable: true, value }) : obj[key] = value;
var __publicField = (obj, key, value) => __defNormalProp(obj, typeof key !== "symbol" ? key + "" : key, value);

  function _interopNamespaceDefault(e) {
    const n = Object.create(null, { [Symbol.toStringTag]: { value: "Module" } });
    if (e) {
      for (const k in e) {
        if (k !== "default") {
          const d = Object.getOwnPropertyDescriptor(e, k);
          Object.defineProperty(n, k, d.get ? d : {
            enumerable: true,
            get: () => e[k]
          });
        }
      }
    }
    n.default = e;
    return Object.freeze(n);
  }
  const vscode__namespace = /* @__PURE__ */ _interopNamespaceDefault(vscode);
  function getNonce() {
    let text = "";
    const possible = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    for (let i = 0; i < 32; i++) {
      text += possible.charAt(Math.floor(Math.random() * possible.length));
    }
    return text;
  }
  let EditorProvider$1 = class EditorProvider {
    constructor(_extensionUri, _context) {
      __publicField(this, "_view");
      __publicField(this, "_doc");
      this._extensionUri = _extensionUri;
      this._context = _context;
    }
    resolveCustomTextEditor(document, webviewPanel) {
      this._view = webviewPanel;
      webviewPanel.webview.options = {
        // Allow scripts in the webview
        enableScripts: true,
        localResourceRoots: [this._extensionUri]
      };
      webviewPanel.webview.html = this._getHtmlForWebview(webviewPanel.webview);
      function updateWebview() {
        webviewPanel.webview.postMessage({
          type: "update",
          text: document.getText()
        });
      }
      let reactingToFrontendUpdate = false;
      const changeDocumentSubscription = vscode__namespace.workspace.onDidChangeTextDocument(
        (e) => {
          if (e.document.uri.toString() === document.uri.toString() && !reactingToFrontendUpdate) {
            updateWebview();
          }
        }
      );
      webviewPanel.onDidDispose(() => {
        changeDocumentSubscription.dispose();
      });
      webviewPanel.webview.onDidReceiveMessage(async (data) => {
        switch (data.type) {
          case "onInfo": {
            if (!data.value) {
              return;
            }
            vscode__namespace.window.showInformationMessage(data.value);
            break;
          }
          case "onError": {
            if (!data.value) {
              return;
            }
            vscode__namespace.window.showErrorMessage(data.value);
            break;
          }
          case "init-view":
            webviewPanel.webview.postMessage({
              type: "init",
              text: document.getText()
            });
            return;
          case "update":
            reactingToFrontendUpdate = true;
            this.updateTextDocument(document, data.data);
            reactingToFrontendUpdate = false;
            return;
        }
      });
    }
    revive(panel) {
      this._view = panel;
    }
    _getHtmlForWebview(webview) {
      console.log("getting HTML");
      const scriptUri = webview.asWebviewUri(
        vscode__namespace.Uri.joinPath(
          this._extensionUri,
          "out",
          "vscode-chor",
          "assets",
          "index.js"
        )
      );
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

       <script nonce="${nonce}" type="module" src="${scriptUri}"><\/script>
        <div id="root" style="position:fixed;height:100%;width:100%;top:0;left:0;overflow:hidden"></div>
			</body>
			</html>`;
    }
    //
    /**
     * Try to get a current document as json text.
     */
    getDocumentAsJson(document) {
      const text = document.getText();
      if (text.trim().length === 0) {
        return {};
      }
      try {
        return JSON.parse(text);
      } catch {
        throw new Error(
          "Could not get document as json. Content is not valid json"
        );
      }
    }
    /**
     * Write out the json to a given document.
     */
    updateTextDocument(document, json) {
      const edit = new vscode__namespace.WorkspaceEdit();
      edit.replace(
        document.uri,
        new vscode__namespace.Range(0, 0, document.lineCount, 0),
        JSON.stringify(JSON.parse(json), null, 2)
      );
      return vscode__namespace.workspace.applyEdit(edit);
    }
  };
  class EditorProvider {
    constructor(_extensionUri, _context) {
      __publicField(this, "_view");
      __publicField(this, "_doc");
      this._extensionUri = _extensionUri;
      this._context = _context;
    }
    resolveCustomTextEditor(document, webviewPanel) {
      this._view = webviewPanel;
      webviewPanel.webview.options = {
        // Allow scripts in the webview
        enableScripts: true,
        localResourceRoots: [this._extensionUri]
      };
      webviewPanel.webview.html = this._getHtmlForWebview(webviewPanel.webview);
      function updateWebview() {
        webviewPanel.webview.postMessage({
          type: "updateTraj",
          text: document.getText()
        });
      }
      let reactingToFrontendUpdate = false;
      const changeDocumentSubscription = vscode__namespace.workspace.onDidChangeTextDocument(
        (e) => {
          if (e.document.uri.toString() === document.uri.toString() && !reactingToFrontendUpdate) {
            updateWebview();
          }
        }
      );
      webviewPanel.onDidDispose(() => {
        changeDocumentSubscription.dispose();
      });
      webviewPanel.webview.onDidReceiveMessage(async (data) => {
        switch (data.type) {
          case "onInfo": {
            if (!data.value) {
              return;
            }
            vscode__namespace.window.showInformationMessage(data.value);
            break;
          }
          case "onError": {
            if (!data.value) {
              return;
            }
            vscode__namespace.window.showErrorMessage(data.value);
            break;
          }
          case "init-view":
            webviewPanel.webview.postMessage({
              type: "initTraj",
              text: document.getText()
            });
            return;
          case "updateTraj":
            reactingToFrontendUpdate = true;
            this.updateTextDocument(document, data.data);
            reactingToFrontendUpdate = false;
            return;
        }
      });
    }
    revive(panel) {
      this._view = panel;
    }
    _getHtmlForWebview(webview) {
      console.log("getting HTML");
      const scriptUri = webview.asWebviewUri(
        vscode__namespace.Uri.joinPath(
          this._extensionUri,
          "out",
          "vscode-traj",
          "assets",
          "index.js"
        )
      );
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

       <script nonce="${nonce}" type="module" src="${scriptUri}"><\/script>
        <div id="root" style="position:fixed;height:100%;width:100%;top:0;left:0;overflow:hidden"></div>
            </body>
            </html>`;
    }
    //
    /**
     * Try to get a current document as json text.
     */
    getDocumentAsJson(document) {
      const text = document.getText();
      if (text.trim().length === 0) {
        return {};
      }
      try {
        return JSON.parse(text);
      } catch {
        throw new Error(
          "Could not get document as json. Content is not valid json"
        );
      }
    }
    /**
     * Write out the json to a given document.
     */
    updateTextDocument(document, json) {
      const edit = new vscode__namespace.WorkspaceEdit();
      edit.replace(
        document.uri,
        new vscode__namespace.Range(0, 0, document.lineCount, 0),
        JSON.stringify(JSON.parse(json), null, 2)
      );
      return vscode__namespace.workspace.applyEdit(edit);
    }
  }
  function activate(context) {
    const chorEditorProvider = new EditorProvider$1(
      context.extensionUri,
      context
    );
    const trajEditorProvider = new EditorProvider(
      context.extensionUri,
      context
    );
    context.subscriptions.push(
      vscode__namespace.window.registerCustomEditorProvider(
        "choreo-chor-editor",
        chorEditorProvider
      ),
      vscode__namespace.window.registerCustomEditorProvider(
        "choreo-traj-editor",
        trajEditorProvider
      )
    );
  }
  function deactivate() {
  }
  exports2.activate = activate;
  exports2.deactivate = deactivate;
  Object.defineProperty(exports2, Symbol.toStringTag, { value: "Module" });
});
//# sourceMappingURL=extension.umd.cjs.map

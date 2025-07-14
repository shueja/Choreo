var a = Object.defineProperty;
var u = (o, t, e) => t in o ? a(o, t, { enumerable: !0, configurable: !0, writable: !0, value: e }) : o[t] = e;
var n = (o, t, e) => u(o, typeof t != "symbol" ? t + "" : t, e);
import * as s from "vscode";
function p() {
  let o = "";
  const t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  for (let e = 0; e < 32; e++)
    o += t.charAt(Math.floor(Math.random() * t.length));
  return o;
}
class d {
  constructor(t) {
    n(this, "_view");
    n(this, "_doc");
    this._extensionUri = t;
  }
  resolveCustomTextEditor(t, e) {
    this._view = e, e.webview.options = {
      // Allow scripts in the webview
      enableScripts: !0,
      localResourceRoots: [this._extensionUri]
    }, e.webview.html = this._getHtmlForWebview(e.webview);
    function i() {
      e.webview.postMessage({
        type: "update",
        text: t.getText()
      });
    }
    const c = s.workspace.onDidChangeTextDocument((r) => {
      r.document.uri.toString() === t.uri.toString() && i();
    });
    e.onDidDispose(() => {
      c.dispose();
    }), e.webview.onDidReceiveMessage(async (r) => {
      switch (r.type) {
        case "onInfo": {
          if (!r.value)
            return;
          s.window.showInformationMessage(r.value);
          break;
        }
        case "onError": {
          if (!r.value)
            return;
          s.window.showErrorMessage(r.value);
          break;
        }
        case "init-view":
          e.webview.postMessage({
            type: "init",
            text: t.getText()
          });
          return;
        case "update":
          this.updateTextDocument(t, r.data);
          return;
      }
    });
  }
  revive(t) {
    this._view = t;
  }
  _getHtmlForWebview(t) {
    console.log("getting HTML");
    const e = t.asWebviewUri(
      s.Uri.joinPath(this._extensionUri, "out", "vscode-chor", "assets", "main-D6pdcp6R.js")
    ), i = p();
    return `<!DOCTYPE html>
			<html lang="en">
			<head>
				<meta charset="UTF-8">
				<!--
					Use a content security policy to only allow loading images from https or from our extension directory,
					and only allow scripts that have a specific nonce.
        -->
        <meta http-equiv="Content-Security-Policy" content="img-src https: data:; style-src 'unsafe-inline' ${t.cspSource}; script-src 'nonce-${i}'">
				<meta name="viewport" content="width=device-width, initial-scale=1.0">

        
			</head>
      <body>
        
				  <script nonce="${i}">const vscode = acquireVsCodeApi()<\/script>
       <script nonce="${i}" type="module" src="${e}"><\/script>
        <div id="root"></div>

        Hi
			</body>
			</html>`;
  }
  // 
  /**
  * Try to get a current document as json text.
  */
  getDocumentAsJson(t) {
    const e = t.getText();
    if (e.trim().length === 0)
      return {};
    try {
      return JSON.parse(e);
    } catch {
      throw new Error("Could not get document as json. Content is not valid json");
    }
  }
  /**
   * Write out the json to a given document.
   */
  updateTextDocument(t, e) {
    const i = new s.WorkspaceEdit();
    return i.replace(
      t.uri,
      new s.Range(0, 0, t.lineCount, 0),
      JSON.stringify(JSON.parse(e), null, 2)
    ), s.workspace.applyEdit(i);
  }
}
function h(o) {
  const t = new d(o.extensionUri);
  o.subscriptions.push(
    s.window.registerCustomEditorProvider("ext-editor", t)
  );
}
function g() {
}
export {
  h as activate,
  g as deactivate
};
//# sourceMappingURL=extension.js.map

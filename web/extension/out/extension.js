var a = Object.defineProperty;
var u = (o, t, e) => t in o ? a(o, t, { enumerable: !0, configurable: !0, writable: !0, value: e }) : o[t] = e;
var n = (o, t, e) => u(o, typeof t != "symbol" ? t + "" : t, e);
import * as r from "vscode";
function p() {
  let o = "";
  const t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  for (let e = 0; e < 32; e++)
    o += t.charAt(Math.floor(Math.random() * t.length));
  return o;
}
class d {
  constructor(t, e) {
    n(this, "_view");
    n(this, "_doc");
    this._extensionUri = t, this._context = e;
  }
  resolveCustomTextEditor(t, e) {
    this._view = e, e.webview.options = {
      // Allow scripts in the webview
      enableScripts: !0,
      localResourceRoots: [this._extensionUri]
    }, e.webview.html = this._getHtmlForWebview(e.webview);
    function s() {
      e.webview.postMessage({
        type: "update",
        text: t.getText()
      });
    }
    const c = r.workspace.onDidChangeTextDocument((i) => {
      i.document.uri.toString() === t.uri.toString() && s();
    });
    e.onDidDispose(() => {
      c.dispose();
    }), e.webview.onDidReceiveMessage(async (i) => {
      switch (i.type) {
        case "onInfo": {
          if (!i.value)
            return;
          r.window.showInformationMessage(i.value);
          break;
        }
        case "onError": {
          if (!i.value)
            return;
          r.window.showErrorMessage(i.value);
          break;
        }
        case "init-view":
          e.webview.postMessage({
            type: "init",
            text: t.getText()
          });
          return;
        case "update":
          this.updateTextDocument(t, i.data);
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
      r.Uri.joinPath(this._extensionUri, "out", "vscode-chor", "assets", "index.js")
    ), s = p();
    return `<!DOCTYPE html>
			<html lang="en">
			<head>
				<meta charset="UTF-8">
				<!--
					Use a content security policy to only allow loading images from https or from our extension directory,
					and only allow scripts that have a specific nonce.
        -->
        <meta http-equiv="Content-Security-Policy" content="img-src https: data:; style-src 'unsafe-inline' ${t.cspSource}; script-src 'nonce-${s}'">
				<meta name="viewport" content="width=device-width, initial-scale=1.0">

        
			</head>
      <body>
        
       <script nonce="${s}" type="module" src="${e}"><\/script>
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
    const s = new r.WorkspaceEdit();
    return s.replace(
      t.uri,
      new r.Range(0, 0, t.lineCount, 0),
      JSON.stringify(JSON.parse(e), null, 2)
    ), r.workspace.applyEdit(s);
  }
}
function h(o) {
  const t = new d(o.extensionUri, o);
  o.subscriptions.push(
    r.window.registerCustomEditorProvider("ext-editor", t)
  );
}
function g() {
}
export {
  h as activate,
  g as deactivate
};
//# sourceMappingURL=extension.js.map

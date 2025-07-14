var d = Object.defineProperty;
var w = (s, e, t) => e in s ? d(s, e, { enumerable: !0, configurable: !0, writable: !0, value: t }) : s[e] = t;
var a = (s, e, t) => w(s, typeof e != "symbol" ? e + "" : e, t);
import * as i from "vscode";
function u() {
  let s = "";
  const e = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  for (let t = 0; t < 32; t++)
    s += e.charAt(Math.floor(Math.random() * e.length));
  return s;
}
const p = "Svelte Extension", v = "Franklin Shera", r = class r {
  constructor(e, t) {
    a(this, "_panel");
    a(this, "_extensionUri");
    a(this, "_disposables", []);
    this._panel = e, this._extensionUri = t, this._update(), this._panel.onDidDispose(() => this.dispose(), null, this._disposables), this._panel.onDidChangeViewState(
      (n) => {
        this._panel.visible && this._update();
      },
      null,
      this._disposables
    ), this._panel.webview.onDidReceiveMessage(
      (n) => {
        switch (n.command) {
          case "alert":
            i.window.showErrorMessage(n.text);
            return;
        }
      },
      null,
      this._disposables
    );
  }
  static createOrShow(e) {
    const t = i.window.activeTextEditor ? i.window.activeTextEditor.viewColumn : void 0;
    if (r.currentPanel) {
      r.currentPanel._panel.reveal(t);
      return;
    }
    const n = i.window.createWebviewPanel(
      r.viewType,
      "App Panel",
      t || i.ViewColumn.One,
      {
        // Enable javascript in the webview
        enableScripts: !0,
        // And restrict the webview to only loading content from our extension's `media` directory.
        localResourceRoots: [i.Uri.joinPath(e, "media")]
      }
    );
    r.currentPanel = new r(n, e);
  }
  static revive(e, t) {
    r.currentPanel = new r(e, t);
  }
  doRefactor() {
    this._panel.webview.postMessage({ command: "refactor" });
  }
  dispose() {
    for (r.currentPanel = void 0, this._panel.dispose(); this._disposables.length; ) {
      const e = this._disposables.pop();
      e && e.dispose();
    }
  }
  _update() {
    const e = this._panel.webview;
    switch (this._panel.viewColumn) {
      case i.ViewColumn.Two:
        this._updateAppPanel(e);
        return;
      case i.ViewColumn.Three:
        this._updateAppPanel(e);
        return;
      case i.ViewColumn.One:
      default:
        this._updateAppPanel(e);
        return;
    }
  }
  _updateAppPanel(e) {
    this._panel.title = p, this._panel.webview.html = this._getHtmlForWebview(e);
  }
  _getHtmlForWebview(e) {
    const t = e.asWebviewUri(
      i.Uri.joinPath(this._extensionUri, "ouc/compiled", "app.js")
    ), n = e.asWebviewUri(
      i.Uri.joinPath(this._extensionUri, "media", "reset.css")
    ), c = e.asWebviewUri(
      i.Uri.joinPath(this._extensionUri, "media", "vscode.css")
    ), o = u();
    return `<!DOCTYPE html>
			<html lang="en">
			<head>
				<meta charset="UTF-8">
				<!--
					Use a content security policy to only allow loading images from https or from our extension directory,
					and only allow scripts that have a specific nonce.
				-->
				<meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src ${e.cspSource}; img-src ${e.cspSource} https:; script-src 'nonce-${o}';">
				<meta name="viewport" content="width=device-width, initial-scale=1.0">
				<link href="${n}" rel="stylesheet">
				<link href="${c}" rel="stylesheet">
				<title>App Panel</title>
			</head>
			<body>
				
				<h1>${p} BY ${v}</h1>
        <input />
        <button>Hey</button>
				<script nonce="${o}" type="module" src="${t}"><\/script>
			</body>
			</html>`;
  }
};
/**
 * Track the currently panel. Only allow a single panel to exist at a time.
 */
a(r, "currentPanel"), a(r, "viewType", "appPanel");
let h = r;
class m {
  constructor(e) {
    a(this, "_view");
    a(this, "_doc");
    this._extensionUri = e;
  }
  resolveWebviewView(e) {
    this._view = e, e.webview.options = {
      // Allow scripts in the webview
      enableScripts: !0,
      localResourceRoots: [this._extensionUri]
    }, e.webview.html = this._getHtmlForWebview(e.webview), e.webview.onDidReceiveMessage(async (t) => {
      switch (t.type) {
        case "onInfo": {
          if (!t.value)
            return;
          i.window.showInformationMessage(t.value);
          break;
        }
        case "onError": {
          if (!t.value)
            return;
          i.window.showErrorMessage(t.value);
          break;
        }
      }
    });
  }
  revive(e) {
    this._view = e;
  }
  _getHtmlForWebview(e) {
    const t = e.asWebviewUri(
      i.Uri.joinPath(this._extensionUri, "media", "reset.css")
    ), n = e.asWebviewUri(
      i.Uri.joinPath(this._extensionUri, "media", "vscode.css")
    ), c = e.asWebviewUri(
      i.Uri.joinPath(this._extensionUri, "out", "compiled/sidebar.js")
    ), o = e.asWebviewUri(
      i.Uri.joinPath(this._extensionUri, "out", "compiled/sidebar.css")
    ), l = u();
    return `<!DOCTYPE html>
			<html lang="en">
			<head>
				<meta charset="UTF-8">
				<!--
					Use a content security policy to only allow loading images from https or from our extension directory,
					and only allow scripts that have a specific nonce.
        -->
        <meta http-equiv="Content-Security-Policy" content="img-src https: data:; style-src 'unsafe-inline' ${e.cspSource}; script-src 'nonce-${l}';">
				<meta name="viewport" content="width=device-width, initial-scale=1.0">
				<link href="${t}" rel="stylesheet">
				<link href="${n}" rel="stylesheet">
        <link href="${o}" rel="stylesheet">
        
			</head>
      <body>
				<script nonce="${l}" type="module" src="${c}"><\/script>
			</body>
			</html>`;
  }
}
class b {
  constructor(e, t) {
    a(this, "_view");
    a(this, "_doc");
    this._extensionUri = e, this._context = t;
  }
  resolveCustomTextEditor(e, t) {
    this._view = t, t.webview.options = {
      // Allow scripts in the webview
      enableScripts: !0,
      localResourceRoots: [this._extensionUri]
    }, t.webview.html = this._getHtmlForWebview(t.webview, this._context);
    function n() {
      t.webview.postMessage({
        type: "update",
        text: e.getText()
      });
    }
    const c = i.workspace.onDidChangeTextDocument((o) => {
      o.document.uri.toString() === e.uri.toString() && n();
    });
    t.onDidDispose(() => {
      c.dispose();
    }), t.webview.onDidReceiveMessage(async (o) => {
      switch (o.type) {
        case "onInfo": {
          if (!o.value)
            return;
          i.window.showInformationMessage(o.value);
          break;
        }
        case "onError": {
          if (!o.value)
            return;
          i.window.showErrorMessage(o.value);
          break;
        }
        case "init-view":
          t.webview.postMessage({
            type: "init",
            text: e.getText()
          });
          return;
        case "update":
          this.updateTextDocument(e, o.data);
          return;
      }
    });
  }
  revive(e) {
    this._view = e;
  }
  _getHtmlForWebview(e, t) {
    const n = require("path"), c = require("fs"), o = t.asAbsolutePath(n.join("src", "html", "HTMLView.html"));
    return c.readFileSync(o, "utf8");
  }
  /**
  * Try to get a current document as json text.
  */
  getDocumentAsJson(e) {
    const t = e.getText();
    if (t.trim().length === 0)
      return {};
    try {
      return JSON.parse(t);
    } catch {
      throw new Error("Could not get document as json. Content is not valid json");
    }
  }
  /**
   * Write out the json to a given document.
   */
  updateTextDocument(e, t) {
    const n = new i.WorkspaceEdit();
    return n.replace(
      e.uri,
      new i.Range(0, 0, e.lineCount, 0),
      JSON.stringify(JSON.parse(t), null, 2)
    ), i.workspace.applyEdit(n);
  }
}
function y(s) {
  const e = new m(s.extensionUri), t = new b(s.extensionUri);
  s.subscriptions.push(
    i.window.registerWebviewViewProvider("ext-sidebar", e)
  ), s.subscriptions.push(
    i.window.registerCustomEditorProvider("ext-editor", t)
  ), s.subscriptions.push(
    i.commands.registerCommand("vscode-svelte-template.start", () => {
      h.createOrShow(s.extensionUri);
    })
  );
}
function g() {
}
export {
  y as activate,
  g as deactivate
};
//# sourceMappingURL=extension.js.map

(function(o,i){typeof exports=="object"&&typeof module<"u"?i(exports,require("vscode")):typeof define=="function"&&define.amd?define(["exports","vscode"],i):(o=typeof globalThis<"u"?globalThis:o||self,i(o.extension={},o.vscode))})(this,function(o,i){"use strict";var v=Object.defineProperty;var m=(o,i,a)=>i in o?v(o,i,{enumerable:!0,configurable:!0,writable:!0,value:a}):o[i]=a;var d=(o,i,a)=>m(o,typeof i!="symbol"?i+"":i,a);function a(n){const e=Object.create(null,{[Symbol.toStringTag]:{value:"Module"}});if(n){for(const t in n)if(t!=="default"){const r=Object.getOwnPropertyDescriptor(n,t);Object.defineProperty(e,t,r.get?r:{enumerable:!0,get:()=>n[t]})}}return e.default=n,Object.freeze(e)}const s=a(i);function l(){let n="";const e="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";for(let t=0;t<32;t++)n+=e.charAt(Math.floor(Math.random()*e.length));return n}class p{constructor(e,t){d(this,"_view");d(this,"_doc");this._extensionUri=e,this._context=t}resolveCustomTextEditor(e,t){this._view=t,t.webview.options={enableScripts:!0,localResourceRoots:[this._extensionUri]},t.webview.html=this._getHtmlForWebview(t.webview);function r(){t.webview.postMessage({type:"update",text:e.getText()})}let u=!1;const g=s.workspace.onDidChangeTextDocument(c=>{c.document.uri.toString()===e.uri.toString()&&!u&&r()});t.onDidDispose(()=>{g.dispose()}),t.webview.onDidReceiveMessage(async c=>{switch(c.type){case"onInfo":{if(!c.value)return;s.window.showInformationMessage(c.value);break}case"onError":{if(!c.value)return;s.window.showErrorMessage(c.value);break}case"init-view":t.webview.postMessage({type:"init",text:e.getText()});return;case"update":u=!0,this.updateTextDocument(e,c.data),u=!1;return}})}revive(e){this._view=e}_getHtmlForWebview(e){console.log("getting HTML");const t=e.asWebviewUri(s.Uri.joinPath(this._extensionUri,"out","vscode-chor","assets","index.js")),r=l();return`<!DOCTYPE html>
			<html lang="en">
			<head>
				<meta charset="UTF-8">
				<!--
					Use a content security policy to only allow loading images from https or from our extension directory,
					and only allow scripts that have a specific nonce.
        -->
        <meta http-equiv="Content-Security-Policy" content="img-src https: data:; style-src 'unsafe-inline' ${e.cspSource}; script-src 'nonce-${r}'">
				<meta name="viewport" content="width=device-width, initial-scale=1.0">

        
			</head>
      <body>
        
       <script nonce="${r}" type="module" src="${t}"><\/script>
        <div id="root" style="position:fixed;height:100%;width:100%;top:0;left:0;overflow:hidden"></div>
			</body>
			</html>`}getDocumentAsJson(e){const t=e.getText();if(t.trim().length===0)return{};try{return JSON.parse(t)}catch{throw new Error("Could not get document as json. Content is not valid json")}}updateTextDocument(e,t){const r=new s.WorkspaceEdit;return r.replace(e.uri,new s.Range(0,0,e.lineCount,0),JSON.stringify(JSON.parse(t),null,2)),s.workspace.applyEdit(r)}}function h(n){const e=new p(n.extensionUri,n);n.subscriptions.push(s.window.registerCustomEditorProvider("choreo-chor-editor",e))}function f(){}o.activate=h,o.deactivate=f,Object.defineProperty(o,Symbol.toStringTag,{value:"Module"})});
//# sourceMappingURL=extension.umd.cjs.map

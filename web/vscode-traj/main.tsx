import ReactDOM from "react-dom/client";
import App from "./App";
//import "./styles.css";

if (import.meta.env.DEV) {
  await import("@vscode-elements/webview-playground");
}

//@ts-expect-error
export const vscode = acquireVsCodeApi();

ReactDOM.createRoot(document.getElementById("root") as HTMLElement).render(
  <>
    <App></App>
  </>
);

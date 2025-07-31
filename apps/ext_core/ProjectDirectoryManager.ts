import * as vscode from "vscode";
let projectDirectoryUri: vscode.Uri | undefined = undefined;
export function getWorkspaceRoot():vscode.Uri {
    return vscode.workspace.workspaceFolders![0]!.uri
}
export async function checkForProjects(context: vscode.ExtensionContext): Promise<vscode.Uri[]> {
    let choreoDir = vscode.Uri.joinPath(getWorkspaceRoot(), "src","main","deploy","choreo");
    return (await vscode.workspace.findFiles("/src/main/deploy/choreo/*.chor"));
}
import * as vscode from "vscode";
let projectDirectoryUri: vscode.Uri | undefined = undefined;
export function getWorkspaceRoot():vscode.Uri {
    return vscode.workspace.workspaceFolders![0]!.uri
}
export async function checkForProjects(context: vscode.ExtensionContext): Promise<vscode.Uri[]> {
    let choreoDir = vscode.Uri.joinPath(getWorkspaceRoot(), "src","main","deploy","choreo");
    let chor = (await vscode.workspace.fs.readDirectory(choreoDir)).find(f=>{
                        return f[1]==vscode.FileType.File && f[0].slice(-5) === ".chor"});
    if (chor != undefined) {
        return [choreoDir];
    }
    return [];

    //return (await vscode.workspace.findFiles("/src/main/deploy/choreo/*.chor"));
}
import * as vscode from 'vscode';
import { getChor, getTrajsInProject } from '../ProjectDirectoryManager';

export class PathListProvider implements vscode.TreeDataProvider<Dependency> {

	private _onDidChangeTreeData: vscode.EventEmitter<Dependency | undefined | void> = new vscode.EventEmitter<Dependency | undefined | void>();
	readonly onDidChangeTreeData: vscode.Event<Dependency | undefined | void> = this._onDidChangeTreeData.event;

	constructor(private workspaceRoot: vscode.Uri | undefined) {
	}

	refresh(): void {
		this._onDidChangeTreeData.fire();
	}

	getTreeItem(element: Dependency): vscode.TreeItem {
		return element;
	}

	async getChildren(element?: Dependency):Promise<Dependency[]> {
		if (!this.workspaceRoot) {
			vscode.window.showInformationMessage('No dependency in empty workspace');
			return Promise.resolve([]);
		}

		const projectDirectory = vscode.Uri.joinPath(this.workspaceRoot, 'src/main/deploy/choreo');
				return this.getPathsInProject(projectDirectory);

	}
	 private async getPathsInProject(projectDirectory: vscode.Uri): Promise<Dependency[]> {
		return (await getTrajsInProject(projectDirectory))
                .map(uri=>new Dependency(
                    uri.toString().slice(0, -5),
                    vscode.TreeItemCollapsibleState.None,
                    uri,
				await getChor()));
	}
}

// This object is passed to the handler for the "Generate" button on the 
export class Dependency extends vscode.TreeItem {

	constructor(
		public readonly label: string,
		public readonly collapsibleState: vscode.TreeItemCollapsibleState,
        public readonly itemUri: vscode.Uri,
		public readonly chorUri: vscode.Uri

	) {
		super(label, collapsibleState);
        this.command = {
                
                command: 'choreo-paths.open',
                title: 'gen',
                arguments: [itemUri]
            
        }
	}

	contextValue = 'path';
} 
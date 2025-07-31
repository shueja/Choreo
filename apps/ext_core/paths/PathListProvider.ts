import * as vscode from 'vscode';

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

		const packageJsonPath = vscode.Uri.joinPath(this.workspaceRoot, 'src/main/deploy/choreo');
				return this.getDepsInPackageJson(packageJsonPath);

	}

	/**
	 * Given the path to package.json, read all its dependencies and devDependencies.
	 */
	 private async getDepsInPackageJson(packageJsonPath: vscode.Uri): Promise<Dependency[]> {
		const workspaceRoot = this.workspaceRoot;
		if (workspaceRoot) {
			const files = await vscode.workspace.fs.readDirectory(packageJsonPath);
            return files
                .filter(f=>{
                    return f[1]==vscode.FileType.File && f[0].slice(-5) === ".traj"})
                .map(entry=>new Dependency(
                    entry[0].slice(0, -5),
                    vscode.TreeItemCollapsibleState.None,
                    vscode.Uri.joinPath(packageJsonPath, entry[0])));

		} else {
			return [];
		}
	}
}

export class Dependency extends vscode.TreeItem {

	constructor(
		public readonly label: string,
		public readonly collapsibleState: vscode.TreeItemCollapsibleState,
        public readonly itemUri: vscode.Uri

	) {
		super(label, collapsibleState);
        this.command = {
                
                command: 'vscode.open',
                title: 'gen',
                arguments: [itemUri]
            
        }
	}

	contextValue = 'path';
}
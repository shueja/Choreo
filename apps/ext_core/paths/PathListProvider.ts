import * as vscode from 'vscode';
import { ProjectDirectory, ProjectDirectoryList } from '../ProjectDirectoryManager';
// import { getChor, getTrajsInProject } from '../ProjectDirectoryManager';

export class PathListProvider implements vscode.TreeDataProvider<Item> {

	private _onDidChangeTreeData: vscode.EventEmitter<Item | undefined | void> = new vscode.EventEmitter<Item | undefined | void>();
	readonly onDidChangeTreeData: vscode.Event<Item | undefined | void> = this._onDidChangeTreeData.event;

	readonly disposeProjectListListener: vscode.Disposable;
	
		constructor(readonly projectDirectoryList: ProjectDirectoryList) {
			this.disposeProjectListListener = projectDirectoryList.subscribeToChanges(()=>this.refresh());
		}
	
		refresh(): void {
			this._onDidChangeTreeData.fire();
		}

	getTreeItem(element: Item): vscode.TreeItem {
		return element;
	}

	async getChildren(element?: Item):Promise<Item[]> {
		if (element === undefined)
			return this.projectDirectoryList.getProjects().map((dir : ProjectDirectory)=>
				new Item(`${dir.workspaceName} (${dir.chorName})`, dir, undefined));
		else {
			return element.getChildren();
		}


	}
}

// This object is passed to the handler for the "Generate" button on the 
export class Item extends vscode.TreeItem {

	constructor(
		public readonly label: string,
        public readonly projectDirectory : ProjectDirectory | undefined,
		public readonly command: vscode.Command | undefined
	) {
		super(label, projectDirectory !== undefined ? vscode.TreeItemCollapsibleState.Expanded : vscode.TreeItemCollapsibleState.None);
	}

	getChildren() {
		if (this.projectDirectory !== undefined) {
			let projectDirectory = this.projectDirectory;
			return projectDirectory.trajectories.map((uri)=>{console.log(uri); return new Item(
				uri.trajName, undefined, {
                
                command: 'vscode.open',
                title: 'gen',
                arguments: [uri.uri(projectDirectory.directory), projectDirectory.chorUri]
            
        		}
			)})
		} else {
			return [];
		}
	}

	contextValue = 'path';
} 
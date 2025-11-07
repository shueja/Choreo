import * as vscode from 'vscode';
import { ProjectDirectory, ProjectDirectoryList } from '../ProjectDirectoryManager';
// import { getChor, getTrajsInProject } from '../ProjectDirectoryManager';

export class ProjectListProvider implements vscode.TreeDataProvider<ProjectDirectory> {

    private _onDidChangeTreeData: vscode.EventEmitter<ProjectDirectory | undefined | void> = new vscode.EventEmitter<ProjectDirectory | undefined | void>();
    readonly onDidChangeTreeData: vscode.Event<ProjectDirectory | undefined | void> = this._onDidChangeTreeData.event;
    readonly disposeProjectListListener: vscode.Disposable;

    constructor(readonly projectDirectoryList: ProjectDirectoryList) {
        this.disposeProjectListListener = projectDirectoryList.subscribeToChanges(()=>this.refresh());
    }

    refresh(): void {
        this._onDidChangeTreeData.fire();
    }

    getTreeItem(element: ProjectDirectory): vscode.TreeItem {
        return new vscode.TreeItem(`${element.workspaceName} (${element.chorName})`);
    }

    async getChildren(element?: ProjectDirectory):Promise<ProjectDirectory[]> {

        return this.projectDirectoryList.getProjects();

    }

}
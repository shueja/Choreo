import * as vscode from "vscode";

export const TRAJ_EXTENSION = "traj"
export const CHOR_EXTENSION = "chor"

export class ProjectDirectoryList { 
    projectListUpdated = new vscode.EventEmitter<void>();
    projects : ProjectDirectory[] = []
    subscribeToChanges(listener: (e:void)=>any) : vscode.Disposable {
        return this.projectListUpdated.event(listener)
    }
    setProjectList(projectDirectories: ProjectDirectory[]) {
        this.projects = projectDirectories;
        this.projectListUpdated.fire();
    }
    getProjects() : ProjectDirectory[] {
        return this.projects;
    }
    async rescanWorkspace() {
        let projects = [];
        for (const workspace of vscode.workspace.workspaceFolders ?? []) {
            const projectInWorkspace = await ProjectDirectory.identifyProjectDirectory(workspace);
            if (projectInWorkspace !== undefined) {
                projects.push(projectInWorkspace)
            }

        }
        this.setProjectList(projects);
    }
}


export class TrajectoryFileUri {
    constructor(
        public trajName: string
    ){}
    uri(parent: vscode.Uri) {return vscode.Uri.joinPath(parent, this.trajName + "." + TRAJ_EXTENSION)}
}
export class ProjectDirectory {
    trajectories: TrajectoryFileUri[] = [];
    trajWatcher: vscode.FileSystemWatcher;
    chorWatcher: vscode.FileSystemWatcher;
    directory: vscode.Uri;
    workspaceName: string;
    private constructor(
        private workspaceFolder :vscode.WorkspaceFolder,
        private relativePath: string,
        readonly chorName: string 
    ) { 
        this.workspaceName = workspaceFolder.name;
        
        this.directory = vscode.Uri.joinPath(this.workspaceFolder.uri, relativePath);
        console.error(this.directory);
        this.trajWatcher = this.watchTrajList();
        this.chorWatcher = this.watchChor(this.chorName);
        this.updateTrajectoryList();
        

    }

    watchTrajList() {
        if (this.trajWatcher) {this.trajWatcher.dispose();}
        const trajWatcher = vscode.workspace.createFileSystemWatcher(
            new vscode.RelativePattern(this.workspaceFolder,
                `${this.relativePath.replace("\\", "/")}/*.${TRAJ_EXTENSION}`));
        trajWatcher.onDidCreate((trajUri)=>vscode.window.showInformationMessage("created " + trajUri.toString()))
        trajWatcher.onDidDelete((trajUri)=>vscode.window.showInformationMessage("deleted " + trajUri.toString()))
        trajWatcher.onDidChange((trajUri)=>vscode.window.showInformationMessage("changed " + trajUri.toString()))
        return trajWatcher;
    }
    watchChor(chorName: string) {
        if (this.chorWatcher) {this.chorWatcher.dispose();}
        const chorWatcher = vscode.workspace.createFileSystemWatcher(
            new vscode.RelativePattern(this.workspaceFolder,
                `${this.relativePath.replace("\\", "/")}/${chorName}.${CHOR_EXTENSION}`));

        chorWatcher.onDidDelete((chorUri)=>vscode.window.showInformationMessage("don't delete your .chor"));
        chorWatcher.onDidChange((chorUri)=>vscode.window.showInformationMessage("changed " + chorUri.toString()))
        return chorWatcher;
    }
    dispose() {
        this.trajWatcher.dispose();
        this.chorWatcher.dispose();
    }



    get chorUri() {return vscode.Uri.joinPath(this.directory, this.chorName + "." + CHOR_EXTENSION)}
    async updateTrajectoryList() : Promise<TrajectoryFileUri[]> {
        const files = await vscode.workspace.fs.readDirectory(this.directory);
        const trajectories = files
            .filter(f=>{
                return f[1]==vscode.FileType.File && f[0].slice(-5) === ".traj"})
            .map(([file, type])=>new TrajectoryFileUri(file.slice(0, -5)));
        this.trajectories = trajectories;
        this.dispose();
        this.watchTrajList();
        this.watchChor(this.chorName);
        return trajectories;
    }
    /**
     * Reads the given directory and populates a ProjectDirectory with the .chor filename and any trajectories, if found.
     * If no .chor found in the directory, resolves to undefined.
     * @param directory 
     */
    static async identifyProjectDirectory(workspaceFolder:vscode.WorkspaceFolder) : Promise<ProjectDirectory | undefined>{
        // search order matters
        const supportedWorkspaceRelativePaths = ["src/main/deploy/choreo"];
        for (let path of supportedWorkspaceRelativePaths) {
            let possibleProjectDirectory = vscode.Uri.joinPath(workspaceFolder.uri, path);
            let chorEntry = (await vscode.workspace.fs.readDirectory(possibleProjectDirectory)).find(f=>{
                        return f[1]==vscode.FileType.File && f[0].slice(-5) === ".chor"});
            if (chorEntry !== undefined) {
                return new ProjectDirectory(workspaceFolder, path, chorEntry[0].slice(0, -5));
            }
            
        }
        return undefined;
    }
}


import * as vscode from "vscode";

export const TRAJ_EXTENSION = "traj"
export const CHOR_EXTENSION = "chor"
class TrajectoryFileUri {
    constructor(
        public trajName: string
    ){}
    uri(parent: vscode.Uri) {return vscode.Uri.joinPath(parent, this.trajName + "." + TRAJ_EXTENSION)}
}
class ProjectDirectory {
    trajectories: TrajectoryFileUri[] = [];
    private constructor(
        private directory: vscode.Uri,
        private chorName: string 
    ) {
        this.updateTrajectoryList();
    }

    get chorUri() {return vscode.Uri.joinPath(this.directory, this.chorName + "." + TRAJ_EXTENSION)}
    async updateTrajectoryList() : Promise<TrajectoryFileUri[]> {
        const files = await vscode.workspace.fs.readDirectory(this.directory);
        const trajectories = files
            .filter(f=>{
                return f[1]==vscode.FileType.File && f[0].slice(-5) === ".traj"})
            .map(([file, type])=>new TrajectoryFileUri(file));
        this.trajectories = trajectories;
        return trajectories;
    }
    /**
     * Reads the given directory and populates a ProjectDirectory with the .chor filename and any trajectories, if found.
     * If no .chor found in the directory, resolves to undefined.
     * @param directory 
     */
    static async identifyProjectDirectory(directory: vscode.Uri) : Promise<ProjectDirectory | undefined>{
        let chorEntry = (await vscode.workspace.fs.readDirectory(directory)).find(f=>{
                        return f[1]==vscode.FileType.File && f[0].slice(-5) === ".chor"});
        if (chorEntry == undefined) {
            return undefined;
        }

        return new ProjectDirectory(directory, chorEntry[0]);
    }
}

let projectDirectory : ProjectDirectory | undefined = undefined;
export async function getProjectDirectory():Promise<ProjectDirectory| undefined> {
    return projectDirectory;
}

export function getWorkspaceRoot():vscode.Uri {
    return vscode.workspace.workspaceFolders![0]!.uri
}

export async function checkForProjects(context: vscode.ExtensionContext): Promise<vscode.Uri[]> {

    let choreoDir = vscode.Uri.joinPath(getWorkspaceRoot(), "src","main","deploy","choreo");
    let possibleProject = ProjectDirectory.identifyProjectDirectory(choreoDir);
    if (possibleProject !== undefined) {
        projectDirectory = possibleProject;
    }

    //return (await vscode.workspace.findFiles("/src/main/deploy/choreo/*.chor"));
}



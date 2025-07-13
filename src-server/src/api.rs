// #![allow(clippy::needless_pass_by_value)]

// use std::path::PathBuf;

// use crate::tauri::TauriResult;
// use choreo_core::{
//     file_management::{self, create_diagnostic_file, get_log_lines, WritingResources},
//     generation::remote::RemoteGenerationResources,
//     spec::{
//         project::{ProjectFile, RobotConfig},
//         trajectory::TrajectoryFile,
//         Expr, OpenFilePayload,
//     },
//     ChoreoError, ChoreoResult,
// };
// use tauri::Manager;
// use tauri_plugin_dialog::{DialogExt, FilePath};

// macro_rules! debug_result (
//     ($result:expr) => {
//         #[cfg(debug_assertions)]
//         {
//             match $result {
//                 Ok(val) => {
//                     Ok(val)
//                 }
//                 Err(e) => {
//                     tracing::debug!("{e}");
//                     Err(e.into())
//                 }
//             }
//         }
//         #[cfg(not(debug_assertions))]
//         {
//             $result.map_err(Into::into)
//         }
//     };
// );
fn debug_result<T, E: Display>(result: Result<T, E>) -> Result<T, E> {
    #[cfg(debug_assertions)]
    {
        match result {
            Ok(val) => {
                return Ok(val);
            }
            Err(e) => {
                tracing::debug!("{e}");
                return Err(e.into());
            }
        }
    }
    #[cfg(not(debug_assertions))]
    {
        return result.map_err(Into::into);
    }
}
#[derive(Deserialize)]
struct GuessControlIntervalCountsBody {
    config: RobotConfig<Expr>,
    trajectory: TrajectoryFile,
}
#[post("/guess_control_interval_counts")]
async fn guess_control_interval_counts(
    json: web::Json<GuessControlIntervalCountsBody>,
) -> ChoreoResponse<Vec<usize>> {
    result_to_response(
        choreo_core::generation::intervals::guess_control_interval_counts(
            &json.config.snapshot(),
            &json.trajectory.params.snapshot(),
        ),
    )
}

#[get("/default_project")]
async fn default_project() -> impl Responder {
    web::Json::<ProjectFile>(ProjectFile::default())
}

// #[tauri::command]
// pub async fn open_in_explorer(path: String) -> TauriResult<()> {
//     debug_result!(open::that(path).map_err(ChoreoError::from));
// }

// #[tauri::command]
// pub async fn open_project_dialog(app_handle: tauri::AppHandle) -> TauriResult<OpenFilePayload> {
//     app_handle
//         .dialog()
//         .file()
//         .set_title("Open a .chor file")
//         .add_filter("Choreo Save File", &["chor"])
//         .blocking_pick_file()
//         .ok_or(ChoreoError::FileNotFound(None))
//         .map(|file| match file {
//             FilePath::Url(_) => Ok(OpenFilePayload {
//                 dir: ChoreoError::FileNotFound(None).to_string(),
//                 name: ChoreoError::FileNotFound(None).to_string(),
//             }),
//             FilePath::Path(path) => Ok(OpenFilePayload {
//                 dir: path
//                     .parent()
//                     .ok_or(ChoreoError::FileNotFound(None))?
//                     .to_str()
//                     .ok_or(ChoreoError::FileNotFound(None))?
//                     .to_string(),
//                 name: path
//                     .file_name()
//                     .ok_or(ChoreoError::FileNotFound(None))?
//                     .to_str()
//                     .ok_or(ChoreoError::FileNotFound(None))?
//                     .to_string(),
//             }),
//         })?
// }

// #[tauri::command]
// pub async fn default_project() -> TauriResult<ProjectFile> {
//     Ok(ProjectFile::default())
// }

// #[tauri::command]
// pub async fn read_project(app_handle: tauri::AppHandle, name: String) -> TauriResult<ProjectFile> {
//     let resources = app_handle.state::<WritingResources>();
//     debug_result!(file_management::read_projectfile(&resources, name).await);
// }

// #[tauri::command]
// pub async fn write_project(app_handle: tauri::AppHandle, project: ProjectFile) -> ChoreoResult<()> {
//     let resources = app_handle.state::<WritingResources>();
//     file_management::write_projectfile(&resources, project).await
// }

// #[tauri::command]
// pub async fn read_trajectory(
//     app_handle: tauri::AppHandle,
//     name: String,
// ) -> TauriResult<TrajectoryFile> {
//     let resources = app_handle.state::<WritingResources>();
//     debug_result!(file_management::read_trajectory_file(&resources, name).await);
// }

// #[tauri::command]
// pub async fn read_all_trajectory(app_handle: tauri::AppHandle) -> Vec<TrajectoryFile> {
//     let resources = app_handle.state::<WritingResources>();
//     let trajectories = file_management::find_all_trajectories(&resources).await;
//     let mut out = vec![];
//     for trajectory_name in trajectories {
//         let trajectory_res =
//             file_management::read_trajectory_file(&resources, trajectory_name).await;
//         match trajectory_res {
//             Ok(trajectory) => out.push(trajectory),
//             Err(e) => tracing::error!("{e}"),
//         }
//     }
//     out
// }

// #[tauri::command]
// pub async fn write_trajectory(
//     app_handle: tauri::AppHandle,
//     trajectory: TrajectoryFile,
// ) -> ChoreoResult<()> {
//     let resources = app_handle.state::<WritingResources>();
//     file_management::write_trajectory_file(&resources, trajectory).await
// }

// #[tauri::command]
// pub async fn rename_trajectory(
//     app_handle: tauri::AppHandle,
//     old_trajectory: TrajectoryFile,
//     new_name: String,
// ) -> TauriResult<()> {
//     let resources = app_handle.state::<WritingResources>();
//     debug_result!(
//         file_management::rename_trajectory_file(&resources, old_trajectory, new_name)
//             .await
//             .map(|_| ())
//     );
// }

// #[tauri::command]
// #[delete("/trajectory")]
// pub async fn delete_trajectory(
//     app_handle: tauri::AppHandle,
//     trajectory: TrajectoryFile,
// ) -> TauriResult<()> {
//     let resources = app_handle.state::<WritingResources>();
//     debug_result!(file_management::delete_trajectory_file(&resources, trajectory).await);
// }

// #[tauri::command]
// pub async fn trajectory_up_to_date(trajectory: TrajectoryFile) -> bool {
//     trajectory.up_to_date()
// }

// #[tauri::command]
#[derive(Deserialize)]
struct SetDeployRootBody {
    dir: String,
}
#[post("/deploy_root")]
pub async fn set_deploy_root(body: web::Json<SetDeployRootBody>) -> HttpResponse {
    HttpResponse::Ok()
        .cookie(
            Cookie::build("choreo_working_directory", body.dir.clone())
                //.domain("localhost:1420")
                .path("/")
                .secure(true)
                .same_site(SameSite::None)
                .permanent()
                .http_only(false)
                .finish(),
        )
        .finish()
    // let resources = app_handle.state::<WritingResources>();
    // file_management::set_deploy_path(&resources, PathBuf::from(dir)).await;
}

#[get("/deploy_root")]
pub async fn get_deploy_root(resources: web::Data<WritingResources>) -> ChoreoResponse<String> {
    let result = match resources.get_deploy_path().await {
        Ok(path) => Ok(path.to_string_lossy().to_string()),
        // an absent deploy path is represented as an empty string in the frontend
        Err(ChoreoError::NoDeployPath) => Ok(String::new()),
        Err(e) => {
            tracing::error!("{e}");
            Err(e)
        }
    };
    result_to_response(result)
}

use std::{fmt::Display, sync::Arc};

use actix_web::{
    cookie::{Cookie, SameSite},
    delete, get, post, web, Either, HttpRequest, HttpResponse, Responder,
};
use choreo_core::{
    file_management::WritingResources,
    generation::{
        generate::{generate, HandledLocalProgressUpdate, LocalProgressUpdate},
        remote::RemoteGenerationResources,
    },
    spec::{
        project::{ProjectFile, RobotConfig},
        trajectory::TrajectoryFile,
        Expr,
    },
    tokio::{
        self,
        sync::mpsc::{self, Sender},
    },
    ChoreoError, ChoreoResult,
};
use serde::Deserialize;

use crate::broadcast::SseBroadcaster;
type ChoreoResponse<T> = Either<web::Json<T>, HttpResponse>;
fn result_to_response<T>(result: ChoreoResult<T>) -> ChoreoResponse<T> {
    match result {
        Ok(value) => Either::Left(web::Json(value)),
        Err(message) => {
            Either::Right(HttpResponse::InternalServerError().body(message.to_string()))
        }
    }
}

#[get("/events")]
pub async fn event_stream(broadcaster: web::Data<SseBroadcaster>) -> impl Responder {
    broadcaster.new_client().await
}
#[derive(Deserialize)]
struct GenerateBody {
    project: ProjectFile,
    trajectory: TrajectoryFile,
    handle: i64,
}
#[post("/generate_remote")]
pub async fn generate_remote(
    req: HttpRequest,
    resources: web::Data<RemoteGenerationResources>,
    frontend_sender: web::Data<Sender<HandledLocalProgressUpdate>>,
    body: web::Json<GenerateBody>,
) -> ChoreoResponse<TrajectoryFile> {
    println!("{:?}", req.cookie("choreo_working_directory"));
    let remote_resources = resources;
    let tx = frontend_sender.get_ref().clone();
    use choreo_core::generation::remote::remote_generate_parent;

    let result = remote_generate_parent(
        &remote_resources,
        &body.project,
        &body.trajectory,
        body.handle,
        tx,
    )
    .await;
    result_to_response(debug_result(result))
}

static WORKING_DIRECTORY_TOKEN: &str = "choreo_working_directory";
fn working_directory(req: HttpRequest) -> Option<String> {
    req.cookie(WORKING_DIRECTORY_TOKEN)
        .map(|c| c.name_value().1.to_owned())
}

#[derive(Deserialize)]
struct CancelRemoteGeneratorBody {
    handle: i64,
}
#[post("/cancel_remote_generator")]
pub async fn cancel_remote_generator(
    resources: web::Data<RemoteGenerationResources>,
    body: web::Json<CancelRemoteGeneratorBody>,
) -> ChoreoResponse<()> {
    result_to_response(debug_result(resources.kill(body.handle)))
}

#[post("/cancel_all_remote_generators")]
pub async fn cancel_all_remote_generators(
    resources: web::Data<RemoteGenerationResources>,
) -> ChoreoResponse<()> {
    resources.kill_all();
    result_to_response(Ok(()))
}

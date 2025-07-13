//! Choreo (_Constraint-Honoring Omnidirectional Route Editor and Optimizer_,
//! pronounced like choreography) is a graphical tool for planning
//! time-optimized trajectories for autonomous mobile robots in the FIRST
//! Robotics Competition.

#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod api;
mod broadcast;
mod built;
mod logging;

use std::{fs, result, sync::Arc};

use broadcast::SseBroadcaster;
use choreo_core::{
    file_management::WritingResources,
    generation::{
        generate::{setup_progress_sender, HandledLocalProgressUpdate},
        remote::{remote_generate_child, RemoteArgs, RemoteGenerationResources},
    },
    spec::{
        project::{ProjectFile, RobotConfig},
        trajectory::TrajectoryFile,
        Expr,
    },
    tokio::{self, sync::mpsc},
};

use actix_cors::Cors;
use actix_web::{
    get, middleware::Logger, post, web, App, Either, HttpResponse, HttpServer, Responder,
};
use serde::Deserialize;

#[actix_web::main]
async fn main() -> std::io::Result<()> {
    let args = std::env::args().collect::<Vec<_>>();
    if args.len() > 2 {
        panic!("Unsupported arguments: {:?}", args);
    }

    if let Some(arg) = args.get(1) {
        if let Ok(remote_args) = RemoteArgs::from_content(arg) {
            tracing_subscriber::fmt()
                .with_max_level(tracing::Level::ERROR)
                .event_format(logging::CompactFormatter { ansicolor: false })
                .init();
            remote_generate_child(remote_args);
            return Ok(());
        }
    }
    env_logger::init_from_env(env_logger::Env::new().default_filter_or("debug"));

    let event_broadcaster = SseBroadcaster::create();
    let broadcaster = event_broadcaster.clone();
    let (tx, mut rx) = mpsc::channel::<HandledLocalProgressUpdate>(50);
    let arc_tx = Arc::new(tx);
    tokio::spawn(async move {
        // TODO shueja: use recv_many? impact unknown 6/29/25
        // ends when all senders are dropped, including the one within SseBroadcaster
        while let Some(update) = rx.recv().await {
            if let Ok(string) = update.update.contents_json() {
                broadcaster
                    .broadcast(
                        string.as_str(),
                        update.update.sse_event_string(),
                        format!("{}", update.handle).as_str(),
                    )
                    .await;
            }
        }
    });
    HttpServer::new(move || {
        let cors = Cors::permissive().supports_credentials(); // TODO set something sensible

        App::new()
            .wrap(cors)
            .wrap(Logger::default())
            .app_data(web::Data::new(RemoteGenerationResources::new()))
            .app_data(web::Data::new(WritingResources::new()))
            .app_data(web::Data::from(Arc::clone(&arc_tx)))
            .app_data(web::Data::from(Arc::clone(&event_broadcaster)))
            .service(api::default_project)
            .service(api::guess_control_interval_counts)
            .service(api::generate_remote)
            .service(api::get_deploy_root)
            .service(api::set_deploy_root)
            .service(api::event_stream)
    })
    .bind(("127.0.0.1", 8080))?
    .run()
    .await
}

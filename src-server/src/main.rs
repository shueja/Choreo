//! Choreo (_Constraint-Honoring Omnidirectional Route Editor and Optimizer_,
//! pronounced like choreography) is a graphical tool for planning
//! time-optimized trajectories for autonomous mobile robots in the FIRST
//! Robotics Competition.

#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod api;
mod built;
mod logging;

use std::{fs, result};

use choreo_core::{file_management::WritingResources, generation::remote::{remote_generate_child, RemoteArgs, RemoteGenerationResources}, spec::{project::{ProjectFile, RobotConfig}, trajectory::TrajectoryFile, Expr}};

use actix_web::{get, middleware::Logger, post, web, App, Either, HttpResponse, HttpServer, Responder};
use actix_cors::Cors;
use serde::Deserialize;

#[actix_web::main]
async fn main() -> std::io::Result<()> {
    env_logger::init_from_env(env_logger::Env::new().default_filter_or("info"));
    HttpServer::new(|| {
        let cors = Cors::permissive(); // TODO set something sensible
        App::new()
            .wrap(cors)
            .wrap(Logger::default())
            .app_data(web::Data::new(RemoteGenerationResources::new()))
            .app_data(web::Data::new(WritingResources::new()))
            .service(api::default_project)
            .service(api::guess_control_interval_counts)
            .service(api::generate_remote)
    })
    .bind(("127.0.0.1", 8080))?
    .run()
    .await
}

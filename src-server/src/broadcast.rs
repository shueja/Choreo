use std::{sync::Arc, time::Duration};

use actix_web::rt::time::interval;
use actix_web_lab::{
    sse::{self, Sse},
    util::InfallibleStream,
};
use futures_util::future;
use tokio::sync::Mutex;
use choreo_core::{generation::generate::HandledLocalProgressUpdate, tokio::{self, sync::mpsc}};
use serde::Serialize;
use tokio_stream::wrappers::ReceiverStream;

pub struct SseBroadcaster {
    inner: Mutex<SseBroadcasterInner>,
}

#[derive(Debug, Clone, Default)]
struct SseBroadcasterInner {
    clients: Vec<mpsc::Sender<sse::Event>>,
}

impl SseBroadcaster {
    /// Constructs new broadcaster and spawns ping loop.
    pub fn create() -> Arc<Self> {
        let this = Arc::new(Self {
            inner: Mutex::new(SseBroadcasterInner::default()),
        });

        SseBroadcaster::spawn_ping(Arc::clone(&this));

        this
    }

    /// Pings clients every 10 seconds to see if they are alive and remove them from the broadcast
    /// list if not.
    fn spawn_ping(this: Arc<Self>) {
        println!("STARTING PING LOOP");
        actix_web::rt::spawn(async move {
            let mut interval = interval(Duration::from_secs(10));

            loop {
                interval.tick().await;
                this.remove_stale_clients().await;
            }
        });
    }

    /// Removes all non-responsive clients from broadcast list.
    async fn remove_stale_clients(&self) {
        
        let clients = self.inner.lock().await.clients.clone();
        
        let mut ok_clients = Vec::new();

        for client in clients {
            if client
                .send(sse::Event::Comment("ping".into()))
                .await
                .is_ok()
            {
                ok_clients.push(client.clone());
            }
        }
        println!("Removing stale clients, remaining: {}", ok_clients.len());
        self.inner.lock().clients = ok_clients;
        
    }

    /// Registers client with broadcaster, returning an SSE response body.
    pub async fn new_client(&self) -> Sse<InfallibleStream<ReceiverStream<sse::Event>>> {
        println!("New connection to event stream");
        let (tx, rx) = mpsc::channel(10);

        tx.send(sse::Data::new("connected").into()).await.unwrap();
        println!("connection message sent");
        self.inner.lock().await.clients.push(tx);
        println!("connection added to clients vec");
        Sse::from_infallible_receiver(rx)
    }

    /// Broadcasts `msg` to all clients.
    pub async fn broadcast(&self, msg: &str, event: &str, id: &str) {
        let clients = self.inner.lock().await.clients.clone();
        
        let send_futures = clients
            .iter()
            .map(|client| client.send(sse::Data::new(msg).event(event).id(id).into()));

        // try to send to all clients, ignoring failures
        // disconnected clients will get swept up by `remove_stale_clients`
        let _ = future::join_all(send_futures).await;
    }
}
import type {
  ApiErrorPayload,
  EventMarkerPatch,
  GeometryDragAnalysis,
  ServerEvent,
  ServerSessionState,
  SessionStatus,
  SessionSnapshot
} from "@choreo/api";
import type {
  ProjectDocument,
  ServerGeometryType,
  ServerVariable,
  TrajectoryDocument,
  ServerWaypoint as Waypoint
} from "@choreo/document";

export class ApiError extends Error {
  constructor(
    readonly status: number,
    readonly payload: ApiErrorPayload
  ) {
    super(payload.message);
    this.name = "ApiError";
  }
}

const defaultApiOrigin = (): string => {
  const configuredOrigin = import.meta.env.VITE_CHOREO_API_ORIGIN;
  return configuredOrigin || window.location.origin;
};

async function requestJson<T>(path: string, init?: RequestInit): Promise<T> {
  const response = await fetch(new URL(path, defaultApiOrigin()), init);
  const payload = (await response.json().catch(() => null)) as
    | ApiErrorPayload
    | T
    | null;

  if (!response.ok) {
    const errorPayload =
      payload && typeof payload === "object" && "message" in payload
        ? (payload as ApiErrorPayload)
        : { code: "HTTP_ERROR", message: response.statusText };
    throw new ApiError(response.status, errorPayload);
  }

  return payload as T;
}

function webSocketUrl(): string {
  const url = new URL("/api/v1/events", defaultApiOrigin());
  url.protocol = url.protocol === "https:" ? "wss:" : "ws:";
  return url.toString();
}

export class ServerSessionStore {
  private state: ServerSessionState = {
    status: null,
    snapshot: null,
    connected: false,
    loading: false,
    error: null,
    lastEvent: null
  };
  private listeners = new Set<() => void>();
  private socket: WebSocket | null = null;
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
  private reconnectDelay = 500;
  private started = false;
  private refreshPromise: Promise<void> | null = null;
  private requestedProjectId: string | null = null;
  private requestedRevision: number | null = null;

  readonly subscribe = (listener: () => void): (() => void) => {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  };

  readonly getSnapshot = (): ServerSessionState => this.state;

  start(): void {
    if (this.started) {
      return;
    }
    this.started = true;
    this.connect();
  }

  stop(): void {
    this.started = false;
    if (this.reconnectTimer !== null) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
    this.socket?.close();
    this.socket = null;
  }

  async openProject(): Promise<string> {
    const accepted = await requestJson<{ operationId: string }>(
      "/api/v1/session/open",
      {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ source: "picker" })
      }
    );
    return accepted.operationId;
  }

  async createNewProject(project: ProjectDocument): Promise<string> {
    const accepted = await requestJson<{ operationId: string }>(
      "/api/v1/session/new",
      {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
          "Idempotency-Key": crypto.randomUUID()
        },
        body: JSON.stringify({ project })
      }
    );
    return accepted.operationId;
  }

  async save(): Promise<string> {
    const accepted = await requestJson<{ operationId: string }>(
      "/api/v1/session/save",
      { method: "POST" }
    );
    return accepted.operationId;
  }

  async getGeometryDragControls(
    projectId: string,
    expression: string,
    type: ServerGeometryType
  ): Promise<GeometryDragAnalysis> {
    const result = await requestJson<GeometryDragAnalysis>(
      `/api/v1/projects/${encodeURIComponent(projectId)}/geometry-controls`,
      {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ expression, type })
      }
    );
    return result;
  }

  async undo(): Promise<void> {
    await this.mutate("/api/v1/session/undo", "POST");
  }

  async redo(): Promise<void> {
    await this.mutate("/api/v1/session/redo", "POST");
  }

  async mutate<T = unknown>(
    path: string,
    method: "POST" | "PATCH" | "DELETE",
    body?: unknown,
    contentType = "application/json"
  ): Promise<T> {
    const revision = this.state.snapshot?.revision;
    if (revision === undefined) {
      throw new Error("Open a project before editing it.");
    }

    try {
      const result = await requestJson<T>(path, {
        method,
        headers: {
          "If-Match": `"revision-${revision}"`,
          "Idempotency-Key": crypto.randomUUID(),
          ...(body === undefined ? {} : { "Content-Type": contentType })
        },
        ...(body === undefined ? {} : { body: JSON.stringify(body) })
      });
      await this.refresh();
      return result;
    } catch (error) {
      if (error instanceof ApiError && error.status === 409) {
        await this.refresh();
      }
      throw error;
    }
  }

  private connect(): void {
    if (!this.started) {
      return;
    }

    const socket = new WebSocket(webSocketUrl());
    this.socket = socket;
    socket.onopen = () => {
      this.reconnectDelay = 500;
      this.update({ connected: true, error: null });
      void this.refresh();
    };
    socket.onmessage = (message) => {
      this.receiveEvent(message.data);
    };
    socket.onerror = () => socket.close();
    socket.onclose = () => {
      if (this.socket === socket) {
        this.socket = null;
      }
      this.update({ connected: false });
      if (this.started && this.reconnectTimer === null) {
        this.reconnectTimer = setTimeout(() => {
          this.reconnectTimer = null;
          this.connect();
        }, this.reconnectDelay);
        this.reconnectDelay = Math.min(this.reconnectDelay * 2, 8000);
      }
    };
  }

  private receiveEvent(data: string): void {
    let event: ServerEvent;
    try {
      event = JSON.parse(data) as ServerEvent;
    } catch {
      return;
    }
    this.update({ lastEvent: event });

    if (event.type === "session.persistence") {
      this.update({
        error: event.success
          ? null
          : (event.error?.message ?? "Automatic save failed.")
      });
      void this.refreshStatus();
      return;
    }

    if (event.type === "session.operation" && event.status === "failed") {
      this.update({
        error:
          event.error?.message ?? event.message ?? "Session operation failed."
      });
    }

    if (event.type === "session.closed") {
      this.requestedProjectId = null;
      this.requestedRevision = null;
      this.update({ snapshot: null, status: null });
      void this.refreshStatus();
      return;
    }

    if (event.type === "document.changed") {
      if (this.requestedProjectId !== event.projectId) {
        this.requestedProjectId = event.projectId;
        this.requestedRevision = event.revision;
      } else {
        this.requestedRevision = Math.max(
          this.requestedRevision ?? event.revision,
          event.revision
        );
      }
      void this.refresh();
      return;
    }

    if (
      (event.type === "session.operation" ||
        event.type === "generation.finished") &&
      typeof event.revision === "number"
    ) {
      const projectId = this.state.status?.projectId;
      if (projectId !== null && projectId !== undefined) {
        if (this.requestedProjectId !== projectId) {
          this.requestedProjectId = projectId;
          this.requestedRevision = event.revision;
        } else {
          this.requestedRevision = Math.max(
            this.requestedRevision ?? event.revision,
            event.revision
          );
        }
      }
      void this.refresh();
    }
  }

  private async refreshStatus(): Promise<void> {
    try {
      const status = await requestJson<SessionStatus>("/api/v1/session/status");
      this.update({ status });
    } catch (error) {
      this.update({ error: this.errorMessage(error) });
    }
  }

  private refresh(): Promise<void> {
    if (this.refreshPromise !== null) {
      return this.refreshPromise;
    }

    this.update({ loading: true, error: null });
    this.refreshPromise = this.refreshUntilCurrent().finally(() => {
      this.refreshPromise = null;
      this.update({ loading: false });
    });
    return this.refreshPromise;
  }

  private async refreshUntilCurrent(): Promise<void> {
    try {
      do {
        const status = await requestJson<SessionStatus>(
          "/api/v1/session/status"
        );
        if (!status.open) {
          this.requestedProjectId = null;
          this.requestedRevision = null;
          this.update({ status, snapshot: null });
          return;
        }

        const snapshot = await requestJson<SessionSnapshot>("/api/v1/snapshot");
        if (this.requestedProjectId !== snapshot.project.uuid) {
          this.requestedProjectId = snapshot.project.uuid;
          this.requestedRevision = snapshot.revision;
        }
        if (
          this.state.snapshot === null ||
          snapshot.project.uuid !== this.state.snapshot.project.uuid ||
          snapshot.revision >= this.state.snapshot.revision
        ) {
          this.update({ status, snapshot });
        }
      } while (
        this.requestedProjectId === this.state.snapshot?.project.uuid &&
        this.requestedRevision !== null &&
        this.state.snapshot.revision < this.requestedRevision
      );
    } catch (error) {
      this.update({ error: this.errorMessage(error) });
    }
  }

  private errorMessage(error: unknown): string {
    return error instanceof Error ? error.message : String(error);
  }

  private update(patch: Partial<ServerSessionState>): void {
    this.state = { ...this.state, ...patch };
    this.listeners.forEach((listener) => listener());
  }
}

export const serverSession = new ServerSessionStore();

import type {
  ProjectDocument,
  ServerEventMarkerDocument,
  TrajectoryDocument
} from "@choreo/document";

export interface ApiErrorPayload {
  code: string;
  message: string;
  requestId?: string;
  currentRevision?: number;
  details?: { path: string; message: string }[];
}

export interface SessionStatus {
  open: boolean;
  projectId: string | null;
  revision: number | null;
  dirty: boolean;
  persistenceBlocked: boolean;
}

export interface GeometryDragControl {
  start: number;
  end: number;
  value: number;
  dx: number;
  dy: number;
  dheading: number;
  segmentDx: number;
  segmentDy: number;
  segmentDheading: number;
  chainIndex: number;
  anchorX: number;
  anchorY: number;
  anchorHeading: number;
  rigidMotion: boolean;
  motionDx: number;
  motionDy: number;
  motionDheading: number;
  pivotX: number;
  pivotY: number;
  effects: GeometryDragEffect[];
}

export interface GeometryDragEffect {
  pathIndex: number;
  pointIndex: number;
  dx: number;
  dy: number;
}

export interface GeometryFeaturePoint {
  x: number;
  y: number;
}

export interface GeometryChainLink {
  startX: number;
  startY: number;
  startHeading: number;
  endX: number;
  endY: number;
  endHeading: number;
}

export interface GeometryDragAnalysis {
  controls: GeometryDragControl[];
  chain: GeometryChainLink[];
  paths: GeometryFeaturePoint[][];
}

export type EventMarkerPatch = {
  name?: string;
  from?: Partial<ServerEventMarkerDocument["from"]>;
};

export interface SessionSnapshot {
  revision: number;
  project: ProjectDocument;
  trajectories: TrajectoryDocument[];
}

export type ServerEvent =
  | { type: "document.changed"; projectId: string; revision: number }
  | { type: "session.closed"; projectId: string; revision: number }
  | {
      type: "session.persistence";
      projectId: string;
      revision: number;
      success: boolean;
      error?: ApiErrorPayload;
    }
  | {
      type: "session.operation";
      operationId: string;
      operation: string;
      status: string;
      message?: string;
      revision?: number;
      error?: ApiErrorPayload;
    }
  | {
      type: "generation.progress";
      operationId: string;
      trajectoryId: string;
      progress?: number;
      message?: string;
    }
  | {
      type: "generation.status";
      operationId: string;
      trajectoryId: string;
      status: string;
      message?: string;
      revision?: number;
    }
  | {
      type: "generation.finished";
      operationId: string;
      trajectoryId: string;
      status: string;
      revision?: number;
      error?: ApiErrorPayload;
    };

export interface ServerSessionState {
  status: SessionStatus | null;
  snapshot: SessionSnapshot | null;
  connected: boolean;
  loading: boolean;
  error: string | null;
  lastEvent: ServerEvent | null;
}

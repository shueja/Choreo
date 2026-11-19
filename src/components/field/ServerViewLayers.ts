export interface ServerViewLayers {
  field: boolean;
  grid: boolean;
  trajectory: boolean;
  samples: boolean;
  waypoints: boolean;
}

export type ServerWaypointType = "full" | "translation" | "heading" | "empty";
export type ServerConstraintType =
  | "MaxLinearVelocity"
  | "MaxAngularVelocity"
  | "KeepInCircle"
  | "Heading";

export const SERVER_WAYPOINT_TYPE_DEFAULT: ServerWaypointType = "full";

export const SERVER_VIEW_LAYER_DEFAULTS: ServerViewLayers = {
  field: true,
  grid: false,
  trajectory: true,
  samples: false,
  waypoints: true
};

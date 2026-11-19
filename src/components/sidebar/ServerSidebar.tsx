import {
  ArrowCircleDown,
  Add,
  CheckBox,
  CheckBoxOutlineBlank,
  Circle,
  CircleOutlined,
  CloudDone,
  CloudOff,
  ContentCopy,
  DeleteOutline,
  Menu as MenuIcon,
  NearMe,
  Room,
  Redo,
  Speed,
  Sync,
  Undo
} from "@mui/icons-material";
import { IconButton, Menu, MenuItem, Tooltip } from "@mui/material";
import { useState } from "react";
import WaypointIcon from "../../assets/Waypoint";
import type {
  ProjectDocument,
  TrajectoryDocument
} from "@choreo/document";
import styles from "./ServerSidebar.module.css";

export type ServerConstraintType =
  | "MaxLinearVelocity"
  | "MaxAngularVelocity"
  | "KeepInCircle"
  | "Heading";

const constraintOptions: { type: ServerConstraintType; label: string }[] = [
  { type: "MaxLinearVelocity", label: "Max Linear Velocity" },
  { type: "MaxAngularVelocity", label: "Max Angular Velocity" },
  { type: "KeepInCircle", label: "Keep In Circle" },
  { type: "Heading", label: "Heading" }
];

function constraintLabel(type: string): string {
  return type.replace(/([a-z])([A-Z])/g, "$1 $2");
}

function waypointType(
  waypoint: TrajectoryDocument["params"]["waypoints"][number]
): string {
  if (waypoint.fixTranslation && waypoint.fixHeading) {
    return "Full";
  }
  if (waypoint.fixTranslation) {
    return "Translation";
  }
  if (waypoint.fixHeading) {
    return "Heading";
  }
  return "Empty";
}

function waypointScopeLabel(
  id: { uuid: string } | "first" | "last",
  waypoints: TrajectoryDocument["params"]["waypoints"]
): string {
  if (id === "first") {
    return "1";
  }
  if (id === "last") {
    return String(waypoints.length);
  }
  const index = waypoints.findIndex((waypoint) => waypoint.uuid === id.uuid);
  return index < 0 ? "?" : String(index + 1);
}

function constraintIcon(type: string, color: string) {
  const props = { className: styles.featureIcon, htmlColor: color };
  switch (type) {
    case "MaxLinearVelocity":
      return <Speed {...props} />;
    case "MaxAngularVelocity":
      return <Sync {...props} />;
    case "KeepInCircle":
      return <ArrowCircleDown {...props} />;
    case "Heading":
      return <NearMe {...props} />;
    default:
      return <CheckBoxOutlineBlank {...props} />;
  }
}

interface Props {
  project: ProjectDocument;
  trajectories: TrajectoryDocument[];
  selectedTrajectoryId: string;
  connected: boolean;
  pending: boolean;
  dirty: boolean;
  persistenceBlocked: boolean;
  selectedWaypointId: string;
  selectedConstraintId: string;
  selectedEventMarkerId: string;
  onDuplicateTrajectory: (trajectory: TrajectoryDocument) => void;
  onSelectWaypoint: (waypointId: string) => void;
  onDeleteWaypoint: (waypointId: string) => void;
  onSelectConstraint: (constraintId: string) => void;
  onSelectEventMarker: (markerId: string) => void;
  onAddTrajectory: () => void;
  onAddConstraint: (type: ServerConstraintType) => void;
  onToggleConstraint: (constraintId: string, enabled: boolean) => void;
  onDeleteConstraint: (constraintId: string) => void;
  onAddEventMarker: () => void;
  onDeleteEventMarker: (markerId: string) => void;
  onToggleMainMenu: () => void;
  onUndo: () => void;
  onRedo: () => void;
}

export default function ServerSidebar({
  project,
  trajectories,
  selectedTrajectoryId,
  connected,
  pending,
  dirty,
  persistenceBlocked,
  selectedWaypointId,
  selectedConstraintId,
  selectedEventMarkerId,
  onDuplicateTrajectory,
  onSelectWaypoint,
  onDeleteWaypoint,
  onSelectConstraint,
  onSelectEventMarker,
  onAddTrajectory,
  onAddConstraint,
  onToggleConstraint,
  onDeleteConstraint,
  onAddEventMarker,
  onDeleteEventMarker,
  onToggleMainMenu,
  onUndo,
  onRedo
}: Props) {
  const [constraintMenuAnchor, setConstraintMenuAnchor] =
    useState<HTMLElement | null>(null);
  const activeTrajectory = trajectories.find(
    (trajectory) => trajectory.uuid === selectedTrajectoryId
  );

  return (
    <aside className={styles.sidebar}>
      <div className={styles.header}>
        <div className={styles.brandRow}>
          <Tooltip title="Main Menu">
            <IconButton
              aria-label="Main Menu"
              size="small"
              onClick={onToggleMainMenu}
            >
              <MenuIcon />
            </IconButton>
          </Tooltip>
          <strong>Choreo</strong>
        </div>
        <div className={styles.headerActions}>
          <Tooltip
            title={
              persistenceBlocked
                ? "Saving is blocked"
                : dirty
                  ? "Unsaved changes"
                  : "All changes saved"
            }
          >
            <span className={dirty ? styles.dirtyDot : styles.cleanDot} />
          </Tooltip>
          <Tooltip title={connected ? "Connected" : "Reconnecting"}>
            <span className={styles.connection}>
              {connected ? <CloudDone /> : <CloudOff />}
            </span>
          </Tooltip>
          <Tooltip title="Undo">
            <span>
              <IconButton
                aria-label="Undo"
                disabled={!connected || pending}
                onClick={onUndo}
              >
                <Undo />
              </IconButton>
            </span>
          </Tooltip>
          <Tooltip title="Redo">
            <span>
              <IconButton
                aria-label="Redo"
                disabled={!connected || pending}
                onClick={onRedo}
              >
                <Redo />
              </IconButton>
            </span>
          </Tooltip>
        </div>
      </div>

      <div className={styles.sectionHeading}>
        <span>PATHS</span>
        <Tooltip title="Duplicate trajectory">
          <span>
            <IconButton
              aria-label="Duplicate trajectory"
              size="small"
              disabled={!connected || pending || !activeTrajectory}
              onClick={() => {
                if (activeTrajectory) {
                  onDuplicateTrajectory(activeTrajectory);
                }
              }}
            >
              <ContentCopy fontSize="small" />
            </IconButton>
          </span>
        </Tooltip>
        <Tooltip title="Add trajectory">
          <span>
            <IconButton
              aria-label="Add trajectory"
              size="small"
              disabled={!connected || pending}
              onClick={onAddTrajectory}
            >
              <Add fontSize="small" />
            </IconButton>
          </span>
        </Tooltip>
      </div>
      <div className={styles.featuresHeading}>FEATURES</div>
      <div className={styles.featureList}>
        <div className={styles.featureSection}>
          <div className={styles.featureTitle}>WAYPOINTS</div>
          {activeTrajectory?.params.waypoints.map((waypoint, index) => (
            <div
              className={`${styles.featureItem} ${waypoint.uuid === selectedWaypointId ? styles.featureSelected : ""}`}
              key={waypoint.uuid}
              onClick={() => onSelectWaypoint(waypoint.uuid)}
            >
              {(() => {
                const Icon =
                  waypoint.fixTranslation && waypoint.fixHeading
                    ? WaypointIcon
                    : waypoint.fixTranslation
                      ? Circle
                      : waypoint.fixHeading
                        ? NearMe
                        : CircleOutlined;
                const color =
                  waypoint.uuid === selectedWaypointId
                    ? "var(--select-yellow)"
                    : index === 0
                      ? "green"
                      : index === activeTrajectory.params.waypoints.length - 1
                        ? "red"
                        : "var(--accent-purple)";
                return (
                  <Icon className={styles.featureIcon} htmlColor={color} />
                );
              })()}
              <span className={styles.featureLabel}>
                <span>{waypointType(waypoint)} Waypoint</span>
                <small>{index + 1}</small>
              </span>
              <Tooltip title="Delete waypoint">
                <IconButton
                  aria-label={`Delete waypoint ${index + 1}`}
                  size="small"
                  disabled={!connected || pending}
                  onClick={(event) => {
                    event.stopPropagation();
                    onDeleteWaypoint(waypoint.uuid);
                  }}
                >
                  <DeleteOutline fontSize="small" />
                </IconButton>
              </Tooltip>
            </div>
          ))}
          {!activeTrajectory?.params.waypoints.length && (
            <p className={styles.emptyHint}>No waypoints</p>
          )}
        </div>
        <div className={styles.featureSection}>
          <div className={styles.featureHeading}>
            <div className={styles.featureTitle}>CONSTRAINTS</div>
            <Tooltip title="Add constraint">
              <span>
                <IconButton
                  aria-label="Add constraint"
                  size="small"
                  disabled={
                    !connected ||
                    pending ||
                    !activeTrajectory?.params.waypoints.length
                  }
                  onClick={(event) =>
                    setConstraintMenuAnchor(event.currentTarget)
                  }
                >
                  <Add fontSize="small" />
                </IconButton>
              </span>
            </Tooltip>
            <Menu
              anchorEl={constraintMenuAnchor}
              open={constraintMenuAnchor !== null}
              onClose={() => setConstraintMenuAnchor(null)}
            >
              {constraintOptions.map((option) => (
                <MenuItem
                  key={option.type}
                  onClick={() => {
                    onAddConstraint(option.type);
                    setConstraintMenuAnchor(null);
                  }}
                >
                  {option.label}
                </MenuItem>
              ))}
            </Menu>
          </div>
          {activeTrajectory?.params.constraints.map((constraint) => (
            <div
              className={`${styles.featureItem} ${constraint.uuid === selectedConstraintId ? styles.featureSelected : ""}`}
              key={constraint.uuid}
              onClick={() => onSelectConstraint(constraint.uuid)}
            >
              {constraintIcon(
                constraint.data.type,
                constraint.uuid === selectedConstraintId
                  ? "var(--select-yellow)"
                  : constraint.enabled
                    ? "var(--accent-purple)"
                    : "gray"
              )}
              <span className={styles.featureLabel}>
                <span>{constraintLabel(constraint.data.type)}</span>
                <small>
                  {waypointScopeLabel(
                    constraint.from,
                    activeTrajectory.params.waypoints
                  )}
                  {constraint.to
                    ? `-${waypointScopeLabel(constraint.to, activeTrajectory.params.waypoints)}`
                    : ""}
                </small>
              </span>
              <span className={styles.featureActions}>
                <Tooltip
                  title={
                    constraint.enabled
                      ? "Disable constraint"
                      : "Enable constraint"
                  }
                >
                  <IconButton
                    aria-label={`${constraint.enabled ? "Disable" : "Enable"} ${constraint.data.type}`}
                    size="small"
                    onClick={() =>
                      onToggleConstraint(constraint.uuid, !constraint.enabled)
                    }
                  >
                    {constraint.enabled ? (
                      <CheckBox fontSize="small" />
                    ) : (
                      <CheckBoxOutlineBlank fontSize="small" />
                    )}
                  </IconButton>
                </Tooltip>
                <Tooltip title="Delete constraint">
                  <IconButton
                    aria-label={`Delete ${constraint.data.type}`}
                    size="small"
                    onClick={(event) => {
                      event.stopPropagation();
                      onDeleteConstraint(constraint.uuid);
                    }}
                  >
                    <DeleteOutline fontSize="small" />
                  </IconButton>
                </Tooltip>
              </span>
            </div>
          ))}
          {!activeTrajectory?.params.constraints.length && (
            <p className={styles.emptyHint}>No constraints</p>
          )}
        </div>
        <div className={styles.featureSection}>
          <div className={styles.featureHeading}>
            <div className={styles.featureTitle}>MARKERS</div>
            <Tooltip title="Add event marker">
              <span>
                <IconButton
                  aria-label="Add event marker"
                  size="small"
                  disabled={
                    !connected ||
                    pending ||
                    !activeTrajectory?.params.waypoints.length
                  }
                  onClick={onAddEventMarker}
                >
                  <Add fontSize="small" />
                </IconButton>
              </span>
            </Tooltip>
          </div>
          {activeTrajectory?.events.map((marker) => (
            <div
              className={`${styles.featureItem} ${marker.uuid === selectedEventMarkerId ? styles.featureSelected : ""}`}
              key={marker.uuid}
              onClick={() => onSelectEventMarker(marker.uuid)}
            >
              <Room
                className={styles.featureIcon}
                htmlColor={
                  marker.uuid === selectedEventMarkerId
                    ? "var(--select-yellow)"
                    : "var(--accent-purple)"
                }
              />
              <span className={styles.featureLabel}>
                <span>{marker.name}</span>
                <small>
                  {waypointScopeLabel(
                    marker.from.target,
                    activeTrajectory.params.waypoints
                  )}{" "}
                  ({marker.from.offset.val >= 0 ? "+" : ""}
                  {marker.from.offset.val.toFixed(2)} s)
                </small>
              </span>
              <Tooltip title="Delete event marker">
                <IconButton
                  aria-label={`Delete event marker ${marker.name}`}
                  size="small"
                  onClick={(event) => {
                    event.stopPropagation();
                    onDeleteEventMarker(marker.uuid);
                  }}
                >
                  <DeleteOutline fontSize="small" />
                </IconButton>
              </Tooltip>
            </div>
          ))}
          {!activeTrajectory?.events.length && (
            <p className={styles.emptyHint}>No event markers</p>
          )}
        </div>
      </div>
    </aside>
  );
}

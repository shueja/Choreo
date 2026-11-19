import { Add, CloudDone, CloudOff, FolderOpen } from "@mui/icons-material";
import { Alert, Button, CircularProgress } from "@mui/material";
import { useEffect, useRef, useState, useSyncExternalStore } from "react";
import ServerFieldCanvas from "./components/field/ServerFieldCanvas";
import ServerFieldControls from "./components/field/ServerFieldControls";
import ServerPathAnimationPanel from "./components/field/ServerPathAnimationPanel";
import ServerWaypointEditor from "./components/field/ServerWaypointEditor";
import ServerConstraintEditor from "./components/sidebar/ServerConstraintEditor";
import ServerEventMarkerEditor from "./components/sidebar/ServerEventMarkerEditor";
import ServerVariablesPanel from "./components/sidebar/ServerVariablesPanel";
import ServerAppMenu from "./ServerAppMenu";
import {
  SERVER_WAYPOINT_TYPE_DEFAULT,
  SERVER_VIEW_LAYER_DEFAULTS,
  ServerViewLayers,
  ServerWaypointType
} from "./components/field/ServerViewLayers";
import ServerNavbar from "./components/navbar/ServerNavbar";
import ServerSidebar, {
  ServerConstraintType
} from "./components/sidebar/ServerSidebar";
import {
  ConstraintDocument,
  EventMarkerDocument,
  EventMarkerPatch,
  ProjectDocument,
  ServerVariable,
  TrajectoryDocument,
  Waypoint,
  serverSession
} from "./document/ServerSessionStore";
import styles from "./ServerWorkspace.module.css";

const TRAJECTORY_SCHEMA_VERSION = 4;

function useServerSession() {
  const state = useSyncExternalStore(
    serverSession.subscribe,
    serverSession.getSnapshot,
    serverSession.getSnapshot
  );

  useEffect(() => {
    serverSession.start();
    return () => serverSession.stop();
  }, []);

  return state;
}

function trajectoryPath(projectId: string, trajectoryId: string): string {
  return `/api/v1/projects/${encodeURIComponent(projectId)}/trajectories/${encodeURIComponent(trajectoryId)}`;
}

function projectPath(projectId: string): string {
  return `/api/v1/projects/${encodeURIComponent(projectId)}`;
}

function poseExpression(x: number, y: number, heading: number): string {
  return `Pose2d(${x} m, ${y} m, ${heading} rad)`;
}

function createDefaultProject(): ProjectDocument {
  const expression = (exp: string, val: unknown) => ({ exp, val });
  const modules = [
    [-0.3, 0.3],
    [0.3, 0.3],
    [0.3, -0.3],
    [-0.3, -0.3]
  ];
  const bumperVertices = [
    [-0.45, -0.45],
    [0.45, -0.45],
    [0.45, 0.45],
    [-0.45, 0.45]
  ];

  return {
    uuid: crypto.randomUUID(),
    name: "Untitled",
    version: 4,
    type: "Swerve",
    variables: { variables: {}, order: [] },
    config: {
      mass: expression("55 kg", 55),
      inertia: expression("5 kg*m^2", 5),
      gearing: expression("6.75", 6.75),
      radius: expression("0.05 m", 0.05),
      cof: expression("1.2", 1.2),
      wheels: {
        swerve: {
          modules: modules.map(([x, y]) =>
            expression(`Translation2d(${x} m, ${y} m)`, { x, y })
          )
        }
      },
      bumpers: expression(
        `[${bumperVertices.map(([x, y]) => `Translation2d(${x} m, ${y} m)`).join(", ")}]`,
        { vertices: bumperVertices.map(([x, y]) => ({ x, y })) }
      ),
      motor: {
        freeSpeed: expression("6000 RPM", 628.3185307179587),
        stallTorque: expression("2.6 N * m", 2.6),
        kT: expression("0.019 N * m / A", 0.019),
        kV: expression("0.019 V * s / rad", 0.019),
        supplyLimit: expression("60 A", 60),
        statorLimit: expression("80 A", 80)
      }
    },
    codegen: {
      root: "",
      genVars: true,
      genTrajData: true,
      useChoreoLib: true
    }
  };
}

function ServerWorkspace() {
  const state = useServerSession();
  const [selectedTrajectoryId, setSelectedTrajectoryId] = useState("");
  const [selectedWaypointId, setSelectedWaypointId] = useState("");
  const [selectedConstraintId, setSelectedConstraintId] = useState("");
  const [selectedEventMarkerId, setSelectedEventMarkerId] = useState("");
  const [activeEditor, setActiveEditor] = useState<
    "waypoint" | "constraint" | "marker" | "none"
  >("waypoint");
  const [pending, setPending] = useState(false);
  const [mainMenuOpen, setMainMenuOpen] = useState(false);
  const [animationTimestamp, setAnimationTimestamp] = useState(0);
  const [placementType, setPlacementType] = useState<ServerWaypointType>(
    SERVER_WAYPOINT_TYPE_DEFAULT
  );
  const [constraintTool, setConstraintTool] =
    useState<ServerConstraintType | null>(null);
  const [visibleLayers, setVisibleLayers] = useState(
    SERVER_VIEW_LAYER_DEFAULTS
  );
  const [actionError, setActionError] = useState<string | null>(null);
  const [actionMessage, setActionMessage] = useState<string | null>(null);
  const defaultProjectRequested = useRef(false);
  const snapshot = state.snapshot;
  const project = snapshot?.project;
  const trajectories = snapshot?.trajectories ?? [];

  useEffect(() => {
    if (
      !state.connected ||
      !state.status ||
      state.status.open ||
      defaultProjectRequested.current
    ) {
      return;
    }
    defaultProjectRequested.current = true;
    setPending(true);
    setActionError(null);
    setActionMessage("Creating project");
    void serverSession
      .createNewProject(createDefaultProject())
      .catch((error: unknown) => {
        setActionError(error instanceof Error ? error.message : String(error));
        setActionMessage(null);
      })
      .finally(() => setPending(false));
  }, [state.connected, state.status]);

  useEffect(() => {
    if (
      !trajectories.some(
        (trajectory) => trajectory.uuid === selectedTrajectoryId
      )
    ) {
      setSelectedTrajectoryId(trajectories[0]?.uuid ?? "");
    }
  }, [selectedTrajectoryId, trajectories]);

  const activeTrajectory = trajectories.find(
    (trajectory) => trajectory.uuid === selectedTrajectoryId
  );
  const selectedWaypoint = activeTrajectory?.params.waypoints.find(
    (waypoint) => waypoint.uuid === selectedWaypointId
  );
  const selectedConstraint = activeTrajectory?.params.constraints.find(
    (constraint) => constraint.uuid === selectedConstraintId
  );
  const selectedEventMarker = activeTrajectory?.events.find(
    (marker) => marker.uuid === selectedEventMarkerId
  );
  const bumperVertices =
    (
      project?.config as
        | { bumpers?: { val?: { vertices?: { x: number; y: number }[] } } }
        | undefined
    )?.bumpers?.val?.vertices ?? [];
  const generatedSamples =
    activeTrajectory?.output?.trajectory?.samples?.map(
      (sample) => sample.pose.translation
    ) ?? [];
  const outputSamples = activeTrajectory?.output?.trajectory?.samples ?? [];
  const animatedSample = outputSamples.reduce<
    (typeof outputSamples)[number] | undefined
  >((closest, sample) => {
    if (!closest) {
      return sample;
    }
    return Math.abs(sample.time - animationTimestamp) <
      Math.abs(closest.time - animationTimestamp)
      ? sample
      : closest;
  }, undefined);

  useEffect(() => {
    setAnimationTimestamp(0);
  }, [selectedTrajectoryId]);

  useEffect(() => {
    if (
      !activeTrajectory?.params.waypoints.some(
        (waypoint) => waypoint.uuid === selectedWaypointId
      )
    ) {
      setSelectedWaypointId(activeTrajectory?.params.waypoints[0]?.uuid ?? "");
    }
  }, [activeTrajectory, selectedWaypointId]);

  useEffect(() => {
    if (
      !activeTrajectory?.params.constraints.some(
        (constraint) => constraint.uuid === selectedConstraintId
      )
    ) {
      setSelectedConstraintId("");
    }
    if (
      !activeTrajectory?.events.some(
        (marker) => marker.uuid === selectedEventMarkerId
      )
    ) {
      setSelectedEventMarkerId("");
    }
  }, [activeTrajectory, selectedConstraintId, selectedEventMarkerId]);

  function toggleLayer(layer: keyof ServerViewLayers) {
    setVisibleLayers((current) => ({ ...current, [layer]: !current[layer] }));
  }

  async function perform(action: () => Promise<unknown>, message: string) {
    if (pending) {
      return;
    }
    setPending(true);
    setActionError(null);
    setActionMessage(message);
    try {
      await action();
    } catch (error) {
      setActionError(error instanceof Error ? error.message : String(error));
      setActionMessage(null);
    } finally {
      setPending(false);
    }
  }

  async function addTrajectory() {
    if (!project || pending) {
      return;
    }
    const uuid = crypto.randomUUID();
    const path = `/api/v1/projects/${encodeURIComponent(project.uuid)}/trajectories`;
    await perform(async () => {
      await serverSession.mutate(path, "POST", {
        uuid,
        name: `Trajectory ${trajectories.length + 1}`,
        version: TRAJECTORY_SCHEMA_VERSION,
        config: null,
        snapshot: null,
        params: {
          waypoints: [],
          constraints: [],
          targetDt: { exp: "0.05 s", val: 0.05 }
        },
        output: null,
        events: []
      });
      setSelectedTrajectoryId(uuid);
    }, "Adding trajectory");
  }

  async function generateTrajectories(targets: TrajectoryDocument[]) {
    if (!project || targets.length === 0) {
      return;
    }
    await perform(
      async () => {
        for (const trajectory of targets) {
          await serverSession.mutate(
            `${trajectoryPath(project.uuid, trajectory.uuid)}/generations`,
            "POST",
            {}
          );
        }
      },
      targets.length === 1 ? "Generating trajectory" : "Generating trajectories"
    );
  }

  function generateAll() {
    void generateTrajectories(trajectories);
  }

  function generateAllOutdated() {
    const outdated = trajectories.filter(
      (trajectory) =>
        trajectory.output === null ||
        JSON.stringify(trajectory.snapshot) !==
          JSON.stringify(trajectory.params)
    );
    void generateTrajectories(outdated);
  }

  async function duplicateTrajectory(trajectory: TrajectoryDocument) {
    if (!project) {
      return;
    }
    const uuid = crypto.randomUUID();
    const waypointIds = new Map(
      trajectory.params.waypoints.map((waypoint) => [
        waypoint.uuid,
        crypto.randomUUID()
      ])
    );
    const remapWaypointId = (
      id: { uuid: string } | "first" | "last"
    ): { uuid: string } | "first" | "last" =>
      typeof id === "string"
        ? id
        : { uuid: waypointIds.get(id.uuid) ?? id.uuid };
    const duplicate: TrajectoryDocument = {
      ...trajectory,
      uuid,
      name: `${trajectory.name} Copy`,
      config: null,
      snapshot: null,
      output: null,
      params: {
        ...trajectory.params,
        waypoints: trajectory.params.waypoints.map((waypoint) => ({
          ...waypoint,
          uuid: waypointIds.get(waypoint.uuid)!,
          pose: { ...waypoint.pose }
        })),
        constraints: trajectory.params.constraints.map((constraint) => ({
          ...constraint,
          uuid: crypto.randomUUID(),
          from: remapWaypointId(constraint.from),
          ...(constraint.to ? { to: remapWaypointId(constraint.to) } : {})
        }))
      },
      events: trajectory.events.map((marker) => ({
        ...marker,
        uuid: crypto.randomUUID(),
        from: { ...marker.from, target: remapWaypointId(marker.from.target) }
      }))
    };
    await perform(async () => {
      await serverSession.mutate(
        `/api/v1/projects/${encodeURIComponent(project.uuid)}/trajectories`,
        "POST",
        duplicate
      );
      setSelectedTrajectoryId(uuid);
    }, "Duplicating trajectory");
  }

  async function addWaypoint(x: number, y: number) {
    if (!project || !activeTrajectory || pending) {
      return;
    }
    const waypoint: Waypoint = {
      uuid: crypto.randomUUID(),
      pose: {
        exp: poseExpression(x, y, 0),
        val: { translation: { x, y }, rotation: { radians: 0 } }
      },
      intervals: 20,
      split: false,
      fixTranslation:
        placementType === "full" || placementType === "translation",
      fixHeading: placementType === "full" || placementType === "heading",
      overrideIntervals: false
    };
    await perform(
      () =>
        serverSession.mutate(
          `${trajectoryPath(project.uuid, activeTrajectory.uuid)}/waypoints`,
          "POST",
          waypoint
        ),
      "Adding waypoint"
    );
    setSelectedWaypointId(waypoint.uuid);
    setActiveEditor("waypoint");
  }

  async function patchWaypoint(waypoint: Waypoint, pose: Waypoint["pose"]) {
    if (!project || !activeTrajectory) {
      return;
    }
    await perform(
      () =>
        serverSession.mutate(
          `${trajectoryPath(project.uuid, activeTrajectory.uuid)}/waypoints/${encodeURIComponent(waypoint.uuid)}`,
          "PATCH",
          { pose },
          "application/merge-patch+json"
        ),
      "Updating waypoint"
    );
  }

  async function patchWaypointOptions(
    waypoint: Waypoint,
    options: Partial<
      Pick<
        Waypoint,
        | "intervals"
        | "split"
        | "fixTranslation"
        | "fixHeading"
        | "overrideIntervals"
      >
    >
  ) {
    if (!project || !activeTrajectory) {
      return;
    }
    await perform(
      () =>
        serverSession.mutate(
          `${trajectoryPath(project.uuid, activeTrajectory.uuid)}/waypoints/${encodeURIComponent(waypoint.uuid)}`,
          "PATCH",
          options,
          "application/merge-patch+json"
        ),
      "Updating waypoint"
    );
  }

  async function deleteWaypoint(waypoint: Waypoint, index: number) {
    if (
      !project ||
      !activeTrajectory ||
      !window.confirm(`Delete waypoint ${index + 1}?`)
    ) {
      return;
    }
    await perform(
      () =>
        serverSession.mutate(
          `${trajectoryPath(project.uuid, activeTrajectory.uuid)}/waypoints/${encodeURIComponent(waypoint.uuid)}`,
          "DELETE"
        ),
      "Deleting waypoint"
    );
  }

  async function addConstraint(
    type: ServerConstraintType,
    startIndex?: number,
    endIndex?: number
  ) {
    if (!project || !activeTrajectory) {
      return;
    }
    const waypoints = activeTrajectory.params.waypoints;
    const fromIndex = startIndex ?? waypoints.length - 1;
    const fromWaypoint = waypoints[fromIndex];
    const toWaypoint = endIndex === undefined ? undefined : waypoints[endIndex];
    if (!fromWaypoint) {
      return;
    }
    const position = fromWaypoint.pose.val.translation;
    const data =
      type === "MaxLinearVelocity"
        ? { type, max: { exp: "2 m / s", val: 2 } }
        : type === "MaxAngularVelocity"
          ? { type, max: { exp: "2 rad / s", val: 2 } }
          : type === "Heading"
            ? {
                type,
                heading: {
                  exp: "Rotation2d(0 rad)",
                  val: { radians: 0 }
                },
                tolerance: { exp: "10 deg", val: Math.PI / 18 }
              }
            : {
                type,
                circle: {
                  exp: `Ellipse2d(Pose2d(${position.x} m, ${position.y} m, 0 rad), 1 m, 1 m)`,
                  val: {
                    center: {
                      translation: position,
                      rotation: { radians: 0 }
                    },
                    xSemiAxis: 1,
                    ySemiAxis: 1
                  }
                }
              };
    const constraint: ConstraintDocument = {
      uuid: crypto.randomUUID(),
      from: { uuid: fromWaypoint.uuid },
      ...(toWaypoint ? { to: { uuid: toWaypoint.uuid } } : {}),
      data,
      enabled: true
    };
    await perform(
      () =>
        serverSession.mutate(
          `${trajectoryPath(project.uuid, activeTrajectory.uuid)}/constraints`,
          "POST",
          constraint
        ),
      "Adding constraint"
    );
    setSelectedConstraintId(constraint.uuid);
    setActiveEditor("constraint");
    setConstraintTool(null);
  }

  async function toggleConstraint(constraintId: string, enabled: boolean) {
    if (!project || !activeTrajectory) {
      return;
    }
    await perform(
      () =>
        serverSession.mutate(
          `${trajectoryPath(project.uuid, activeTrajectory.uuid)}/constraints/${encodeURIComponent(constraintId)}`,
          "PATCH",
          { enabled },
          "application/merge-patch+json"
        ),
      enabled ? "Enabling constraint" : "Disabling constraint"
    );
  }

  async function deleteConstraint(constraintId: string) {
    if (!project || !activeTrajectory) {
      return;
    }
    await perform(
      () =>
        serverSession.mutate(
          `${trajectoryPath(project.uuid, activeTrajectory.uuid)}/constraints/${encodeURIComponent(constraintId)}`,
          "DELETE"
        ),
      "Deleting constraint"
    );
  }

  async function patchConstraint(
    constraint: ConstraintDocument,
    data: ConstraintDocument["data"]
  ) {
    if (!project || !activeTrajectory) {
      return;
    }
    await perform(
      () =>
        serverSession.mutate(
          `${trajectoryPath(project.uuid, activeTrajectory.uuid)}/constraints/${encodeURIComponent(constraint.uuid)}`,
          "PATCH",
          { data },
          "application/merge-patch+json"
        ),
      "Updating constraint"
    );
  }

  async function patchConstraintScope(
    constraint: ConstraintDocument,
    from: ConstraintDocument["from"],
    to: NonNullable<ConstraintDocument["to"]>
  ) {
    if (!project || !activeTrajectory) {
      return;
    }
    await perform(
      () =>
        serverSession.mutate(
          `${trajectoryPath(project.uuid, activeTrajectory.uuid)}/constraints/${encodeURIComponent(constraint.uuid)}`,
          "PATCH",
          { from, to },
          "application/merge-patch+json"
        ),
      "Updating constraint scope"
    );
  }

  async function addEventMarker() {
    if (!project || !activeTrajectory) {
      return;
    }
    const lastWaypoint = activeTrajectory.params.waypoints.at(-1);
    if (!lastWaypoint) {
      return;
    }
    const markerId = crypto.randomUUID();
    await perform(
      () =>
        serverSession.mutate(
          `${trajectoryPath(project.uuid, activeTrajectory.uuid)}/event-markers`,
          "POST",
          {
            uuid: markerId,
            name: `Marker ${activeTrajectory.events.length + 1}`,
            from: {
              target: { uuid: lastWaypoint.uuid },
              targetTime: null,
              offset: { exp: "0 s", val: 0 }
            }
          }
        ),
      "Adding event marker"
    );
    setSelectedEventMarkerId(markerId);
    setActiveEditor("marker");
  }

  async function deleteEventMarker(markerId: string) {
    if (!project || !activeTrajectory) {
      return;
    }
    await perform(
      () =>
        serverSession.mutate(
          `${trajectoryPath(project.uuid, activeTrajectory.uuid)}/event-markers/${encodeURIComponent(markerId)}`,
          "DELETE"
        ),
      "Deleting event marker"
    );
  }

  async function patchEventMarker(
    marker: EventMarkerDocument,
    patch: EventMarkerPatch
  ) {
    if (!project || !activeTrajectory) {
      return;
    }
    await perform(
      () =>
        serverSession.mutate(
          `${trajectoryPath(project.uuid, activeTrajectory.uuid)}/event-markers/${encodeURIComponent(marker.uuid)}`,
          "PATCH",
          patch,
          "application/merge-patch+json"
        ),
      "Updating event marker"
    );
  }

  async function renameTrajectory(
    trajectory: TrajectoryDocument,
    name: string
  ) {
    if (!project || name.trim() === trajectory.name) {
      return;
    }
    await perform(
      () =>
        serverSession.mutate(
          trajectoryPath(project.uuid, trajectory.uuid),
          "PATCH",
          { name: name.trim() },
          "application/merge-patch+json"
        ),
      "Renaming trajectory"
    );
  }

  async function patchVariable(
    id: string,
    patch: { name?: string; expression?: string }
  ) {
    if (!project) {
      return;
    }
    await perform(
      () =>
        serverSession.mutate(
          `${projectPath(project.uuid)}/variables/${encodeURIComponent(id)}`,
          "PATCH",
          patch,
          "application/merge-patch+json"
        ),
      "Updating variable"
    );
  }

  async function addVariable(variable: ServerVariable) {
    if (!project) {
      return;
    }
    const id = crypto.randomUUID();
    const order =
      project.variables.order ??
      Object.keys(project.variables.variables).sort();
    await perform(
      () =>
        serverSession.mutate(
          projectPath(project.uuid),
          "PATCH",
          {
            variables: {
              variables: { [id]: variable },
              order: [...order, id]
            }
          },
          "application/merge-patch+json"
        ),
      "Adding variable"
    );
  }

  async function deleteVariable(id: string) {
    if (!project) {
      return;
    }
    const order = (
      project.variables.order ?? Object.keys(project.variables.variables)
    ).filter((variableId) => variableId !== id);
    await perform(
      () =>
        serverSession.mutate(
          projectPath(project.uuid),
          "PATCH",
          { variables: { variables: { [id]: null }, order } },
          "application/merge-patch+json"
        ),
      "Deleting variable"
    );
  }

  async function reorderVariables(order: string[]) {
    if (!project) {
      return;
    }
    await perform(
      () =>
        serverSession.mutate(
          projectPath(project.uuid),
          "PATCH",
          { variables: { order } },
          "application/merge-patch+json"
        ),
      "Reordering variables"
    );
  }

  async function deleteTrajectory(trajectory: TrajectoryDocument) {
    if (!project || !window.confirm(`Delete ${trajectory.name}?`)) {
      return;
    }
    await perform(
      () =>
        serverSession.mutate(
          trajectoryPath(project.uuid, trajectory.uuid),
          "DELETE"
        ),
      "Deleting trajectory"
    );
  }

  async function generateTrajectory(trajectory: TrajectoryDocument) {
    if (!project) {
      return;
    }
    await perform(
      () =>
        serverSession.mutate(
          `${trajectoryPath(project.uuid, trajectory.uuid)}/generations`,
          "POST",
          {}
        ),
      "Starting generation"
    );
  }

  async function openProject() {
    await perform(() => serverSession.openProject(), "Opening project");
  }

  async function createProject() {
    if (
      state.status?.dirty &&
      !window.confirm("Create a new project and discard unsaved changes?")
    ) {
      return;
    }
    defaultProjectRequested.current = true;
    await perform(
      () => serverSession.createNewProject(createDefaultProject()),
      "Creating project"
    );
    setSelectedTrajectoryId("");
  }

  if (!project) {
    return (
      <main className={styles.workspace}>
        <header className={styles.topbar}>
          <div className={styles.brand}>Choreo</div>
          <span className={styles.connection}>
            {state.connected ? <CloudDone /> : <CloudOff />}
            {state.connected ? "Server connected" : "Connecting to server"}
          </span>
        </header>
        <section className={styles.openProject}>
          <div className={styles.openProjectContent}>
            <p className={styles.eyebrow}>PROJECT SESSION</p>
            <h1>New project</h1>
            <p className={styles.muted}>
              {state.connected
                ? "Preparing a new project."
                : "Waiting for the local Choreo server."}
            </p>
            <Button
              startIcon={<FolderOpen />}
              variant="contained"
              disabled={!state.connected || pending}
              onClick={() => void openProject()}
            >
              Open project
            </Button>
            {(actionError || state.error) && (
              <Alert severity="error">{actionError ?? state.error}</Alert>
            )}
            {pending && (
              <div className={styles.progressLine}>
                <CircularProgress size={16} /> {actionMessage}
              </div>
            )}
            {!state.connected && !state.error && (
              <p className={styles.muted}>
                Waiting for the local Choreo server.
              </p>
            )}
          </div>
        </section>
      </main>
    );
  }

  return (
    <main className={styles.workspace}>
      <ServerAppMenu
        open={mainMenuOpen}
        projectName={project.name}
        dirty={Boolean(state.status?.dirty)}
        onClose={() => setMainMenuOpen(false)}
        onOpenProject={() => void openProject()}
        onSaveProject={() =>
          void perform(() => serverSession.save(), "Saving project")
        }
        onNewProject={() => void createProject()}
      />
      <div className={styles.editor}>
        <ServerSidebar
          project={project}
          trajectories={trajectories}
          selectedTrajectoryId={selectedTrajectoryId}
          connected={state.connected}
          pending={pending}
          dirty={Boolean(state.status?.dirty)}
          persistenceBlocked={Boolean(state.status?.persistenceBlocked)}
          selectedWaypointId={selectedWaypointId}
          selectedConstraintId={selectedConstraintId}
          selectedEventMarkerId={selectedEventMarkerId}
          onToggleMainMenu={() => setMainMenuOpen(true)}
          onSelectTrajectory={setSelectedTrajectoryId}
          onRenameTrajectory={(trajectory, name) =>
            void renameTrajectory(trajectory, name)
          }
          onDeleteTrajectory={(trajectory) => void deleteTrajectory(trajectory)}
          onSelectWaypoint={(id) => {
            setSelectedWaypointId(id);
            setActiveEditor("waypoint");
          }}
          onDeleteWaypoint={(id) => {
            const index = activeTrajectory?.params.waypoints.findIndex(
              (waypoint) => waypoint.uuid === id
            );
            const waypoint = activeTrajectory?.params.waypoints.find(
              (item) => item.uuid === id
            );
            if (waypoint && index !== undefined && index >= 0) {
              void deleteWaypoint(waypoint, index);
            }
          }}
          onSelectConstraint={(id) => {
            setSelectedConstraintId(id);
            setActiveEditor("constraint");
          }}
          onSelectEventMarker={(id) => {
            setSelectedEventMarkerId(id);
            setActiveEditor("marker");
          }}
          onAddTrajectory={() => void addTrajectory()}
          onAddConstraint={(type) => void addConstraint(type)}
          onToggleConstraint={(id, enabled) =>
            void toggleConstraint(id, enabled)
          }
          onDeleteConstraint={(id) => void deleteConstraint(id)}
          onAddEventMarker={() => void addEventMarker()}
          onDeleteEventMarker={(id) => void deleteEventMarker(id)}
          onUndo={() => void perform(() => serverSession.undo(), "Undoing")}
          onRedo={() => void perform(() => serverSession.redo(), "Redoing")}
          onGenerateAll={generateAll}
          onGenerateAllOutdated={generateAllOutdated}
          onDuplicateTrajectory={(trajectory) =>
            void duplicateTrajectory(trajectory)
          }
        />

        <section className={styles.editorMain}>
          {activeTrajectory ? (
            <>
              <ServerNavbar
                waypointType={placementType}
                constraintType={constraintTool}
                pending={pending}
                connected={state.connected}
                onWaypointTypeChange={(type) => {
                  setPlacementType(type);
                  setConstraintTool(null);
                  setActiveEditor("waypoint");
                }}
                onConstraintTypeChange={(type) => {
                  setConstraintTool(type);
                  setActiveEditor(type === null ? "waypoint" : "none");
                }}
              />
              {(actionError || state.error) && (
                <Alert severity="error" onClose={() => setActionError(null)}>
                  {actionError ?? state.error}
                </Alert>
              )}
              {state.lastEvent?.type.startsWith("generation.") &&
                "trajectoryId" in state.lastEvent &&
                state.lastEvent.trajectoryId === activeTrajectory.uuid && (
                  <div className={styles.generationStatus}>
                    {("message" in state.lastEvent &&
                      state.lastEvent.message) ||
                      ("status" in state.lastEvent
                        ? state.lastEvent.status
                        : "Generation update")}
                    {state.lastEvent.type === "generation.progress" &&
                      typeof state.lastEvent.progress === "number" && (
                        <span>
                          {Math.round(state.lastEvent.progress * 100)}%
                        </span>
                      )}
                  </div>
                )}
              <div className={styles.canvasArea}>
                <div className={styles.fieldViewport}>
                  <ServerFieldCanvas
                    waypoints={activeTrajectory.params.waypoints}
                    selectedWaypointId={selectedWaypointId}
                    constraintType={constraintTool}
                    bumperVertices={bumperVertices}
                    samples={generatedSamples}
                    robotPose={
                      animatedSample
                        ? {
                            ...animatedSample.pose.translation,
                            heading: animatedSample.pose.rotation.radians
                          }
                        : undefined
                    }
                    visibleLayers={visibleLayers}
                    onAddWaypoint={(x, y) => void addWaypoint(x, y)}
                    onAddConstraint={(type, startIndex, endIndex) =>
                      void addConstraint(type, startIndex, endIndex)
                    }
                  />
                  <ServerFieldControls
                    visibleLayers={visibleLayers}
                    pending={pending}
                    connected={state.connected}
                    onToggleLayer={toggleLayer}
                    onGenerate={() => void generateTrajectory(activeTrajectory)}
                  />
                  {constraintTool === null &&
                    activeEditor === "waypoint" &&
                    selectedWaypoint && (
                      <ServerWaypointEditor
                        key={selectedWaypoint.uuid}
                        index={activeTrajectory.params.waypoints.indexOf(
                          selectedWaypoint
                        )}
                        isLast={
                          activeTrajectory.params.waypoints.indexOf(
                            selectedWaypoint
                          ) ===
                          activeTrajectory.params.waypoints.length - 1
                        }
                        waypoint={selectedWaypoint}
                        disabled={pending || !state.connected}
                        onOptionsChange={(options) =>
                          void patchWaypointOptions(selectedWaypoint, options)
                        }
                        onDelete={() =>
                          void deleteWaypoint(
                            selectedWaypoint,
                            activeTrajectory.params.waypoints.indexOf(
                              selectedWaypoint
                            )
                          )
                        }
                        onSave={(pose) =>
                          void patchWaypoint(selectedWaypoint, pose)
                        }
                      />
                    )}
                  {constraintTool === null &&
                    activeEditor === "constraint" &&
                    selectedConstraint && (
                      <div
                        className={`${styles.featureEditorPanel} ${styles.constraintEditorPanel}`}
                      >
                        <h3>
                          {selectedConstraint.data.type.replace(
                            /([a-z])([A-Z])/g,
                            "$1 $2"
                          )}
                        </h3>
                        <ServerConstraintEditor
                          constraint={selectedConstraint}
                          points={activeTrajectory.params.waypoints}
                          disabled={pending || !state.connected}
                          onChange={(data) =>
                            void patchConstraint(selectedConstraint, data)
                          }
                          onScopeChange={(from, to) =>
                            void patchConstraintScope(
                              selectedConstraint,
                              from,
                              to as NonNullable<ConstraintDocument["to"]>
                            )
                          }
                        />
                      </div>
                    )}
                  {constraintTool === null &&
                    activeEditor === "marker" &&
                    selectedEventMarker && (
                      <div className={styles.featureEditorPanel}>
                        <h3>Event Marker</h3>
                        <ServerEventMarkerEditor
                          marker={selectedEventMarker}
                          points={activeTrajectory.params.waypoints}
                          disabled={pending || !state.connected}
                          onChange={(patch) =>
                            void patchEventMarker(selectedEventMarker, patch)
                          }
                        />
                      </div>
                    )}
                  {activeTrajectory.params.waypoints.length === 0 && (
                    <p className={styles.emptyHint}>
                      Double-click the field to place the first waypoint.
                    </p>
                  )}
                </div>
              </div>
            </>
          ) : (
            <div className={styles.noTrajectory}>
              <h2>Select or add a trajectory</h2>
              <Button startIcon={<Add />} onClick={() => void addTrajectory()}>
                Add trajectory
              </Button>
            </div>
          )}
        </section>
      </div>
      <ServerVariablesPanel
        variables={project.variables.variables}
        order={
          project.variables.order ??
          Object.keys(project.variables.variables).sort()
        }
        disabled={pending || !state.connected}
        onExpressionChange={(id, expression) =>
          void patchVariable(id, { expression })
        }
        onNameChange={(id, name) => void patchVariable(id, { name })}
        onAdd={(variable) => void addVariable(variable)}
        onDelete={(id) => void deleteVariable(id)}
        onReorder={(order) => void reorderVariables(order)}
      />
      <ServerPathAnimationPanel
        samples={outputSamples}
        timestamp={animationTimestamp}
        onChange={setAnimationTimestamp}
      />
    </main>
  );
}

export default ServerWorkspace;

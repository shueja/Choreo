import * as d3 from "d3";
import { useEffect, useRef, useState } from "react";
import { FIELD_LENGTH, FIELD_WIDTH } from "./svg/fields/FieldDimensions";
import Field2026Url from "./svg/fields/FieldImage2026.svg?url";
import { attachFieldZoom } from "./svg/FieldZoom";
import FieldAxisLayer from "./svg/FieldAxisLayer";
import FieldBumperOutline from "./svg/FieldBumperOutline";
import FieldGridLayer from "./svg/FieldGridLayer";
import FieldPathLine from "./svg/FieldPathLine";
import FieldSampleLayer from "./svg/FieldSampleLayer";
import { ServerConstraintType, ServerViewLayers } from "./ServerViewLayers";
import {
  GeometryDragAnalysis,
  GeometryDragControl,
  ServerVariable,
  Waypoint
} from "../../document/ServerSessionStore";
import {
  GeometryDragPose,
  composeGeometryChain,
  geometryChainPose,
  updateGeometryExpressionForDrag
} from "../../util/GeometryExpressionDrag";
import styles from "./ServerFieldCanvas.module.css";

interface Props {
  waypoints: Waypoint[];
  variables: Record<string, ServerVariable>;
  waypointControls: Record<string, GeometryDragAnalysis>;
  variableControls: Record<string, GeometryDragAnalysis>;
  selectedWaypointId: string;
  bumperVertices: { x: number; y: number }[];
  robotPose?: { x: number; y: number; heading: number };
  samples: { x: number; y: number }[];
  visibleLayers: ServerViewLayers;
  constraintType: ServerConstraintType | null;
  onAddWaypoint: (x: number, y: number) => void;
  onDragWaypoint: (waypointId: string, expression: string) => Promise<void>;
  onDragVariable: (variableId: string, expression: string) => Promise<void>;
  onAddConstraint: (
    type: ServerConstraintType,
    fromIndex: number,
    toIndex: number
  ) => void;
}

type FieldPose = GeometryDragPose;

interface ActiveDrag {
  id: string;
  kind: "waypoint" | "variable";
  expression: string;
  mode: "position" | "heading";
  pointerId: number;
  control: GeometryDragControl;
  chain: GeometryDragAnalysis["chain"];
  pose: FieldPose;
  pointerStart: { x: number; y: number };
  candidateExpression: string;
  candidate: FieldPose;
}

function variablePose(variable: ServerVariable): FieldPose | null {
  if (variable.type === "Pose2d") {
    const pose = variable.value as {
      translation?: { x?: number; y?: number };
      rotation?: { radians?: number };
    };
    const { x, y } = pose.translation ?? {};
    const heading = pose.rotation?.radians;
    if (
      Number.isFinite(x) &&
      Number.isFinite(y) &&
      Number.isFinite(heading)
    ) {
      return { x: x!, y: y!, heading: heading! };
    }
  }
  if (variable.type === "Translation2d") {
    const translation = variable.value as { x?: number; y?: number };
    const { x, y } = translation;
    if (Number.isFinite(x) && Number.isFinite(y)) {
      return { x: x!, y: y!, heading: 0 };
    }
  }
  return null;
}

export default function ServerFieldCanvas({
  waypoints,
  variables,
  waypointControls,
  variableControls,
  selectedWaypointId,
  bumperVertices,
  robotPose,
  samples,
  visibleLayers,
  constraintType,
  onAddWaypoint,
  onDragWaypoint,
  onDragVariable,
  onAddConstraint
}: Props) {
  const svgRef = useRef<SVGSVGElement | null>(null);
  const frameRef = useRef<SVGGElement | null>(null);
  const activeDrag = useRef<ActiveDrag | null>(null);
  const [dragPreview, setDragPreview] = useState<{
    id: string;
    kind: ActiveDrag["kind"];
    pose: FieldPose;
    chain: GeometryDragAnalysis["chain"];
  } | null>(null);
  const [transform, setTransform] = useState<d3.ZoomTransform>(d3.zoomIdentity);
  const [constraintStartIndex, setConstraintStartIndex] = useState<
    number | null
  >(null);
  const waypointPoses = waypoints.map((waypoint) => {
    const pose =
      geometryChainPose(waypointControls[waypoint.uuid]?.chain) ??
      ({
        ...waypoint.pose.val.translation,
        heading: waypoint.pose.val.rotation.radians
      } satisfies FieldPose);
    return dragPreview?.id === waypoint.uuid &&
      dragPreview.kind === "waypoint"
      ? dragPreview.pose
      : pose;
  });
  const points = waypointPoses.map(({ x, y }) => ({ x, y }));
  const fieldMarkerSize = Math.min(
    bumperVertices.length > 0
      ? Math.max(...bumperVertices.map((point) => point.x)) -
          Math.min(...bumperVertices.map((point) => point.x))
      : 0,
    bumperVertices.length > 0
      ? Math.max(...bumperVertices.map((point) => point.y)) -
          Math.min(...bumperVertices.map((point) => point.y))
      : 0
  ) || 0.9;

  function pointerCoordinates(
    event: React.PointerEvent<SVGElement>
  ): { x: number; y: number } | null {
    const svg = svgRef.current;
    const matrix = frameRef.current?.getScreenCTM();
    if (!svg || !matrix) {
      return null;
    }
    const point = svg.createSVGPoint();
    point.x = event.clientX;
    point.y = event.clientY;
    return point.matrixTransform(matrix.inverse());
  }

  function beginDrag(
    event: React.PointerEvent<SVGCircleElement>,
    drag: Omit<
      ActiveDrag,
      "pointerId" | "pointerStart" | "candidate" | "candidateExpression"
    >
  ) {
    if (
      constraintType !== null ||
      (drag.mode === "position"
        ? drag.control.dx === 0 && drag.control.dy === 0
        : drag.control.dheading === 0)
    ) {
      return;
    }
    const pointer = pointerCoordinates(event);
    if (!pointer) {
      return;
    }
    event.stopPropagation();
    event.preventDefault();
    event.currentTarget.setPointerCapture(event.pointerId);
    activeDrag.current = {
      ...drag,
      pointerId: event.pointerId,
      pointerStart: pointer,
      candidateExpression: drag.expression,
      candidate: drag.pose
    };
    const chain = composeGeometryChain(drag.chain);
    setDragPreview({ id: drag.id, kind: drag.kind, pose: drag.pose, chain });
  }

  function moveDrag(event: React.PointerEvent<SVGGElement>) {
    const drag = activeDrag.current;
    if (!drag || drag.pointerId !== event.pointerId) {
      return;
    }
    const pointer = pointerCoordinates(event);
    if (!pointer) {
      return;
    }
    let parameterDelta = 0;
    if (drag.mode === "position") {
      const dx = pointer.x - drag.pointerStart.x;
      const dy = pointer.y - drag.pointerStart.y;
      const normSquared =
        drag.control.dx * drag.control.dx +
        drag.control.dy * drag.control.dy;
      parameterDelta =
        (dx * drag.control.dx + dy * drag.control.dy) / normSquared;
    } else {
      const startAngle = Math.atan2(
        drag.pointerStart.y - drag.control.anchorY,
        drag.pointerStart.x - drag.control.anchorX
      );
      const currentAngle = Math.atan2(
        pointer.y - drag.control.anchorY,
        pointer.x - drag.control.anchorX
      );
      const angleDelta = Math.atan2(
        Math.sin(currentAngle - startAngle),
        Math.cos(currentAngle - startAngle)
      );
      parameterDelta = angleDelta / drag.control.dheading;
    }

    const previewForDelta = (delta: number) => {
      const chain = composeGeometryChain(drag.chain, drag.control, delta);
      const pose = geometryChainPose(chain);
      return pose ? { chain, pose } : null;
    };
    const isInField = (pose: FieldPose) =>
      pose.x >= 0 &&
      pose.x <= FIELD_LENGTH &&
      pose.y >= 0 &&
      pose.y <= FIELD_WIDTH;
    let preview = previewForDelta(parameterDelta);
    if (preview && !isInField(preview.pose)) {
      let inFieldDelta = 0;
      let outOfFieldDelta = parameterDelta;
      for (let iteration = 0; iteration < 32; iteration += 1) {
        const midpoint = (inFieldDelta + outOfFieldDelta) / 2;
        const midpointPreview = previewForDelta(midpoint);
        if (midpointPreview && isInField(midpointPreview.pose)) {
          inFieldDelta = midpoint;
        } else {
          outOfFieldDelta = midpoint;
        }
      }
      parameterDelta = inFieldDelta;
      preview = previewForDelta(parameterDelta);
    }
    if (!preview) {
      return;
    }
    const expression = updateGeometryExpressionForDrag(
      drag.expression,
      drag.control,
      parameterDelta
    );
    if (expression === null) {
      return;
    }
    drag.candidate = preview.pose;
    drag.candidateExpression = expression;
    setDragPreview({
      id: drag.id,
      kind: drag.kind,
      pose: preview.pose,
      chain: preview.chain
    });
  }

  function endDrag(event: React.PointerEvent<SVGGElement>) {
    const drag = activeDrag.current;
    if (!drag || drag.pointerId !== event.pointerId) {
      return;
    }
    activeDrag.current = null;
    const update =
      drag.kind === "waypoint"
        ? onDragWaypoint(drag.id, drag.candidateExpression)
        : onDragVariable(drag.id, drag.candidateExpression);
    void update.finally(() => setDragPreview(null));
  }

  function cancelDrag(event: React.PointerEvent<SVGGElement>) {
    if (activeDrag.current?.pointerId === event.pointerId) {
      activeDrag.current = null;
      setDragPreview(null);
    }
  }

  useEffect(() => {
    setConstraintStartIndex(null);
  }, [constraintType]);

  useEffect(() => {
    if (!svgRef.current) {
      return;
    }
    return attachFieldZoom(svgRef.current, setTransform);
  }, []);

  return (
    <div
      aria-label="Trajectory field. Double-click to add a waypoint."
      className={styles.frame}
      onDoubleClick={(event) => {
        if (constraintType !== null) {
          return;
        }
        const svg = svgRef.current;
        const frame = frameRef.current;
        if (!svg || !frame) {
          return;
        }
        const point = svg.createSVGPoint();
        point.x = event.clientX;
        point.y = event.clientY;
        const coordinates = point.matrixTransform(
          frame.getScreenCTM()!.inverse()
        );
        const x = Math.max(0, Math.min(FIELD_LENGTH, coordinates.x));
        const y = Math.max(0, Math.min(FIELD_WIDTH, coordinates.y));
        onAddWaypoint(Number(x.toFixed(3)), Number(y.toFixed(3)));
      }}
    >
      <svg
        ref={svgRef}
        aria-hidden="true"
        className={styles.field}
        viewBox={`-0.5 ${0.5 - (FIELD_WIDTH + 1)} ${FIELD_LENGTH + 1} ${FIELD_WIDTH + 1}`}
      >
        <g
          ref={frameRef}
          transform={`matrix(${transform.k} 0 0 ${-transform.k} ${transform.x} ${transform.y})`}
          onPointerMove={moveDrag}
          onPointerUp={endDrag}
          onPointerCancel={cancelDrag}
        >
          <FieldAxisLayer />
          {visibleLayers.field && (
            <image
              href={Field2026Url}
              x={-0.5}
              y={-0.5}
              width={FIELD_LENGTH + 1}
              height={FIELD_WIDTH + 1}
              pointerEvents="none"
            />
          )}
          {visibleLayers.grid && <FieldGridLayer />}
          {visibleLayers.trajectory && (
            <FieldPathLine points={samples} stroke="var(--select-yellow)" />
          )}
          {visibleLayers.samples && <FieldSampleLayer points={samples} />}
          {visibleLayers.waypoints && (
            <>
              <FieldPathLine points={points} />
              {waypoints.map((waypoint, index) => {
                const waypointPose = waypointPoses[index];
                const waypointAnalysis = waypointControls[waypoint.uuid];
                const activePreview =
                  dragPreview?.id === waypoint.uuid &&
                  dragPreview.kind === "waypoint"
                    ? dragPreview
                    : null;
                const renderedChain = activePreview
                  ? activePreview.chain
                  : composeGeometryChain(waypointAnalysis?.chain ?? []);
                const pose =
                  activePreview
                    ? activePreview.pose
                    : waypointPose;
                const { x, y, heading } = pose;
                const xCoordinates = bumperVertices.map((point) => point.x);
                const yCoordinates = bumperVertices.map((point) => point.y);
                const bumperLength =
                  xCoordinates.length > 0
                    ? Math.max(...xCoordinates) - Math.min(...xCoordinates)
                    : 0;
                const bumperWidth =
                  yCoordinates.length > 0
                    ? Math.max(...yCoordinates) - Math.min(...yCoordinates)
                    : 0;
                const markerSize =
                  Math.min(bumperLength, bumperWidth) || fieldMarkerSize;
                const headingSideLength =
                  yCoordinates.length > 0
                    ? 2 * 0.1 * Math.sqrt(Math.PI / Math.sqrt(3)) * markerSize
                    : 0.269 * markerSize;
                const frontX =
                  xCoordinates.length > 0
                    ? Math.max(...xCoordinates)
                    : markerSize / 2;
                const headingHeight = (Math.sqrt(3) / 2) * headingSideLength;

                const markerColor =
                  waypoint.uuid === selectedWaypointId
                    ? "var(--select-yellow)"
                    : waypoints.length >= 2 && index === 0
                      ? "green"
                      : waypoints.length >= 2 && index === waypoints.length - 1
                        ? "red"
                        : "var(--accent-purple)";
                const outlineColor =
                  waypoint.uuid === selectedWaypointId
                    ? "var(--select-yellow)"
                    : "var(--accent-purple)";
                return (
                  <g key={waypoint.uuid}>
                    <g
                      transform={`translate(${x} ${y}) rotate(${(heading * 180) / Math.PI})`}
                      onDoubleClick={(event) => event.stopPropagation()}
                    >
                      {bumperVertices.length > 2 && (
                        <FieldBumperOutline
                          points={bumperVertices}
                          strokeColor={outlineColor}
                          strokeWidth={2}
                        />
                      )}
                      {bumperVertices.length > 2 && (
                        <polygon
                          transform={`translate(${frontX}, 0)`}
                          fill={outlineColor}
                          stroke="black"
                          strokeWidth={0.03}
                          points={
                            `${-headingHeight / 2},${headingSideLength / 2} ` +
                            `${-headingHeight / 2},${-headingSideLength / 2} ` +
                            `${headingHeight / 2},0`
                          }
                        />
                      )}
                      <circle
                        r={markerSize * 0.15 || 0.14}
                        fill={markerColor}
                        stroke="black"
                        strokeWidth={0.03}
                      />
                    </g>
                    {renderedChain.length > 1 &&
                      renderedChain.slice(1).map((link, index) => (
                        <g key={`chain-link-${index}`}>
                          <line
                            x1={link.startX}
                            y1={link.startY}
                            x2={link.endX}
                            y2={link.endY}
                            stroke="var(--select-yellow)"
                            strokeWidth={0.035}
                            pointerEvents="none"
                          />
                          <circle
                            cx={link.endX}
                            cy={link.endY}
                            r={markerSize * 0.07 || 0.06}
                            fill="var(--select-yellow)"
                            stroke="black"
                            strokeWidth={0.02}
                            pointerEvents="none"
                          />
                        </g>
                      ))}
                    {(waypointAnalysis?.controls ?? []).map(
                      (control, controlIndex) => {
                        const anchor =
                          renderedChain[control.chainIndex] ?? null;
                        const anchorX = anchor?.startX ?? control.anchorX;
                        const anchorY = anchor?.startY ?? control.anchorY;
                        const anchorHeading =
                          anchor?.startHeading ?? control.anchorHeading;
                        const positionMagnitude = Math.hypot(
                          control.dx,
                          control.dy
                        );
                        const handleDistance = markerSize * 0.8;
                        const handleX =
                          anchorX +
                          (positionMagnitude > 0
                            ? (control.dx / positionMagnitude) * handleDistance
                            : 0);
                        const handleY =
                          anchorY +
                          (positionMagnitude > 0
                            ? (control.dy / positionMagnitude) * handleDistance
                            : 0);
                        const rotationX =
                          anchorX +
                          Math.cos(anchorHeading + Math.PI / 2) *
                            handleDistance;
                        const rotationY =
                          anchorY +
                          Math.sin(anchorHeading + Math.PI / 2) *
                            handleDistance;
                        return (
                          <g key={`control-${controlIndex}`}>
                            {positionMagnitude > 1e-10 && (
                              <>
                                <line
                                  x1={anchorX}
                                  y1={anchorY}
                                  x2={handleX}
                                  y2={handleY}
                                  stroke="var(--select-yellow)"
                                  strokeWidth={0.035}
                                  pointerEvents="none"
                                />
                                <circle
                                  cx={handleX}
                                  cy={handleY}
                                  r={markerSize * 0.1 || 0.09}
                                  fill="var(--select-yellow)"
                                  stroke="black"
                                  strokeWidth={0.025}
                                  style={{
                                    cursor: constraintType === null ? "move" : "default",
                                    pointerEvents: constraintType === null ? "auto" : "none"
                                  }}
                                  onPointerDown={(event) =>
                                    beginDrag(event, {
                                      id: waypoint.uuid,
                                      kind: "waypoint",
                                      expression: waypoint.pose.exp,
                                      mode: "position",
                                      control,
                                      chain: waypointAnalysis?.chain ?? [],
                                      pose: waypointPose
                                    })
                                  }
                                />
                              </>
                            )}
                            {Math.abs(control.dheading) > 1e-10 && (
                              <circle
                                cx={rotationX}
                                cy={rotationY}
                                r={markerSize * 0.09 || 0.08}
                                fill="var(--accent-purple)"
                                stroke="black"
                                strokeWidth={0.025}
                                style={{
                                  cursor:
                                    constraintType === null ? "crosshair" : "default",
                                  pointerEvents:
                                    constraintType === null ? "auto" : "none"
                                }}
                                onPointerDown={(event) =>
                                  beginDrag(event, {
                                    id: waypoint.uuid,
                                    kind: "waypoint",
                                    expression: waypoint.pose.exp,
                                    mode: "heading",
                                    control,
                                    chain: waypointAnalysis?.chain ?? [],
                                    pose: waypointPose
                                  })
                                }
                              />
                            )}
                          </g>
                        );
                      }
                    )}
                  </g>
                );
              })}
              {Object.entries(variables).map(([id, variable]) => {
                if (
                  variable.type !== "Pose2d" &&
                  variable.type !== "Translation2d"
                ) {
                  return null;
                }
                const controls = variableControls[id] ?? {
                  controls: [],
                  chain: []
                };
                const pose =
                  geometryChainPose(controls.chain) ?? variablePose(variable);
                if (!pose) {
                  return null;
                }
                const activePreview =
                  dragPreview?.id === id && dragPreview.kind === "variable"
                    ? dragPreview
                    : null;
                const renderedChain = activePreview
                  ? activePreview.chain
                  : composeGeometryChain(controls.chain);
                const displayedPose =
                  activePreview
                    ? activePreview.pose
                    : pose;
                const color = "var(--select-yellow)";
                return (
                  <g
                    key={`variable-${id}`}
                    onDoubleClick={(event) => event.stopPropagation()}
                  >
                    <circle
                      cx={displayedPose.x}
                      cy={displayedPose.y}
                      r={fieldMarkerSize * 0.12 || 0.11}
                      fill="transparent"
                      stroke={color}
                      strokeWidth={0.045}
                    />
                    {renderedChain.length > 1 &&
                      renderedChain.slice(1).map((link, linkIndex) => (
                        <g key={`chain-${linkIndex}`}>
                          <line
                            x1={link.startX}
                            y1={link.startY}
                            x2={link.endX}
                            y2={link.endY}
                            stroke={color}
                            strokeWidth={0.035}
                            strokeDasharray={linkIndex === 0 ? "0" : "0.08 0.05"}
                            pointerEvents="none"
                          />
                          <circle
                            cx={link.endX}
                            cy={link.endY}
                            r={fieldMarkerSize * 0.07 || 0.06}
                            fill={color}
                            stroke="black"
                            strokeWidth={0.02}
                            pointerEvents="none"
                          />
                        </g>
                      ))}
                    <text
                      x={displayedPose.x + fieldMarkerSize * 0.15}
                      y={displayedPose.y - fieldMarkerSize * 0.2}
                      transform="scale(1, -1)"
                      fill={color}
                      fontSize={fieldMarkerSize * 0.2 || 0.18}
                      pointerEvents="none"
                    >
                      {variable.name}
                    </text>
                    {controls.controls.map((control, controlIndex) => {
                      const anchor =
                        renderedChain[control.chainIndex] ?? null;
                      const anchorX = anchor?.startX ?? control.anchorX;
                      const anchorY = anchor?.startY ?? control.anchorY;
                      const anchorHeading =
                        anchor?.startHeading ?? control.anchorHeading;
                      const magnitude = Math.hypot(control.dx, control.dy);
                      const handleDistance = fieldMarkerSize * 0.8;
                      const handleX =
                        anchorX +
                        (magnitude > 0
                          ? (control.dx / magnitude) * handleDistance
                          : 0);
                      const handleY =
                        anchorY +
                        (magnitude > 0
                          ? (control.dy / magnitude) * handleDistance
                          : 0);
                      const rotationX =
                        anchorX +
                        Math.cos(anchorHeading + Math.PI / 2) *
                          handleDistance;
                      const rotationY =
                        anchorY +
                        Math.sin(anchorHeading + Math.PI / 2) *
                          handleDistance;
                      return (
                        <g key={`control-${controlIndex}`}>
                          {magnitude > 1e-10 && (
                            <>
                              <line
                                x1={anchorX}
                                y1={anchorY}
                                x2={handleX}
                                y2={handleY}
                                stroke={color}
                                strokeWidth={0.035}
                                pointerEvents="none"
                              />
                              <circle
                                cx={handleX}
                                cy={handleY}
                                r={fieldMarkerSize * 0.1 || 0.09}
                                fill={color}
                                stroke="black"
                                strokeWidth={0.025}
                                style={{
                                  cursor: constraintType === null ? "move" : "default",
                                  pointerEvents: constraintType === null ? "auto" : "none"
                                }}
                                onPointerDown={(event) =>
                                  beginDrag(event, {
                                    id,
                                    kind: "variable",
                                    expression: variable.expression,
                                    mode: "position",
                                    control,
                                    chain: controls.chain,
                                    pose
                                  })
                                }
                              />
                            </>
                          )}
                          {Math.abs(control.dheading) > 1e-10 && (
                            <circle
                              cx={rotationX}
                              cy={rotationY}
                              r={fieldMarkerSize * 0.09 || 0.08}
                              fill="var(--accent-purple)"
                              stroke="black"
                              strokeWidth={0.025}
                              style={{
                                cursor:
                                  constraintType === null ? "crosshair" : "default",
                                pointerEvents:
                                  constraintType === null ? "auto" : "none"
                              }}
                              onPointerDown={(event) =>
                                beginDrag(event, {
                                  id,
                                  kind: "variable",
                                  expression: variable.expression,
                                  mode: "heading",
                                  control,
                                  chain: controls.chain,
                                  pose
                                })
                              }
                            />
                          )}
                        </g>
                      );
                    })}
                  </g>
                );
              })}
            </>
          )}
          {constraintType !== null &&
            waypoints.map((waypoint, index) => {
              const { x, y } = waypoint.pose.val.translation;
              const selected = index === constraintStartIndex;
              return (
                <circle
                  key={`constraint-target-${waypoint.uuid}`}
                  cx={x}
                  cy={y}
                  r={0.24}
                  fill={selected ? "var(--darker-purple)" : "black"}
                  fillOpacity={selected ? 0.3 : 0.12}
                  stroke={selected ? "var(--select-yellow)" : "white"}
                  strokeWidth={0.035}
                  onClick={(event) => {
                    event.stopPropagation();
                    if (constraintStartIndex === null) {
                      setConstraintStartIndex(index);
                      return;
                    }
                    onAddConstraint(
                      constraintType,
                      Math.min(constraintStartIndex, index),
                      Math.max(constraintStartIndex, index)
                    );
                    setConstraintStartIndex(null);
                  }}
                />
              );
            })}
          {robotPose && bumperVertices.length > 2 && (
            <g
              transform={`translate(${robotPose.x} ${robotPose.y}) rotate(${(robotPose.heading * 180) / Math.PI})`}
            >
              <FieldBumperOutline
                points={bumperVertices}
                strokeColor="var(--select-yellow)"
                strokeWidth={2}
              />
            </g>
          )}
        </g>
      </svg>
    </div>
  );
}

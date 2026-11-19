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
  ServerVariable,
  Waypoint
} from "../../document/ServerSessionStore";
import {
  EditableGeometryAxes,
  GeometryDragValues,
  getEditableGeometryAxes,
  updateGeometryExpressionForDrag
} from "../../util/GeometryExpressionDrag";
import styles from "./ServerFieldCanvas.module.css";

interface Props {
  waypoints: Waypoint[];
  variables: Record<string, ServerVariable>;
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

interface FieldPose extends GeometryDragValues {
  heading: number;
}

interface ActiveDrag {
  id: string;
  kind: "waypoint" | "variable";
  type: "Pose2d" | "Translation2d";
  expression: string;
  axes: EditableGeometryAxes;
  mode: "position" | "heading";
  pointerId: number;
  pose: FieldPose;
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
  } | null>(null);
  const [transform, setTransform] = useState<d3.ZoomTransform>(d3.zoomIdentity);
  const [constraintStartIndex, setConstraintStartIndex] = useState<
    number | null
  >(null);
  const points = waypoints.map((waypoint) => waypoint.pose.val.translation);

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
    drag: Omit<ActiveDrag, "pointerId" | "candidate">
  ) {
    if (
      constraintType !== null ||
      (drag.mode === "position"
        ? !drag.axes.x && !drag.axes.y
        : !drag.axes.heading)
    ) {
      return;
    }
    event.stopPropagation();
    event.preventDefault();
    event.currentTarget.setPointerCapture(event.pointerId);
    activeDrag.current = {
      ...drag,
      pointerId: event.pointerId,
      candidate: drag.pose
    };
    setDragPreview({ id: drag.id, kind: drag.kind, pose: drag.pose });
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
    const candidate =
      drag.mode === "position"
        ? {
            ...drag.pose,
            x: Math.max(0, Math.min(FIELD_LENGTH, pointer.x)),
            y: Math.max(0, Math.min(FIELD_WIDTH, pointer.y))
          }
        : {
            ...drag.pose,
            heading: Math.atan2(
              pointer.y - drag.pose.y,
              pointer.x - drag.pose.x
            )
          };
    drag.candidate = candidate;
    setDragPreview({ id: drag.id, kind: drag.kind, pose: candidate });
  }

  function endDrag(event: React.PointerEvent<SVGGElement>) {
    const drag = activeDrag.current;
    if (!drag || drag.pointerId !== event.pointerId) {
      return;
    }
    activeDrag.current = null;
    const expression = updateGeometryExpressionForDrag(
      drag.expression,
      drag.type,
      drag.candidate,
      drag.axes
    );
    if (expression === null) {
      setDragPreview(null);
      return;
    }
    const update =
      drag.kind === "waypoint"
        ? onDragWaypoint(drag.id, expression)
        : onDragVariable(drag.id, expression);
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
                const waypointPose: FieldPose = {
                  ...waypoint.pose.val.translation,
                  heading: waypoint.pose.val.rotation.radians
                };
                const pose =
                  dragPreview?.id === waypoint.uuid &&
                  dragPreview.kind === "waypoint"
                    ? dragPreview.pose
                    : waypointPose;
                const { x, y, heading } = pose;
                const axes = getEditableGeometryAxes(
                  waypoint.pose.exp,
                  "Pose2d"
                );
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
                const markerSize = Math.min(bumperLength, bumperWidth) || 0.9;
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
                  <g
                    key={waypoint.uuid}
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
                      style={{
                        cursor:
                          constraintType === null && (axes.x || axes.y)
                            ? "move"
                            : "default",
                        pointerEvents:
                          constraintType === null && (axes.x || axes.y)
                            ? "auto"
                            : "none"
                      }}
                      onPointerDown={(event) =>
                        beginDrag(event, {
                          id: waypoint.uuid,
                          kind: "waypoint",
                          type: "Pose2d",
                          expression: waypoint.pose.exp,
                          axes,
                          mode: "position",
                          pose: waypointPose
                        })
                      }
                    />
                    {axes.heading && (
                      <>
                        <line
                          x1={0}
                          y1={0}
                          x2={markerSize * 1.15}
                          y2={0}
                          stroke={outlineColor}
                          strokeWidth={0.035}
                        />
                        <circle
                          cx={markerSize * 1.15}
                          cy={0}
                          r={markerSize * 0.1 || 0.09}
                          fill={outlineColor}
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
                              type: "Pose2d",
                              expression: waypoint.pose.exp,
                              axes,
                              mode: "heading",
                              pose: waypointPose
                            })
                          }
                        />
                      </>
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
                const pose = variablePose(variable);
                if (!pose) {
                  return null;
                }
                const geometryType = variable.type;
                const axes = getEditableGeometryAxes(
                  variable.expression,
                  geometryType
                );
                if (!axes.x && !axes.y && !axes.heading) {
                  return null;
                }
                const displayedPose =
                  dragPreview?.id === id && dragPreview.kind === "variable"
                    ? dragPreview.pose
                    : pose;
                const color = "var(--select-yellow)";
                return (
                  <g
                    key={`variable-${id}`}
                    transform={`translate(${displayedPose.x} ${displayedPose.y}) rotate(${(displayedPose.heading * 180) / Math.PI})`}
                    onDoubleClick={(event) => event.stopPropagation()}
                  >
                    <circle
                      r={markerSize * 0.12 || 0.11}
                      fill="transparent"
                      stroke={color}
                      strokeWidth={0.045}
                      style={{
                        cursor:
                          constraintType === null && (axes.x || axes.y)
                            ? "move"
                            : "default",
                        pointerEvents:
                          constraintType === null && (axes.x || axes.y)
                            ? "auto"
                            : "none"
                      }}
                      onPointerDown={(event) =>
                        beginDrag(event, {
                          id,
                          kind: "variable",
                          type: geometryType,
                          expression: variable.expression,
                          axes,
                          mode: "position",
                          pose
                        })
                      }
                    />
                    <text
                      x={markerSize * 0.15}
                      y={-markerSize * 0.2}
                      transform="scale(1, -1)"
                      fill={color}
                      fontSize={markerSize * 0.2 || 0.18}
                      pointerEvents="none"
                    >
                      {variable.name}
                    </text>
                    {geometryType === "Pose2d" && axes.heading && (
                      <circle
                        cx={markerSize * 1.1}
                        cy={0}
                        r={markerSize * 0.09 || 0.08}
                        fill={color}
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
                            type: "Pose2d",
                            expression: variable.expression,
                            axes,
                            mode: "heading",
                            pose
                          })
                        }
                      />
                    )}
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

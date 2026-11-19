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
import { Waypoint } from "../../document/ServerSessionStore";
import styles from "./ServerFieldCanvas.module.css";

interface Props {
  waypoints: Waypoint[];
  selectedWaypointId: string;
  bumperVertices: { x: number; y: number }[];
  robotPose?: { x: number; y: number; heading: number };
  samples: { x: number; y: number }[];
  visibleLayers: ServerViewLayers;
  constraintType: ServerConstraintType | null;
  onAddWaypoint: (x: number, y: number) => void;
  onAddConstraint: (
    type: ServerConstraintType,
    fromIndex: number,
    toIndex: number
  ) => void;
}

export default function ServerFieldCanvas({
  waypoints,
  selectedWaypointId,
  bumperVertices,
  robotPose,
  samples,
  visibleLayers,
  constraintType,
  onAddWaypoint,
  onAddConstraint
}: Props) {
  const svgRef = useRef<SVGSVGElement | null>(null);
  const frameRef = useRef<SVGGElement | null>(null);
  const [transform, setTransform] = useState<d3.ZoomTransform>(d3.zoomIdentity);
  const [constraintStartIndex, setConstraintStartIndex] = useState<
    number | null
  >(null);
  const points = waypoints.map((waypoint) => waypoint.pose.val.translation);

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
                const { x, y } = waypoint.pose.val.translation;
                const heading = waypoint.pose.val.rotation.radians;
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

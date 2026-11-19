import { useRef, useState } from "react";
import type {
  GeometryDragAnalysis,
  GeometryDragControl,
  GeometryDragEffect,
  GeometryFeaturePoint,
  ServerVariable
} from "../../document/ServerSessionStore";
import { updateGeometryExpressionForDrag } from "../../util/GeometryExpressionDrag";

interface Props {
  variables: Record<string, ServerVariable>;
  analyses: Record<string, GeometryDragAnalysis>;
  disabled: boolean;
  pointerCoordinates: (
    event: React.PointerEvent<SVGElement>
  ) => { x: number; y: number } | null;
  onDragVariable: (id: string, expression: string) => Promise<void>;
}

interface ActiveDrag {
  id: string;
  pointerId: number;
  expression: string;
  control: GeometryDragControl;
  effect: GeometryDragEffect;
  pointerStart: { x: number; y: number };
  paths: GeometryFeaturePoint[][];
  candidateExpression: string;
}

function movedPaths(
  paths: GeometryFeaturePoint[][],
  control: GeometryDragControl,
  delta: number
): GeometryFeaturePoint[][] {
  return paths.map((path, pathIndex) =>
    path.map((point, pointIndex) => {
      const effect = control.effects.find(
        (entry) =>
          entry.pathIndex === pathIndex && entry.pointIndex === pointIndex
      );
      return effect
        ? { x: point.x + effect.dx * delta, y: point.y + effect.dy * delta }
        : point;
    })
  );
}

function maxEffect(control: GeometryDragControl): GeometryDragEffect | null {
  return (
    control.effects.reduce<GeometryDragEffect | null>(
      (largest, effect) =>
        !largest ||
        Math.hypot(effect.dx, effect.dy) >
          Math.hypot(largest.dx, largest.dy)
          ? effect
          : largest,
      null
    ) ?? null
  );
}

export default function GeometryFieldLayer({
  variables,
  analyses,
  disabled,
  pointerCoordinates,
  onDragVariable
}: Props) {
  const activeDrag = useRef<ActiveDrag | null>(null);
  const [preview, setPreview] = useState<{
    id: string;
    paths: GeometryFeaturePoint[][];
  } | null>(null);

  function moveDrag(event: React.PointerEvent<SVGGElement>) {
    const drag = activeDrag.current;
    if (!drag || drag.pointerId !== event.pointerId) {
      return;
    }
    const pointer = pointerCoordinates(event);
    if (!pointer) {
      return;
    }
    const normSquared =
      drag.effect.dx * drag.effect.dx + drag.effect.dy * drag.effect.dy;
    if (normSquared === 0) {
      return;
    }
    const parameterDelta =
      ((pointer.x - drag.pointerStart.x) * drag.effect.dx +
        (pointer.y - drag.pointerStart.y) * drag.effect.dy) /
      normSquared;
    const candidateExpression = updateGeometryExpressionForDrag(
      drag.expression,
      drag.control,
      parameterDelta
    );
    if (!candidateExpression) {
      return;
    }
    drag.candidateExpression = candidateExpression;
    setPreview({
      id: drag.id,
      paths: movedPaths(drag.paths, drag.control, parameterDelta)
    });
  }

  function endDrag(event: React.PointerEvent<SVGGElement>) {
    const drag = activeDrag.current;
    if (!drag || drag.pointerId !== event.pointerId) {
      return;
    }
    activeDrag.current = null;
    if (drag.candidateExpression === drag.expression) {
      setPreview(null);
      return;
    }
    void onDragVariable(drag.id, drag.candidateExpression).finally(() =>
      setPreview(null)
    );
  }

  function cancelDrag(event: React.PointerEvent<SVGGElement>) {
    if (activeDrag.current?.pointerId === event.pointerId) {
      activeDrag.current = null;
      setPreview(null);
    }
  }

  return (
    <g onPointerMove={moveDrag} onPointerUp={endDrag} onPointerCancel={cancelDrag}>
      {Object.entries(variables).map(([id, variable]) => {
        if (variable.type === "Quantity") {
          return null;
        }
        const analysis = analyses[id];
        if (!analysis?.paths.length) {
          return null;
        }
        const paths =
          preview?.id === id ? preview.paths : analysis.paths;
        const points = paths.flat();
        if (points.length === 0) {
          return null;
        }
        const center = points.reduce(
          (sum, point) => ({ x: sum.x + point.x, y: sum.y + point.y }),
          { x: 0, y: 0 }
        );
        center.x /= points.length;
        center.y /= points.length;
        const fill =
          variable.type === "Polygon2d" ||
          variable.type === "Rectangle2d" ||
          variable.type === "Ellipse2d";
        const color = "var(--select-yellow)";
        return (
          <g key={id}>
            {paths.map((path, pathIndex) => (
              <g key={`path-${pathIndex}`}>
                <polyline
                  points={path.map(({ x, y }) => `${x},${y}`).join(" ")}
                  fill={fill ? color : "none"}
                  fillOpacity={fill ? 0.15 : undefined}
                  stroke={color}
                  strokeWidth={0.04}
                  pointerEvents="none"
                />
                {(path.length === 1 || variable.type === "Polygon2d") &&
                  path.map(({ x, y }, pointIndex) => (
                    <circle
                      key={`point-${pointIndex}`}
                      cx={x}
                      cy={y}
                      r={0.045}
                      fill={color}
                      pointerEvents="none"
                    />
                  ))}
              </g>
            ))}
            <text
              x={center.x + 0.12}
              y={center.y - 0.18}
              transform="scale(1, -1)"
              fill={color}
              fontSize={0.18}
              pointerEvents="none"
            >
              {variable.name}
            </text>
            {analysis.controls.map((control, controlIndex) => {
              const effect = maxEffect(control);
              const point =
                effect && paths[effect.pathIndex]?.[effect.pointIndex];
              if (!effect || !point) {
                return null;
              }
              const magnitude = Math.hypot(effect.dx, effect.dy);
              const handleX = point.x + (effect.dx / magnitude) * 0.5;
              const handleY = point.y + (effect.dy / magnitude) * 0.5;
              return (
                <g key={`control-${controlIndex}`}>
                  <line
                    x1={point.x}
                    y1={point.y}
                    x2={handleX}
                    y2={handleY}
                    stroke={color}
                    strokeWidth={0.035}
                    pointerEvents="none"
                  />
                  <circle
                    cx={handleX}
                    cy={handleY}
                    r={0.09}
                    fill={color}
                    stroke="black"
                    strokeWidth={0.025}
                    style={{ cursor: disabled ? "default" : "move" }}
                    onPointerDown={(event) => {
                      const pointer = pointerCoordinates(event);
                      if (disabled || !pointer || event.button !== 0) {
                        return;
                      }
                      event.stopPropagation();
                      event.preventDefault();
                      event.currentTarget.setPointerCapture(event.pointerId);
                      activeDrag.current = {
                        id,
                        pointerId: event.pointerId,
                        expression: variable.expression,
                        control,
                        effect,
                        pointerStart: pointer,
                        paths: analysis.paths,
                        candidateExpression: variable.expression
                      };
                      setPreview({ id, paths: analysis.paths });
                    }}
                  />
                </g>
              );
            })}
          </g>
        );
      })}
    </g>
  );
}

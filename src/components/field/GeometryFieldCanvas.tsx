import * as d3 from "d3";
import { useEffect, useRef, useState } from "react";
import type { GeometryDragAnalysis } from "@choreo/api";
import type { ServerVariable } from "@choreo/document";
import { FIELD_LENGTH, FIELD_WIDTH } from "./svg/fields/FieldDimensions";
import Field2026Url from "./svg/fields/FieldImage2026.svg?url";
import { attachFieldZoom } from "./svg/FieldZoom";
import FieldAxisLayer from "./svg/FieldAxisLayer";
import FieldGridLayer from "./svg/FieldGridLayer";
import type { ServerViewLayers } from "./ServerViewLayers";
import GeometryFieldLayer from "./GeometryFieldLayer";
import styles from "./ServerFieldCanvas.module.css";

interface Props {
  variables: Record<string, ServerVariable>;
  analyses: Record<string, GeometryDragAnalysis>;
  visibleLayers: ServerViewLayers;
  disabled: boolean;
  onDragVariable: (id: string, expression: string) => Promise<void>;
}

export default function GeometryFieldCanvas({
  variables,
  analyses,
  visibleLayers,
  disabled,
  onDragVariable
}: Props) {
  const svgRef = useRef<SVGSVGElement | null>(null);
  const frameRef = useRef<SVGGElement | null>(null);
  const [transform, setTransform] = useState<d3.ZoomTransform>(d3.zoomIdentity);

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

  useEffect(() => {
    if (!svgRef.current) {
      return;
    }
    return attachFieldZoom(svgRef.current, setTransform);
  }, []);

  return (
    <div aria-label="Geometry field. Drag geometry handles to edit expressions." className={styles.frame}>
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
          <GeometryFieldLayer
            variables={variables}
            analyses={analyses}
            disabled={disabled}
            pointerCoordinates={pointerCoordinates}
            onDragVariable={onDragVariable}
          />
        </g>
      </svg>
    </div>
  );
}

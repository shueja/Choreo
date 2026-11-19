import {
  GeometryChainLink,
  GeometryDragControl
} from "../document/ServerSessionStore";

export interface GeometryDragValues {
  x: number;
  y: number;
  heading?: number;
}

export interface GeometryDragPose extends GeometryDragValues {
  heading: number;
}

export function composeGeometryChain(
  chain: GeometryChainLink[],
  control?: GeometryDragControl,
  parameterDelta = 0
): GeometryChainLink[] {
  if (chain.length === 0) {
    return [];
  }

  const root = {
    x: chain[0].startX,
    y: chain[0].startY,
    heading: chain[0].startHeading
  };
  if (control?.chainIndex === 0) {
    root.x += control.segmentDx * parameterDelta;
    root.y += control.segmentDy * parameterDelta;
    root.heading += control.segmentDheading * parameterDelta;
  }

  const result: GeometryChainLink[] = [
    {
      startX: root.x,
      startY: root.y,
      startHeading: root.heading,
      endX: root.x,
      endY: root.y,
      endHeading: root.heading
    }
  ];
  let pose = root;
  for (let index = 1; index < chain.length; index += 1) {
    const link = chain[index];
    const relativeX = link.endX - link.startX;
    const relativeY = link.endY - link.startY;
    const cosine = Math.cos(link.startHeading);
    const sine = Math.sin(link.startHeading);
    let localX = cosine * relativeX + sine * relativeY;
    let localY = -sine * relativeX + cosine * relativeY;
    let localHeading = link.endHeading - link.startHeading;
    if (control?.chainIndex === index) {
      localX += control.segmentDx * parameterDelta;
      localY += control.segmentDy * parameterDelta;
      localHeading += control.segmentDheading * parameterDelta;
    }

    const start = pose;
    pose = {
      x: start.x + Math.cos(start.heading) * localX -
        Math.sin(start.heading) * localY,
      y: start.y + Math.sin(start.heading) * localX +
        Math.cos(start.heading) * localY,
      heading: start.heading + localHeading
    };
    result.push({
      startX: start.x,
      startY: start.y,
      startHeading: start.heading,
      endX: pose.x,
      endY: pose.y,
      endHeading: pose.heading
    });
  }
  return result;
}

export function geometryChainPose(chain: GeometryChainLink[]): GeometryDragPose | null {
  const composed = composeGeometryChain(chain);
  const last = composed.at(-1);
  return last
    ? { x: last.endX, y: last.endY, heading: last.endHeading }
    : null;
}

export function updateGeometryExpressionForDrag(
  expression: string,
  control: GeometryDragControl,
  parameterDelta: number
): string | null {
  const replacementValue = control.value + parameterDelta;
  if (!Number.isFinite(replacementValue)) {
    return null;
  }

  const utf8ToUtf16Offset = (byteOffset: number): number | null => {
    let bytes = 0;
    for (let index = 0; index < expression.length; ) {
      if (bytes === byteOffset) {
        return index;
      }
      const codePoint = expression.codePointAt(index)!;
      const charLength = codePoint > 0xffff ? 2 : 1;
      const byteLength =
        codePoint <= 0x7f
          ? 1
          : codePoint <= 0x7ff
            ? 2
            : codePoint <= 0xffff
              ? 3
              : 4;
      bytes += byteLength;
      index += charLength;
      if (bytes > byteOffset) {
        return null;
      }
    }
    return bytes === byteOffset ? expression.length : null;
  };
  const start = utf8ToUtf16Offset(control.start);
  const end = utf8ToUtf16Offset(control.end);
  if (start === null || end === null || end <= start) {
    return null;
  }
  const sourceValue = Number(expression.slice(start, end));
  if (
    !Number.isFinite(sourceValue) ||
    Math.abs(sourceValue - control.value) >
      Math.max(1e-12, Math.abs(control.value) * 1e-12)
  ) {
    return null;
  }
  return (
    expression.slice(0, start) +
    String(replacementValue) +
    expression.slice(end)
  );
}

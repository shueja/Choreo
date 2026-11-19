import { GeometryDragControl } from "../document/ServerSessionStore";

export interface GeometryDragValues {
  x: number;
  y: number;
  heading?: number;
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

import { FunctionNode, MathNode, parse } from "mathjs";

export interface AxisDragPermissions {
  x?: boolean;
  y?: boolean;
  heading?: boolean;
}

export interface GeometryDragValues {
  x: number;
  y: number;
  heading?: number;
}

const constantUnits = new Set([
  "m",
  "meter",
  "meters",
  "cm",
  "mm",
  "km",
  "in",
  "inch",
  "inches",
  "ft",
  "foot",
  "feet",
  "rad",
  "radian",
  "radians",
  "deg",
  "degree",
  "degrees"
]);

function isConstantArgument(node: MathNode): boolean {
  if (node.type === "ConstantNode") {
    return true;
  }
  if (node.type === "SymbolNode") {
    return constantUnits.has((node as MathNode & { name: string }).name);
  }
  if (node.type === "ParenthesisNode") {
    return isConstantArgument(
      (node as MathNode & { content: MathNode }).content
    );
  }
  if (node.type === "OperatorNode") {
    return (
      (node as MathNode & { args: MathNode[] }).args.length > 0 &&
      (node as MathNode & { args: MathNode[] }).args.every(
        isConstantArgument
      )
    );
  }
  return false;
}

export function updateGeometryExpressionForDrag(
  expression: string,
  type: "Pose2d" | "Translation2d",
  values: GeometryDragValues,
  allowDrag: AxisDragPermissions
): string | null {
  let node: MathNode;
  try {
    node = parse(expression);
  } catch {
    return null;
  }
  if (node.type !== "FunctionNode") {
    return null;
  }

  const call = node as FunctionNode;
  if (call.name !== type) {
    return null;
  }

  const axes: Array<{
    name: keyof AxisDragPermissions;
    value: number | undefined;
    unit: "m" | "rad";
  }> = [
    { name: "x", value: values.x, unit: "m" },
    { name: "y", value: values.y, unit: "m" }
  ];
  if (type === "Pose2d") {
    axes.push({ name: "heading", value: values.heading, unit: "rad" });
  }

  const replacements: Array<{ start: number; end: number; text: string }> = [];
  axes.forEach((axis, index) => {
    if (!allowDrag[axis.name] || axis.value === undefined) {
      return;
    }
    const argument = call.args[index];
    const range = argument?.range;
    if (
      !argument ||
      !range ||
      !isConstantArgument(argument) ||
      !Number.isFinite(axis.value)
    ) {
      return;
    }
    replacements.push({
      start: range[0],
      end: range[1],
      text: `${axis.value} ${axis.unit}`
    });
  });

  if (replacements.length === 0) {
    return null;
  }
  return replacements
    .sort((left, right) => right.start - left.start)
    .reduce(
      (updated, replacement) =>
        updated.slice(0, replacement.start) +
        replacement.text +
        updated.slice(replacement.end),
      expression
    );
}

import {
  AccessorNode,
  ConstantNode,
  FunctionNode,
  IndexNode,
  MathNode,
  MathType,
  SymbolNode,
  Unit,
  all,
  create,
  isNull,
  isUnit
} from "mathjs";

export const math = create(all, { predictable: true });

export function isSymbolNode(node: MathNode): node is SymbolNode {
  return node.type === "SymbolNode";
}
export function isFunctionNode(node: MathNode): node is FunctionNode {
  return node.type === "FunctionNode";
}
export function isAccessorNode(node: MathNode): node is AccessorNode {
  return node.type === "AccessorNode";
}
export function isConstantNode(node: MathNode): node is ConstantNode {
  return node.type === "ConstantNode";
}

export function addUnitToExpression(
  expression: math.MathNode,
  unit?: string
): math.MathNode {
  if (unit === undefined) return expression;
  const unitNode = math.parse(unit);
  return new math.OperatorNode("*", "multiply", [expression, unitNode], true);
}

const isAlphaOriginal = Unit.isValidAlpha;
math.Unit.isValidAlpha = function (c) {
  return isAlphaOriginal(c) || c == "#";
};
export type Expr = { exp: string; val: number };

export function isExpr(arg: any): arg is Expr {
  return (
    typeof arg === "object" &&
    Object.hasOwn(arg, "exp") &&
    typeof arg["exp"] === "string" &&
    Object.hasOwn(arg, "val") &&
    typeof arg["val"] === "number"
  );
}
export type ExprOrNumber = Expr | number;
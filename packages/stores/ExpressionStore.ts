import { ConstantNode, FunctionNode, IndexNode, isConstantNode, isIndexNode, isAccessorNode, isFunctionNode, isSymbolNode, isNull, isUnit, MathNode, MathType, SymbolNode, Unit } from "mathjs";
import { DimensionName } from "@choreo/document/math/Dimensions";
import { untracked, IReactionDisposer, reaction } from "mobx";
import { Instance, types } from "mobx-state-tree";
import DimensionUnits from "@choreo/math/DimensionUnits";
import { addUnitToExpression, math } from "@choreo/math/math";
import { Expr, ExprOrNumber, isExpr } from "@choreo/document/math/Expr";

export type Evaluated = MathType | null | undefined;
type Evaluator = (arg: MathNode) => Evaluated;

export function transformSymbolsToFunctions(node: MathNode, scope: Map<string, any>) {
    return node.transform((innerNode, path, parent) => {
        // Match standalone symbols, turn them into FunctionNode(symbol)
        if (
            isSymbolNode(innerNode) && // Match symbols (string literals in the expression),
            //that are names of functions in our scope (just the standalone variables)
            typeof scope.get(innerNode.name) === "function" &&
            // The below avoids transforming `variable()` to `variable()()`
            !(
                // ...and ignoring those symbol nodes
                (
                    parent !== null && // with a non-null parent
                    isFunctionNode(parent) && // that is a function node
                    path == "fn"
                ) // where the symbol is already the name of the function
            )
        ) {
            return new math.FunctionNode(innerNode, []);
        }

        // Replace [pose].x (and .y, .heading) with [pose].x(), etc
        // This works for other objects in scope with children that are functions

        /*
        Replace [pose].x (and .y, .heading) with [pose].x(), etc
        This works for other objects in scope with children that are functions
        "[objectName].[childName]" parses to
         AccessorNode {
          object: SymbolNode {
            name: objectName
          },
          index: IndexNode {
            dimensions : [
              ConstantNode {
                value: childName
              }
            ]
          }
        }

          "[objectName].[childName]()" parses to
          FunctionNode {
            args: []
            fn: ...the parse of [objectName].[childName] above
          }
          So if we match [objectName].[childName], but it's already within a function node, don't transform it
        */

        if (isAccessorNode(innerNode)) {
            // filter out accessors within function nodes.
            if (!(parent !== null && isFunctionNode(parent) && path == "fn")) {
                const accessorNode = innerNode;
                const { object, index } = accessorNode;
                if (isSymbolNode(object) && index.isIndexNode) {
                    const symbol = object as SymbolNode;
                    const idx = index as IndexNode;
                    if (
                        idx.dimensions[0] !== undefined &&
                        isConstantNode(idx.dimensions[0])
                    ) {
                        const constant = idx.dimensions[0];
                        // We now know that innerNode was [symbol.name].[constant.value]
                        if (
                            typeof scope.get(symbol.name) === "object" &&
                            typeof scope.get(symbol.name)?.[constant.value] ===
                            "function"
                        ) {
                            // if the symbols are in fact things in our scope, replace `innerNode` with `innerNode()`
                            return new FunctionNode(innerNode, []);
                        }
                    }
                }
            }
        }
        return innerNode;
    });
}

export function isValidResult(newNode: MathNode, newNumber: MathType | undefined, expectedUnit: Unit | undefined): MathNode | undefined {
    if (newNumber === undefined || newNumber === null) {
        console.error("evaluated to undefined or null");
        return undefined;
    }
    // numbers are only valid on dimensionless expressions.
    if (typeof newNumber === "number") {
        if (expectedUnit !== undefined) {
            console.error("failed to evaluate: ", newNumber, "was dimensionless");
            return undefined;
        }
        if (!isFinite(newNumber)) {
            console.error("failed to evaluate: ", newNumber, "is infinite");
            return undefined;
        }
        // number is finite and on a dimensionless expression.
        return newNode;
    }
    // Anything past this (BigNumber | bigint | Fraction | Complex) isn't supported
    if (!isUnit(newNumber)) {
        console.error("not unit:", newNumber);
        return undefined;
    }
    // newNumber is Unit
    // Checking dimension matching
    const unit = expectedUnit;
    const numberIsDimensionless = newNumber.dimensions.every((d) => d == 0);
    if (unit !== undefined) {
        if (numberIsDimensionless) {
            // unit that's just a number (usually from units cancelling)

            console.error("failed to evaluate: ", newNumber, "was dimensionless");
            return undefined;
        } else {
            if (!newNumber.equalBase(unit)) {
                console.error("unit mismatch", unit);
                return undefined;
            }
        }
    } else if (unit === undefined && !numberIsDimensionless) {
        console.error(
            "failed to evaluate: ",
            newNumber,
            "is unit on dimensionless expr"
        );
        return undefined;
    }

    if (isNull(newNumber.value)) {
        console.error("valueless unit", unit);
        return undefined;
    }
    if (!isFinite(newNumber.value)) {
        console.error("failed to evaluate: ", newNumber.value, "is infinite");
        return undefined;
    }
    return newNode;
}
export const ExpressionStore = types
    .model("ExpressionStore", {
        expr: types.frozen<MathNode>(),
        dimension: types.frozen<DimensionName>(),
        // not types.identifier because then Mobx wants the UUID to be the key
        // when ExpressionStores are in Maps
        uuid: types.string
    })
    .views((self) => ({
        get defaultUnit(): Unit | undefined {
            return DimensionUnits[self.dimension];
        }
    }))
    .volatile((self) => ({
        // For optimization: If directly setting the quantity in default units, we don't parse and calculate the numerical value
        tempDisableRecalc: false,
        value: 0,
        // To avoid circular initialization, we set the correct scope getter in afterCreate
        getScope: () => {
            // console.error(
            //     "ExpressionStore did not set its scope getter!",
            //     self.toString()
            // );
            return new Map<string, any>();
        }
    }))
    .actions((self) => ({
        findReplaceVariable(find: string, replace: string) {
            self.expr = self.expr.transform(function (node, _path, _parent) {
                if (isSymbolNode(node) && node.name === find) {
                    const clone = node.clone();
                    clone.name = replace;
                    return clone;
                } else {
                    return node;
                }
            });
        },
        deserialize(serial: Expr) {
            self.expr = math.parse(serial.exp);
            self.value = serial.val;
            return self;
        },
        // WARNING: should not be generally used. This is for cases
        // where the user needs to change the unit for validation
        setDimension(newDefault: DimensionName) {
            self.dimension = newDefault;
        },
        setScopeGetter(getter: () => Map<string, any>) {
            self.getScope = getter;
        },
        set(newNode: MathNode | number) {
            if (typeof newNode === "number") {
                self.tempDisableRecalc = true;
                if (self.defaultUnit === undefined) {
                    self.expr = new ConstantNode(newNode);
                    this.setValue(newNode);
                } else {
                    self.expr = math.parse(
                        math.unit(newNode, self.defaultUnit.toString()).toString()
                    );
                    this.setValue(newNode);
                }
                return;
            }

            self.expr = newNode;
        },
        setValue(value: number) {
            self.value = value;
        },
        setTempDisableRecalc(disable: boolean) {
            self.tempDisableRecalc = disable;
        }
    }))
    .views((self) => ({
        evaluator(node: MathNode): MathType | undefined {
            try {
                // TODO investigate whether this should be untracked
                const scope: Map<string, any> =
                    self.getScope() ??
                    ((() => {
                        console.error("Evaluating without variables!");
                        return undefined;
                    }) as Evaluator);
                // Depend on the keys list of the scope, to re-evaluate when the variables list changes
                scope.keys();
                // turn symbol variables into function variables if they're found in scope
                const transformed = transformSymbolsToFunctions(node, scope);
                const result = transformed.evaluate(scope) ?? undefined;

                return result;
            } catch {
                return undefined;
            }
        },
        get serialize(): Expr {
            return {
                exp: self.expr.toString(),
                val: self.value
            };
        }
    }))
    .views((self) => ({
        get evaluate(): MathType | undefined {
            const result = self.evaluator(self.expr);
            return result;
        }
    }))
    .views((self) => ({
        get asScope(): () => MathType | undefined {
            return () => {
                // eslint-disable-next-line @typescript-eslint/no-unused-expressions
                self.value;
                try {
                    const expr = untracked(() => {
                        return self.evaluate;
                    });
                    return expr;
                } catch {
                    return undefined;
                }
            };
        },
        get toDefaultUnit(): Unit | number | undefined {
            // eslint-disable-next-line @typescript-eslint/no-unused-expressions
            self.expr;

            const result = self.evaluate;
            if (result === undefined || result === null) {
                return undefined;
            }
            if (typeof result === "number") {
                if (self.defaultUnit === undefined) {
                    return result;
                }
                console.error(
                    "unit expression",
                    self.expr.toString(),
                    "evaluated to number"
                );
                return math.unit(result, self.defaultUnit.toString());
            }
            if (self.defaultUnit === undefined) {
                console.error(
                    "number expression",
                    self.expr.toString(),
                    "evaluated to unit"
                );
                return undefined;
            }
            return math.unit(result.toString()).to(self.defaultUnit!.toString());
        },
        get defaultUnitMagnitude(): number | undefined {
            // eslint-disable-next-line @typescript-eslint/no-unused-expressions
            self.expr;

            const defaultUnit = this.toDefaultUnit;
            if (typeof defaultUnit === "number") {
                return defaultUnit;
            }
            return defaultUnit?.toNumber(self.defaultUnit!.toString());
        },
        validate(newNode: MathNode): MathNode | undefined {
            // number | BigNumber | bigint | Fraction | Complex | Unit
            let newNumber: MathType | undefined | null;
            try {
                newNumber = self.evaluator(newNode);
            } catch (e) {
                // Syntax errors, primarily
                console.error("failed to evaluate", e, newNode);
                return undefined;
            }
            return isValidResult(newNode, newNumber, self.defaultUnit);
        },
        get valid(): boolean {
            return this.validate(self.expr) !== undefined;
        }
    }))
    .volatile((self) => {
        let recalcDispose: IReactionDisposer;
        return {
            afterCreate: () => {
                recalcDispose = reaction(
                    () => {
                        if (!self.tempDisableRecalc) {
                            try {
                                const value = self.defaultUnitMagnitude;
                                if (value !== undefined) {
                                    return value;
                                }
                            } finally {
                                self.setTempDisableRecalc(false);
                            }
                        } else {
                            self.setTempDisableRecalc(false);
                            return self.value;
                        }
                    },
                    (value) => {
                        if (value !== undefined) {
                            self.setValue(value);
                        }
                    },
                    // do this calculation when setting up the reaction
                    // so value is populated (default is false but this causes issues
                    // when restoring ExpressionStores out of undo history)
                    { fireImmediately: true }
                );
            },
            beforeDestroy: () => {
                recalcDispose();
            }
        };
    });
export type IExpressionStore = Instance<typeof ExpressionStore>;
export function createExpressionStore(expr: string | Expr | number, dimension: DimensionName, getScope: () => Map<string, any>): IExpressionStore {
    let mathNode: MathNode = (() => {
        if (typeof expr === "number") {
            if (dimension === "Number") {
                return new ConstantNode(expr)
            } else {
                return addUnitToExpression(
                    new math.ConstantNode(expr),
                    DimensionUnits[dimension]?.toString()
                );
            }
        } else if (isExpr(expr)) {
            return math.parse(expr.exp);
        } else {
            return math.parse(expr);
        }
    })();

    const store = ExpressionStore.create({
        expr: mathNode,
        dimension,
        uuid: crypto.randomUUID()
    });
    store.setScopeGetter(getScope);
    return store;
}
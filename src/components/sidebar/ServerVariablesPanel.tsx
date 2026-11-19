import {
  Add,
  ArrowDownward,
  ArrowUpward,
  DeleteOutline
} from "@mui/icons-material";
import {
  IconButton,
  MenuItem,
  Select,
  SelectChangeEvent,
  Tooltip
} from "@mui/material";
import { useEffect, useState } from "react";
import type {
  ServerVariable,
  ServerVariableType
} from "@choreo/document";
import ServerExpressionInput from "../input/ServerExpressionInput";
import InputStyles from "../input/InputList.module.css";
import styles from "./ServerVariablesPanel.module.css";

interface VariablePreset {
  key: string;
  label: string;
  type: ServerVariableType;
  dimension?: Record<string, number>;
  expression: string;
  value: unknown;
}

const originPose = {
  translation: { x: 0, y: 0 },
  rotation: { radians: 0 }
};
const polygonVertices = [
  { x: 0, y: 0 },
  { x: 1, y: 0 },
  { x: 0, y: 1 }
];

const variablePresets: VariablePreset[] = [
  {
    key: "Number",
    label: "Number",
    type: "Quantity",
    dimension: {},
    expression: "0",
    value: 0
  },
  {
    key: "Length",
    label: "Length",
    type: "Quantity",
    dimension: { meter: 1 },
    expression: "0 m",
    value: 0
  },
  {
    key: "Linear Velocity",
    label: "Linear Velocity",
    type: "Quantity",
    dimension: { meter: 1, second: -1 },
    expression: "0 m / s",
    value: 0
  },
  {
    key: "Linear Acceleration",
    label: "Linear Acceleration",
    type: "Quantity",
    dimension: { meter: 1, second: -2 },
    expression: "0 m / s^2",
    value: 0
  },
  {
    key: "Angle",
    label: "Angle",
    type: "Quantity",
    dimension: { radian: 1 },
    expression: "0 rad",
    value: 0
  },
  {
    key: "Angular Velocity",
    label: "Angular Velocity",
    type: "Quantity",
    dimension: { radian: 1, second: -1 },
    expression: "0 rad / s",
    value: 0
  },
  {
    key: "Angular Acceleration",
    label: "Angular Acceleration",
    type: "Quantity",
    dimension: { radian: 1, second: -2 },
    expression: "0 rad / s^2",
    value: 0
  },
  {
    key: "Time",
    label: "Time",
    type: "Quantity",
    dimension: { second: 1 },
    expression: "0 s",
    value: 0
  },
  {
    key: "Mass",
    label: "Mass",
    type: "Quantity",
    dimension: { kilogram: 1 },
    expression: "0 kg",
    value: 0
  },
  {
    key: "Torque",
    label: "Torque",
    type: "Quantity",
    dimension: { kilogram: 1, meter: 2, second: -2 },
    expression: "0 N * m",
    value: 0
  },
  {
    key: "Moment of Inertia",
    label: "Moment of Inertia",
    type: "Quantity",
    dimension: { kilogram: 1, meter: 2 },
    expression: "0 kg * m^2",
    value: 0
  },
  {
    key: "Pose2d",
    label: "Pose2d",
    type: "Pose2d",
    expression: "Pose2d()",
    value: originPose
  },
  {
    key: "Translation2d",
    label: "Translation2d",
    type: "Translation2d",
    expression: "Translation2d()",
    value: { x: 0, y: 0 }
  },
  {
    key: "Polygon2d",
    label: "Polygon2d",
    type: "Polygon2d",
    expression:
      "[Translation2d(), Translation2d(1 m, 0 m), Translation2d(0 m, 1 m)]",
    value: { vertices: polygonVertices }
  },
  {
    key: "Rotation2d",
    label: "Rotation2d",
    type: "Rotation2d",
    expression: "Rotation2d()",
    value: { radians: 0 }
  },
  {
    key: "Transform2d",
    label: "Transform2d",
    type: "Transform2d",
    expression: "Transform2d()",
    value: { translation: { x: 0, y: 0 }, rotation: { radians: 0 } }
  },
  {
    key: "Twist2d",
    label: "Twist2d",
    type: "Twist2d",
    expression: "Twist2d()",
    value: { dx: 0, dy: 0, dtheta: 0 }
  },
  {
    key: "Rectangle2d",
    label: "Rectangle2d",
    type: "Rectangle2d",
    expression: "Rectangle2d(Pose2d(), 2 m, 1 m)",
    value: { center: originPose, xWidth: 2, yWidth: 1 }
  },
  {
    key: "Ellipse2d",
    label: "Ellipse2d",
    type: "Ellipse2d",
    expression: "Ellipse2d(Pose2d(), 1 m, 1 m)",
    value: { center: originPose, xSemiAxis: 1, ySemiAxis: 1 }
  }
];

interface Props {
  variables: Record<string, ServerVariable>;
  order: string[];
  disabled: boolean;
  onExpressionChange: (id: string, expression: string) => void;
  onNameChange: (id: string, name: string) => void;
  onAdd: (variable: ServerVariable) => void;
  onDelete: (id: string) => void;
  onReorder: (order: string[]) => void;
}

function dimensionsMatch(
  left: Record<string, number> | undefined,
  right: Record<string, number> | undefined
): boolean {
  const keys = new Set([
    ...Object.keys(left ?? {}),
    ...Object.keys(right ?? {})
  ]);
  return [...keys].every((key) => (left?.[key] ?? 0) === (right?.[key] ?? 0));
}

function variableTypeLabel(variable: ServerVariable): string {
  if (variable.type !== "Quantity") {
    return variable.type;
  }
  return (
    variablePresets.find(
      (preset) =>
        preset.type === "Quantity" &&
        dimensionsMatch(preset.dimension, variable.dimension)
    )?.label ?? "Quantity"
  );
}

function isIdentifier(value: string): boolean {
  return /^[A-Za-z_][A-Za-z0-9_]*$/.test(value);
}

interface VariableDeclarationProps {
  id: string;
  index: number;
  count: number;
  variable: ServerVariable;
  disabled: boolean;
  onExpressionChange: Props["onExpressionChange"];
  onNameChange: Props["onNameChange"];
  onDelete: Props["onDelete"];
  onMove: (id: string, offset: -1 | 1) => void;
}

function VariableDeclaration({
  id,
  index,
  count,
  variable,
  disabled,
  onExpressionChange,
  onNameChange,
  onDelete,
  onMove
}: VariableDeclarationProps) {
  const [name, setName] = useState(variable.name);

  useEffect(() => {
    setName(variable.name);
  }, [variable.name]);

  function commitName() {
    const next = name.trim();
    if (!isIdentifier(next)) {
      setName(variable.name);
      return;
    }
    if (next !== variable.name) {
      onNameChange(id, next);
    }
  }

  const qualifiedName = variable.namespacePath
    ? `${variable.namespacePath}.${name}`
    : name;

  return (
    <div className={styles.declaration}>
      <span className={styles.typeName}>{variableTypeLabel(variable)}</span>
      <span className={styles.namespace}>
        {variable.namespacePath ? `${variable.namespacePath}.` : ""}
      </span>
      <input
        aria-label={`Name for ${qualifiedName}`}
        className={`${InputStyles.Number} ${InputStyles.ShowWhenDisabled} ${styles.nameInput}`}
        value={name}
        disabled={disabled}
        onChange={(event) => setName(event.target.value)}
        onBlur={commitName}
        onKeyDown={(event) => {
          if (event.key === "Enter") {
            event.currentTarget.blur();
          }
          if (event.key === "Escape") {
            setName(variable.name);
            event.currentTarget.blur();
          }
        }}
      />
      <span className={styles.equals}>=</span>
      <div className={styles.initializer}>
        <ServerExpressionInput
          title={qualifiedName}
          ariaLabel={`Initializer for ${qualifiedName}`}
          hideTitle
          stretch
          value={{ exp: variable.expression, val: variable.value }}
          enabled={!disabled}
          onCommit={(expression) => onExpressionChange(id, expression.exp)}
        />
      </div>
      <div className={styles.actions}>
        <Tooltip title="Move variable up">
          <span>
            <IconButton
              aria-label={`Move variable ${qualifiedName} up`}
              size="small"
              disabled={disabled || index === 0}
              onClick={() => onMove(id, -1)}
            >
              <ArrowUpward fontSize="small" />
            </IconButton>
          </span>
        </Tooltip>
        <Tooltip title="Move variable down">
          <span>
            <IconButton
              aria-label={`Move variable ${qualifiedName} down`}
              size="small"
              disabled={disabled || index === count - 1}
              onClick={() => onMove(id, 1)}
            >
              <ArrowDownward fontSize="small" />
            </IconButton>
          </span>
        </Tooltip>
        <Tooltip title="Delete variable">
          <span>
            <IconButton
              aria-label={`Delete variable ${qualifiedName}`}
              size="small"
              disabled={disabled}
              onClick={() => onDelete(id)}
            >
              <DeleteOutline fontSize="small" />
            </IconButton>
          </span>
        </Tooltip>
      </div>
    </div>
  );
}

export default function ServerVariablesPanel({
  variables,
  order,
  disabled,
  onExpressionChange,
  onNameChange,
  onAdd,
  onDelete,
  onReorder
}: Props) {
  const [presetKey, setPresetKey] = useState("Number");
  const [newName, setNewName] = useState("");
  const [newExpression, setNewExpression] = useState("0");
  const selectedPreset =
    variablePresets.find((preset) => preset.key === presetKey) ??
    variablePresets[0];
  const orderedIds = [
    ...order.filter((id) => Object.hasOwn(variables, id)),
    ...Object.keys(variables)
      .filter((id) => !order.includes(id))
      .sort()
  ];

  function moveVariable(id: string, offset: -1 | 1) {
    const index = orderedIds.indexOf(id);
    const nextIndex = index + offset;
    if (index < 0 || nextIndex < 0 || nextIndex >= orderedIds.length) {
      return;
    }
    const nextOrder = [...orderedIds];
    [nextOrder[index], nextOrder[nextIndex]] = [
      nextOrder[nextIndex],
      nextOrder[index]
    ];
    onReorder(nextOrder);
  }

  function addVariable() {
    const name = newName.trim();
    if (!isIdentifier(name) || disabled) {
      return;
    }
    onAdd({
      name,
      namespacePath: "",
      expression: newExpression.trim(),
      type: selectedPreset.type,
      ...(selectedPreset.dimension
        ? { dimension: selectedPreset.dimension }
        : {}),
      value: selectedPreset.value
    });
    setNewName("");
  }

  return (
    <section className={styles.panel} aria-label="Variables">
      <header className={styles.header}>
        <strong>VARIABLES</strong>
        <span className={styles.declarationHint}>type name = initializer</span>
      </header>
      <div className={styles.body}>
        {orderedIds.map((id, index) => (
          <VariableDeclaration
            key={id}
            id={id}
            index={index}
            count={orderedIds.length}
            variable={variables[id]}
            disabled={disabled}
            onExpressionChange={onExpressionChange}
            onNameChange={onNameChange}
            onDelete={onDelete}
            onMove={moveVariable}
          />
        ))}
        {orderedIds.length === 0 && (
          <span className={styles.empty}>No variables</span>
        )}
      </div>
      <form
        className={styles.addForm}
        onSubmit={(event) => {
          event.preventDefault();
          addVariable();
        }}
      >
        <Select
          aria-label="New variable type"
          className={styles.typeSelect}
          size="small"
          variant="standard"
          value={presetKey}
          disabled={disabled}
          onChange={(event: SelectChangeEvent) => {
            const preset = variablePresets.find(
              (entry) => entry.key === event.target.value
            );
            if (preset) {
              setPresetKey(preset.key);
              setNewExpression(preset.expression);
            }
          }}
        >
          {variablePresets.map((preset) => (
            <MenuItem key={preset.key} value={preset.key}>
              {preset.label}
            </MenuItem>
          ))}
        </Select>
        <input
          aria-label="New variable name"
          className={styles.nameInput}
          placeholder="name"
          value={newName}
          disabled={disabled}
          onChange={(event) => setNewName(event.target.value)}
        />
        <span className={styles.equals}>=</span>
        <div className={styles.initializer}>
          <ServerExpressionInput
            title="New variable initializer"
            ariaLabel="New variable initializer"
            hideTitle
            stretch
            value={{ exp: newExpression, val: selectedPreset.value }}
            enabled={!disabled}
            onCommit={(expression) => setNewExpression(expression.exp)}
          />
        </div>
        <Tooltip title="Add variable">
          <span>
            <IconButton
              aria-label="Add variable"
              size="small"
              type="submit"
              disabled={
                disabled ||
                !isIdentifier(newName.trim()) ||
                newExpression.trim().length === 0
              }
            >
              <Add />
            </IconButton>
          </span>
        </Tooltip>
      </form>
    </section>
  );
}

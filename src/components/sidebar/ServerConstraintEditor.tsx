import {
  ConstraintDocument,
  Waypoint
} from "../../document/ServerSessionStore";
import ServerExpressionInput, {
  ServerExpression
} from "../input/ServerExpressionInput";
import ServerScopeSlider from "../input/ServerScopeSlider";
import inputStyles from "../input/InputList.module.css";

interface Props {
  constraint: ConstraintDocument;
  points: Waypoint[];
  disabled: boolean;
  onChange: (data: ConstraintDocument["data"]) => void;
  onScopeChange: (
    from: ConstraintDocument["from"],
    to: NonNullable<ConstraintDocument["to"]>
  ) => void;
}

function scopeIndex(
  scope: ConstraintDocument["from"],
  points: Waypoint[]
): number {
  if (scope === "first") {
    return 0;
  }
  if (scope === "last") {
    return points.length + 1;
  }
  const index = points.findIndex((point) => point.uuid === scope.uuid);
  return index < 0 ? 0 : index + 1;
}

function scopeAtIndex(
  index: number,
  points: Waypoint[]
): ConstraintDocument["from"] {
  if (index === 0) {
    return "first";
  }
  if (index === points.length + 1) {
    return "last";
  }
  return { uuid: points[index - 1]?.uuid ?? "" };
}

function getFields(type: string): { key: string; label: string }[] {
  switch (type) {
    case "Heading":
      return [
        { key: "heading", label: "Heading" },
        { key: "tolerance", label: "Tolerance" }
      ];
    case "KeepInCircle":
      return [{ key: "circle", label: "Keep In Circle" }];
    case "MaxLinearVelocity":
      return [{ key: "max", label: "Max Velocity" }];
    case "MaxAngularVelocity":
      return [{ key: "max", label: "Max Angular Velocity" }];
    default:
      return [];
  }
}

export default function ServerConstraintEditor({
  constraint,
  points,
  disabled,
  onChange,
  onScopeChange
}: Props) {
  const from = scopeIndex(constraint.from, points);
  const to = constraint.to ? scopeIndex(constraint.to, points) : from;

  return (
    <div style={{ width: "100%" }}>
      <ServerScopeSlider
        waypointCount={points.length}
        isRange
        value={[from, to]}
        disabled={disabled}
        onChange={(selection) => {
          const range =
            typeof selection === "number" ? [selection, selection] : selection;
          onScopeChange(
            scopeAtIndex(range[0], points),
            scopeAtIndex(range[1], points) as NonNullable<
              ConstraintDocument["to"]
            >
          );
        }}
      />
      <div className={`${inputStyles.InputList} ${inputStyles.Expression}`}>
        {getFields(constraint.data.type).map(({ key, label }) => {
          const value = constraint.data[key];
          if (
            !value ||
            typeof value !== "object" ||
            !("exp" in value) ||
            !("val" in value)
          ) {
            return null;
          }
          return (
            <ServerExpressionInput
              key={`${constraint.uuid}-${key}`}
              title={label}
              value={value as ServerExpression}
              enabled={!disabled}
              onCommit={(expression) =>
                onChange({ ...constraint.data, [key]: expression })
              }
            />
          );
        })}
      </div>
    </div>
  );
}

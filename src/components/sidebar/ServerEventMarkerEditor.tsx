import { TextField, Tooltip } from "@mui/material";
import { useRef } from "react";
import type { EventMarkerPatch } from "@choreo/api";
import type {
  ServerEventMarkerDocument as EventMarkerDocument,
  ServerWaypoint as Waypoint
} from "@choreo/document";
import ServerExpressionInput from "../input/ServerExpressionInput";
import ServerScopeSlider from "../input/ServerScopeSlider";
import InputStyles from "../input/InputList.module.css";
import PanelStyles from "../config/WaypointConfigPanel.module.css";

interface Props {
  marker: EventMarkerDocument;
  points: Waypoint[];
  disabled: boolean;
  onChange: (patch: EventMarkerPatch) => void;
}

function scopeIndex(
  target: EventMarkerDocument["from"]["target"],
  points: Waypoint[]
): number {
  if (target === "first") {
    return 0;
  }
  if (target === "last") {
    return points.length + 1;
  }
  const index = points.findIndex((point) => point.uuid === target.uuid);
  return index < 0 ? 0 : index + 1;
}

function targetAtIndex(
  index: number,
  points: Waypoint[]
): EventMarkerDocument["from"]["target"] {
  if (index === 0) {
    return "first";
  }
  if (index === points.length + 1) {
    return "last";
  }
  return { uuid: points[index - 1]?.uuid ?? "" };
}

export default function ServerEventMarkerEditor({
  marker,
  points,
  disabled,
  onChange
}: Props) {
  const nameInput = useRef<HTMLInputElement | null>(null);

  return (
    <div
      className={PanelStyles.WaypointPanel}
      style={{
        width: `min(80%, max(400px, calc(${points.length} * 3ch + 8ch)))`
      }}
    >
      <ServerScopeSlider
        waypointCount={points.length}
        isRange={false}
        value={scopeIndex(marker.from.target, points)}
        disabled={disabled}
        onChange={(selection) => {
          const index =
            typeof selection === "number" ? selection : selection[0];
          onChange({ from: { target: targetAtIndex(index, points) } });
        }}
      />
      <span className={PanelStyles.markerHint}>
        Changes to waypoint target take effect after regeneration.
      </span>
      <div className={`${InputStyles.InputList} ${InputStyles.Expression}`}>
        <Tooltip disableInteractive title="Name of the marker">
          <span className={`${InputStyles.Title} ${InputStyles.Tooltip}`}>
            Name
          </span>
        </Tooltip>
        <TextField
          inputRef={nameInput}
          variant="standard"
          size="small"
          defaultValue={marker.name}
          key={`${marker.uuid}-${marker.name}`}
          disabled={disabled}
          onBlur={(event) => {
            const name = event.target.value.trim();
            if (name && name !== marker.name) {
              onChange({ name });
            }
          }}
          onKeyDown={(event) => {
            if (event.key === "Enter") {
              nameInput.current?.blur();
            }
          }}
        />
        <span />
        <span />
        <ServerExpressionInput
          title="Offset"
          titleTooltip="The marker's time offset before or after this waypoint"
          value={marker.from.offset}
          enabled={!disabled}
          onCommit={(offset) => onChange({ from: { offset } })}
        />
      </div>
    </div>
  );
}

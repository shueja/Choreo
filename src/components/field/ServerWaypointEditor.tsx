import {
  Circle,
  CircleOutlined,
  DeleteOutline,
  NearMe
} from "@mui/icons-material";
import {
  IconButton,
  ToggleButton,
  ToggleButtonGroup,
  Tooltip
} from "@mui/material";
import WaypointIcon from "../../assets/Waypoint";
import ServerBooleanInput from "../input/ServerBooleanInput";
import ServerExpressionInput from "../input/ServerExpressionInput";
import ServerNumberInput from "../input/ServerNumberInput";
import InputStyles from "../input/InputList.module.css";
import { Waypoint } from "../../document/ServerSessionStore";
import styles from "./ServerWaypointEditor.module.css";
import PanelStyles from "../config/WaypointConfigPanel.module.css";

interface Props {
  index: number;
  isLast: boolean;
  waypoint: Waypoint;
  disabled: boolean;
  onOptionsChange: (
    options: Partial<
      Pick<
        Waypoint,
        | "intervals"
        | "split"
        | "fixTranslation"
        | "fixHeading"
        | "overrideIntervals"
      >
    >
  ) => void;
  onSave: (pose: Waypoint["pose"]) => void;
  onDelete: () => void;
}

function waypointType(waypoint: Waypoint): string {
  if (waypoint.fixTranslation && waypoint.fixHeading) {
    return "full";
  }
  if (waypoint.fixTranslation) {
    return "translation";
  }
  if (waypoint.fixHeading) {
    return "heading";
  }
  return "empty";
}

export default function ServerWaypointEditor({
  index,
  isLast,
  waypoint,
  disabled,
  onOptionsChange,
  onSave,
  onDelete
}: Props) {
  return (
    <div
      className={PanelStyles.WaypointPanel}
      style={{
        position: "static",
        width: "100%",
        minWidth: 0,
        boxSizing: "border-box",
        padding: 0,
        borderRadius: 0
      }}
      aria-label={`Waypoint ${index + 1} settings`}
    >
      <div className={`${InputStyles.InputList} ${InputStyles.Expression}`}>
        <ServerExpressionInput
          title="Pose"
          titleTooltip="Pose2d expression for the waypoint position and heading"
          value={waypoint.pose}
          enabled={!disabled}
          onCommit={onSave}
        />
        <div className={styles.dragControls}>
          {(["x", "y", "heading"] as const).map((axis) => (
            <label key={axis}>
              <span>{axis === "heading" ? "Heading" : axis.toUpperCase()}</span>
              <input
                type="checkbox"
                aria-label={`Allow field drag for waypoint ${index + 1} ${axis}`}
                checked={waypoint.allowDrag?.[axis] ?? true}
                disabled={disabled}
                onChange={(event) =>
                  onOptionsChange({
                    allowDrag: {
                      ...(waypoint.allowDrag ?? {
                        x: true,
                        y: true,
                        heading: true
                      }),
                      [axis]: event.target.checked
                    }
                  })
                }
              />
            </label>
          ))}
        </div>
      </div>
      <div className={styles.controls}>
        <div className={InputStyles.InputList}>
          <ServerNumberInput
            title="Samples"
            titleTooltip="Override the number of samples between this and next waypoint"
            value={waypoint.intervals}
            enabled={waypoint.overrideIntervals && !isLast && !disabled}
            showNumberWhenDisabled={!isLast}
            showCheckbox={!isLast}
            checkboxEnabled={!disabled}
            maxWidthCharacters={4}
            roundingPrecision={0}
            onChange={(intervals) => onOptionsChange({ intervals })}
            onEnable={(overrideIntervals) =>
              onOptionsChange({ overrideIntervals })
            }
          />
          <ServerBooleanInput
            title="Split"
            titleTooltip="Split trajectory at this point. Does not force stopping."
            value={waypoint.split}
            enabled={!disabled}
            onChange={(split) => onOptionsChange({ split })}
          />
        </div>
        <ToggleButtonGroup
          exclusive
          size="small"
          value={waypointType(waypoint)}
          onChange={(_event, value: string | null) => {
            if (value === null) {
              return;
            }
            onOptionsChange({
              fixTranslation: value === "full" || value === "translation",
              fixHeading: value === "full" || value === "heading"
            });
          }}
        >
          <Tooltip title="Pose Waypoint">
            <ToggleButton
              value="full"
              aria-label="Full waypoint"
              disabled={disabled}
            >
              <WaypointIcon />
            </ToggleButton>
          </Tooltip>
          <Tooltip title="Translation Waypoint">
            <ToggleButton
              value="translation"
              aria-label="Translation waypoint"
              disabled={disabled}
            >
              <Circle />
            </ToggleButton>
          </Tooltip>
          <Tooltip title="Heading Waypoint">
            <ToggleButton
              value="heading"
              aria-label="Heading waypoint"
              disabled={disabled}
            >
              <NearMe />
            </ToggleButton>
          </Tooltip>
          <Tooltip title="Empty Waypoint">
            <ToggleButton
              value="empty"
              aria-label="Empty waypoint"
              disabled={disabled}
            >
              <CircleOutlined />
            </ToggleButton>
          </Tooltip>
        </ToggleButtonGroup>
        <div className={styles.indexBadge}>{index + 1}</div>
        <Tooltip title="Delete waypoint">
          <span>
            <IconButton
              aria-label={`Delete waypoint ${index + 1}`}
              disabled={disabled}
              size="small"
              onClick={onDelete}
            >
              <DeleteOutline />
            </IconButton>
          </span>
        </Tooltip>
      </div>
    </div>
  );
}

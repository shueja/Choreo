import {
  ArrowCircleDown,
  Circle,
  CircleOutlined,
  NearMe,
  Speed,
  Sync
} from "@mui/icons-material";
import { ToggleButton, ToggleButtonGroup, Tooltip } from "@mui/material";
import WaypointIcon from "../../assets/Waypoint";
import { ServerWaypointType } from "../field/ServerViewLayers";
import { ServerConstraintType } from "../field/ServerViewLayers";
import styles from "./ServerNavbar.module.css";

interface Props {
  waypointType: ServerWaypointType;
  constraintType: ServerConstraintType | null;
  pending: boolean;
  connected: boolean;
  onWaypointTypeChange: (type: ServerWaypointType) => void;
  onConstraintTypeChange: (type: ServerConstraintType | null) => void;
}

const waypointTools: {
  type: ServerWaypointType;
  label: string;
  icon: React.ReactNode;
}[] = [
  { type: "full", label: "Full Waypoint", icon: <WaypointIcon /> },
  { type: "translation", label: "Translation Waypoint", icon: <Circle /> },
  { type: "heading", label: "Heading Waypoint", icon: <NearMe /> },
  { type: "empty", label: "Empty Waypoint", icon: <CircleOutlined /> }
];

const constraintTools: {
  type: ServerConstraintType;
  label: string;
  icon: React.ReactNode;
}[] = [
  { type: "MaxLinearVelocity", label: "Max Linear Velocity", icon: <Speed /> },
  { type: "MaxAngularVelocity", label: "Max Angular Velocity", icon: <Sync /> },
  { type: "KeepInCircle", label: "Keep In Circle", icon: <ArrowCircleDown /> },
  { type: "Heading", label: "Heading Constraint", icon: <NearMe /> }
];

export default function ServerNavbar({
  waypointType,
  constraintType,
  pending,
  connected,
  onWaypointTypeChange,
  onConstraintTypeChange
}: Props) {
  return (
    <div className={styles.navbar}>
      <ToggleButtonGroup
        exclusive
        size="small"
        value={waypointType}
        onChange={(_event, value: ServerWaypointType | null) => {
          if (value !== null) {
            onWaypointTypeChange(value);
          }
        }}
      >
        {waypointTools.map((tool) => (
          <Tooltip key={tool.type} title={tool.label}>
            <ToggleButton
              value={tool.type}
              aria-label={tool.label}
              disabled={!connected || pending}
            >
              {tool.icon}
            </ToggleButton>
          </Tooltip>
        ))}
      </ToggleButtonGroup>
      <ToggleButtonGroup
        exclusive
        size="small"
        value={constraintType ?? ""}
        onChange={(_event, value: ServerConstraintType | "" | null) =>
          onConstraintTypeChange(value === "" ? null : value)
        }
      >
        {constraintTools.map((tool) => (
          <Tooltip key={tool.type} title={tool.label}>
            <ToggleButton
              value={tool.type}
              aria-label={tool.label}
              disabled={!connected || pending}
            >
              {tool.icon}
            </ToggleButton>
          </Tooltip>
        ))}
      </ToggleButtonGroup>
    </div>
  );
}

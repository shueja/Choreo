import {
  Grid4x4,
  Route,
  ScatterPlot,
  SquareOutlined,
  Visibility,
  ZoomIn,
  ZoomOut
} from "@mui/icons-material";
import { Checkbox, IconButton, Menu, MenuItem, Tooltip } from "@mui/material";
import { useState } from "react";
import WaypointIcon from "../../assets/Waypoint";
import { ServerViewLayers } from "./ServerViewLayers";
import styles from "./ServerFieldControls.module.css";

interface Props {
  visibleLayers: ServerViewLayers;
  pending: boolean;
  connected: boolean;
  onToggleLayer: (layer: keyof ServerViewLayers) => void;
  onGenerate: () => void;
}

export default function ServerFieldControls({
  visibleLayers,
  pending,
  connected,
  onToggleLayer,
  onGenerate
}: Props) {
  const [viewAnchor, setViewAnchor] = useState<HTMLElement | null>(null);
  const viewItems: {
    key: keyof ServerViewLayers;
    label: string;
    icon: React.ReactNode;
  }[] = [
    { key: "field", label: "Field", icon: <SquareOutlined /> },
    { key: "grid", label: "Grid", icon: <Grid4x4 /> },
    { key: "trajectory", label: "Trajectory", icon: <Route /> },
    { key: "samples", label: "Samples", icon: <ScatterPlot /> },
    { key: "waypoints", label: "Waypoints", icon: <WaypointIcon /> }
  ];

  return (
    <div className={styles.controls}>
      <div className={styles.viewOptions}>
        <Tooltip title="Zoom out">
          <IconButton
            aria-label="Zoom out"
            onClick={() => window.dispatchEvent(new Event("zoomOut"))}
          >
            <ZoomOut />
          </IconButton>
        </Tooltip>
        <Tooltip title="Zoom in">
          <IconButton
            aria-label="Zoom in"
            onClick={() => window.dispatchEvent(new Event("zoomIn"))}
          >
            <ZoomIn />
          </IconButton>
        </Tooltip>
        <Tooltip title="View layers">
          <IconButton
            aria-label="View layers"
            onClick={(event) => setViewAnchor(event.currentTarget)}
          >
            <Visibility />
          </IconButton>
        </Tooltip>
        <Menu
          anchorEl={viewAnchor}
          open={viewAnchor !== null}
          onClose={() => setViewAnchor(null)}
        >
          {viewItems.map((item) => (
            <MenuItem key={item.key} onClick={() => onToggleLayer(item.key)}>
              <Checkbox checked={visibleLayers[item.key]} size="small" />
              <span className={styles.viewIcon}>{item.icon}</span>
              {item.label}
            </MenuItem>
          ))}
        </Menu>
      </div>
      <Tooltip title="Generate trajectory">
        <span className={styles.generate}>
          <IconButton
            aria-label="Generate trajectory"
            color="primary"
            disabled={pending || !connected}
            onClick={onGenerate}
          >
            <Route />
          </IconButton>
        </span>
      </Tooltip>
    </div>
  );
}

import Waypoint from "@choreo/icons/Waypoint";
import {
  Circle,
  CircleOutlined,
  CropFree,
  Grid4x4,
  Room,
  Route,
  ScatterPlot,
  SquareOutlined
} from "@mui/icons-material";

/* ViewOptionsPanel items */
export const ViewData = {
  Field: {
    index: 0,
    name: "Field",
    icon: (
      <SquareOutlined style={{ transform: "scale(1.2, 0.6)" }}></SquareOutlined>
    ),
    default: true
  },
  Grid: {
    index: 1,
    name: "Grid",
    icon: <Grid4x4 />,
    default: false
  },
  Trajectory: {
    index: 2,
    name: "Trajectory",
    icon: <Route />,
    default: true
  },
  Samples: {
    index: 3,
    name: "Samples",
    icon: <ScatterPlot />,
    default: false
  },
  Waypoints: {
    index: 4,
    name: "Waypoints",
    icon: <Waypoint />,
    default: true
  },
  Focus: {
    index: 5,
    name: "Focus",
    icon: <CropFree />,
    default: false
  }
};

export const ViewLayers = (() => {
  const x: { [key: string]: number } = {};
  Object.entries(ViewData).forEach(([key, _data], index) => {
    x[key] = index;
  });
  return x;
})();

export const ViewItemData = (() => {
  const x: Array<{ name: string; icon: any; default: boolean }> = [];
  Object.entries(ViewData).forEach(([_key, data], _index) => {
    x[data.index] = { name: data.name, icon: data.icon, default: data.default };
  });
  return x;
})();
export const ViewLayerDefaults = ViewItemData.map((layer) => layer.default);
export type ViewLayerType = typeof ViewLayers;
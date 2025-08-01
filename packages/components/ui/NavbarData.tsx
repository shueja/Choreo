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
import { ReactElement } from "react";
import Waypoint from "@choreo/icons/Waypoint";
import {
  ConstraintDefinition,
  ConstraintDefinitions,
  ConstraintKey
} from "@choreo/document/constraint/ConstraintDefinitions";
import { ConstraintStore } from "@choreo/stores/ConstraintStore";

/* Navbar stuff */
export const WaypointData: {
  [key: string]: {
    index: number;
    name: string;
    icon: ReactElement;
  };
} = {
  FullWaypoint: {
    index: 0,
    name: "Pose Waypoint",
    icon: <Waypoint />
  },
  TranslationWaypoint: {
    index: 1,
    name: "Translation Waypoint",
    icon: <Circle />
  },
  EmptyWaypoint: {
    index: 2,
    name: "Empty Waypoint",
    icon: <CircleOutlined />
  }
};
export const NavbarData: {
  [key: string]: {
    index: number;
    name: string;
    icon: ReactElement;
  };
} = Object.assign({}, WaypointData);
const waypointNavbarCount = Object.keys(NavbarData).length;
const constraintsIndices: number[] = [];
export const navbarIndexToConstraint: {
  [key: number]: typeof ConstraintStore;
} = {};
export const navbarIndexToConstraintDefinition: {
  [key: number]: ConstraintDefinition<any>;
} = {};
export const navbarIndexToConstraintKey: {
  [key: number]: ConstraintKey;
} = {};
{
  let constraintsOffset = Object.keys(NavbarData).length;
  Object.entries(ConstraintDefinitions).forEach(([key, data], _index) => {
    NavbarData[key] = {
      index: constraintsOffset,
      name: data.name,
      icon: <Circle></Circle>
    };
    navbarIndexToConstraintDefinition[constraintsOffset] = data;
    navbarIndexToConstraintKey[constraintsOffset] = key as ConstraintKey;
    constraintsIndices.push(constraintsOffset);
    constraintsOffset++;
  });
}
const constraintNavbarCount = Object.keys(ConstraintDefinitions).length;

const eventMarkerCount = 1;
NavbarData.EventMarker = {
  index: Object.keys(NavbarData).length,
  name: "Event Marker",
  icon: <Room></Room>
};

/** An map of  */
export const NavbarLabels = (() => {
  const x: { [key: string]: number } = {};
  Object.entries(NavbarData).forEach(([key, _data], index) => {
    x[key] = index;
  });
  return x;
})();

/** An array of name-and-icon objects for the navbar */
export const NavbarItemData = (() => {
  const x: Array<{ name: string; icon: any }> = [];
  Object.entries(NavbarData).forEach(([_key, data], _index) => {
    x[data.index] = { name: data.name, icon: data.icon };
  });
  return x;
})();

const NavbarItemSections = [waypointNavbarCount, constraintNavbarCount];
NavbarItemSections.push(eventMarkerCount);

export const NavbarItemSectionEnds = NavbarItemSections.map((_s, idx) =>
  NavbarItemSections.slice(0, idx + 1).reduce((prev, cur) => prev + cur, -1)
);






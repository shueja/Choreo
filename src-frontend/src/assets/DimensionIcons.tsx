import { JSX } from "react"
import { DimensionNameExt, DimensionNamesExt } from "../../lib/types/math/Dimensions"
import {
  KeyboardArrowRight,
  KeyboardDoubleArrowRight,
  Numbers,
  RotateLeftOutlined,
  Straighten,
  SyncOutlined,
  TimerOutlined
} from "@mui/icons-material";
import Angle from "./Angle";
import Mass from "./Mass";
import MoI from "./MoI";
import Torque from "./Torque";
import WaypointIcon from "./Waypoint";
export default {
  Number: () => <Numbers></Numbers>,
  Length:() => <Straighten></Straighten>,
  Angle: () => <Angle></Angle>,
  LinVel:  () => <KeyboardArrowRight />,
  LinAcc:() => <KeyboardDoubleArrowRight />,
  AngVel: () => <SyncOutlined />,
  AngAcc: () => <RotateLeftOutlined />,
  Time: () => <TimerOutlined></TimerOutlined>,
  Mass: () => <Mass></Mass>,
  MoI: () => <MoI></MoI>,
  Torque: () => <Torque></Torque>,
  Pose: ()=><WaypointIcon></WaypointIcon>
} as const satisfies {
    [key in DimensionNameExt]: ()=>JSX.Element
}
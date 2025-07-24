export const DimensionNames = [
  "Number",
  "Length",
  "LinVel",
  "LinAcc",
  "Angle",
  "AngVel",
  "AngAcc",
  "Time",
  "Mass",
  "Torque",
  "MoI"
] as const;
export type DimensionName = (typeof DimensionNames)[number];

export const DimensionDescriptions = {

    Number: "Number",
    Length: "Length",
    Angle: "Angle",
    LinVel: "Linear Velocity",
    LinAcc: "Linear Acceleration",
    AngVel: "Angular Velocity",
    AngAcc: "Angular Acceleration",
    Time: "Time",
    Mass: "Mass",
    Torque: "Torque",
    MoI: "Moment of Inertia"

} as const satisfies {[key in DimensionName]: string;}


export const DimensionNamesExt = [...DimensionNames, "Pose"] as const;
export type DimensionNameExt = (typeof DimensionNamesExt)[number];

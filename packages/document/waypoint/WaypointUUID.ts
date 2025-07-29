export type WaypointUUID = "first" | "last" | {uuid: string};

export function waypointIDToText(
  id: WaypointUUID | undefined,
  points: {uuid:string}[]
) {
  if (id == undefined) return "?";
  if (id == "first") return "Start";
  if (id == "last") return "End";
  return points.findIndex(pt=>pt.uuid === id.uuid) + 1;
}
// All Sample types have a timestamp and a pose in meters.
export interface Sample {
    t: number,
    x: number,
    y: number,
    heading: number
}
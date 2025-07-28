// All Sample types have a timestamp and a pose in meters.
export default interface Sample {
    t: number,
    x: number,
    y: number,
    heading: number
}
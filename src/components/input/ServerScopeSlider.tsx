import { Slider } from "@mui/material";

interface Props {
  waypointCount: number;
  isRange: boolean;
  value: number | [number, number];
  disabled?: boolean;
  onChange: (selection: number | [number, number]) => void;
}

export default function ServerScopeSlider({
  waypointCount,
  isRange,
  value,
  disabled = false,
  onChange
}: Props) {
  const marks = [
    { value: 0, label: "Start" },
    ...Array.from({ length: waypointCount }, (_unused, index) => ({
      value: index + 1,
      label: index + 1
    })),
    { value: waypointCount + 1, label: "End" }
  ];

  return (
    <div style={{ marginInline: "4ch" }}>
      <Slider
        sx={{
          '& .MuiSlider-markLabel[data-index="0"]': {
            transform: "translateX(-3.5ch)"
          },
          [`& .MuiSlider-markLabel[data-index="${waypointCount + 1}"]`]: {
            transform: "translateX(0ch)"
          }
        }}
        step={null}
        min={0}
        max={waypointCount + 1}
        value={value}
        marks={marks}
        track={isRange ? "normal" : false}
        disabled={disabled || waypointCount === 0}
        onChange={(_event, next) => {
          if (typeof next === "number") {
            onChange(next);
          } else {
            onChange([next[0], next[1]]);
          }
        }}
      />
    </div>
  );
}

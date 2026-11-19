interface Point {
  x: number;
  y: number;
}

interface Props {
  points: Point[];
  stroke?: string;
}

export default function FieldPathLine({ points, stroke = "grey" }: Props) {
  const path = points.map(({ x, y }) => `${x}, ${y}`).join(" ");
  return (
    <polyline
      points={path}
      stroke={stroke}
      strokeWidth={0.05}
      fill="transparent"
      style={{ pointerEvents: "none" }}
    />
  );
}

interface Point {
  x: number;
  y: number;
}

interface Props {
  points: Point[];
}

export default function FieldSampleLayer({ points }: Props) {
  return (
    <>
      {points.map((point, index) => (
        <circle cx={point.x} cy={point.y} r={0.02} fill="black" key={index} />
      ))}
    </>
  );
}

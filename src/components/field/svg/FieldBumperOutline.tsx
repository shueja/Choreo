interface Point {
  x: number;
  y: number;
}

interface Props {
  path?: string;
  points?: Point[];
  strokeColor: string;
  strokeWidth: number;
}

export default function FieldBumperOutline({
  path,
  points,
  strokeColor,
  strokeWidth
}: Props) {
  const sharedProps = {
    fill: "transparent",
    stroke: strokeColor,
    strokeWidth,
    strokeLinecap: "square" as const,
    vectorEffect: "non-scaling-stroke" as const,
    style: { pointerEvents: "none" as const }
  };

  if (path !== undefined) {
    return <path d={path} {...sharedProps} />;
  }

  return (
    <polygon
      points={points?.map(({ x, y }) => `${x},${y}`).join(" ") ?? ""}
      {...sharedProps}
    />
  );
}

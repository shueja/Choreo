const DRAW_BOUND = 100;
const GRID_STROKE = 0.01;

export default function FieldGridLayer() {
  return (
    <>
      <defs>
        <pattern id="grid" width="1" height="1" patternUnits="userSpaceOnUse">
          <path
            d="M 1 0 L 0 0 0 1"
            fill="none"
            stroke="silver"
            strokeWidth={GRID_STROKE}
          />
        </pattern>
      </defs>
      <circle
        cx={0}
        cy={0}
        r={DRAW_BOUND}
        fill="url(#grid)"
        style={{ pointerEvents: "none" }}
      />
    </>
  );
}

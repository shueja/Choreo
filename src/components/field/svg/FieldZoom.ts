import * as d3 from "d3";

interface CenterDetail {
  x: number;
  y: number;
  k: number;
}

export function attachFieldZoom(
  svg: SVGSVGElement,
  onZoom: (transform: d3.ZoomTransform) => void
): () => void {
  const behavior = d3
    .zoom<SVGSVGElement, undefined>()
    .scaleExtent([0.3, 12])
    .on("zoom", (event) => onZoom(event.transform));
  const selection = d3.select<SVGSVGElement, undefined>(svg);
  const transition = () => d3.transition().duration(750).ease(d3.easeCubicOut);
  const center = (event: Event) => {
    const { x, y, k } = (event as CustomEvent<CenterDetail>).detail;
    selection.call(behavior.scaleTo, k);
    selection.transition(transition()).call(behavior.translateTo, x, -y);
  };
  const zoomIn = () =>
    selection.transition(transition()).call(behavior.scaleBy, 2);
  const zoomOut = () =>
    selection.transition(transition()).call(behavior.scaleBy, 0.5);

  selection.call(behavior).on("dblclick.zoom", null);
  window.addEventListener("center", center);
  window.addEventListener("zoomIn", zoomIn);
  window.addEventListener("zoomOut", zoomOut);

  return () => {
    selection.interrupt();
    selection.on(".zoom", null);
    window.removeEventListener("center", center);
    window.removeEventListener("zoomIn", zoomIn);
    window.removeEventListener("zoomOut", zoomOut);
  };
}

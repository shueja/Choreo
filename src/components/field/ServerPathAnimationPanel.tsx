import Pause from "@mui/icons-material/Pause";
import PlayArrow from "@mui/icons-material/PlayArrow";
import { IconButton, Slider, Tooltip } from "@mui/material";
import { useEffect, useRef, useState } from "react";
import { TrajectorySample } from "../../document/ServerSessionStore";
import styles from "./ServerPathAnimationPanel.module.css";

interface Props {
  samples: TrajectorySample[];
  timestamp: number;
  onChange: (timestamp: number) => void;
}

export default function ServerPathAnimationPanel({
  samples,
  timestamp,
  onChange
}: Props) {
  const [playing, setPlaying] = useState(false);
  const timestampRef = useRef(timestamp);
  const onChangeRef = useRef(onChange);
  const duration = samples.at(-1)?.time ?? 0;

  useEffect(() => {
    timestampRef.current = timestamp;
  }, [timestamp]);

  useEffect(() => {
    onChangeRef.current = onChange;
  }, [onChange]);

  useEffect(() => {
    if (!playing || samples.length < 2 || duration <= 0) {
      return;
    }

    let frame = 0;
    const startTime = performance.now() - timestampRef.current * 1000;
    const tick = (now: number) => {
      const next = Math.min(duration, (now - startTime) / 1000);
      onChangeRef.current(next);
      if (next >= duration) {
        setPlaying(false);
      } else {
        frame = requestAnimationFrame(tick);
      }
    };
    frame = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(frame);
  }, [playing, duration, samples.length]);

  return (
    <div className={styles.panel}>
      <Tooltip title={playing ? "Pause path animation" : "Play path animation"}>
        <span>
          <IconButton
            aria-label={
              playing ? "Pause path animation" : "Play path animation"
            }
            size="small"
            disabled={samples.length < 2}
            onClick={() => setPlaying((current) => !current)}
          >
            {playing ? (
              <Pause fontSize="small" />
            ) : (
              <PlayArrow fontSize="small" />
            )}
          </IconButton>
        </span>
      </Tooltip>
      <Slider
        aria-label="Path animation time"
        min={0}
        max={Math.max(duration, 0.01)}
        step={0.01}
        value={Math.min(timestamp, duration)}
        disabled={samples.length < 2}
        onChange={(_event, value) => {
          setPlaying(false);
          onChange(typeof value === "number" ? value : value[0]);
        }}
      />
      <span className={styles.time}>
        {timestamp.toFixed(2)} / {duration.toFixed(2)} s
      </span>
    </div>
  );
}

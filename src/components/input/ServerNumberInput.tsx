import { Checkbox, Tooltip } from "@mui/material";
import { useState } from "react";
import styles from "./InputList.module.css";

interface Props {
  title: string;
  value: number;
  enabled: boolean;
  suffix?: string;
  roundingPrecision?: number;
  showCheckbox?: boolean;
  showNumberWhenDisabled?: boolean;
  checkboxEnabled?: boolean;
  titleTooltip?: string;
  maxWidthCharacters?: number;
  onChange: (value: number) => void;
  onEnable?: (enabled: boolean) => void;
}

export default function ServerNumberInput({
  title,
  value,
  enabled,
  suffix = "",
  roundingPrecision = 3,
  showCheckbox = false,
  showNumberWhenDisabled = true,
  checkboxEnabled = true,
  titleTooltip,
  maxWidthCharacters,
  onChange,
  onEnable
}: Props) {
  const [focused, setFocused] = useState(false);
  const [editing, setEditing] = useState(false);
  const [editedValue, setEditedValue] = useState("");
  const rounded = (
    Math.round(value * 10 ** roundingPrecision) /
    10 ** roundingPrecision
  ).toFixed(roundingPrecision);
  const display = editing ? editedValue : focused ? String(value) : rounded;
  const width = Math.min(rounded.length + 3, maxWidthCharacters ?? 48);

  return (
    <>
      <Tooltip disableInteractive title={titleTooltip ?? ""}>
        <span
          className={`${styles.Title} ${enabled ? "" : styles.Disabled} ${titleTooltip ? styles.Tooltip : ""}`}
        >
          {title}
        </span>
      </Tooltip>
      <input
        aria-label={title}
        className={`${styles.Number} ${showNumberWhenDisabled ? styles.ShowWhenDisabled : ""}`}
        style={{ maxWidth: `${width}ch` }}
        type="text"
        disabled={!enabled}
        value={display}
        onFocus={() => {
          setFocused(true);
          setEditing(false);
          setEditedValue(String(value));
        }}
        onChange={(event) => {
          setEditing(true);
          setEditedValue(event.target.value);
        }}
        onBlur={() => {
          const next = Number.parseFloat(editedValue);
          if (editing && Number.isFinite(next)) {
            onChange(next);
          }
          setFocused(false);
          setEditing(false);
        }}
        onKeyDown={(event) => {
          if (event.key === "Enter") {
            event.currentTarget.blur();
          }
          if (event.key === "Escape") {
            setFocused(false);
            setEditing(false);
            event.currentTarget.blur();
          }
        }}
        onClick={(event) => event.stopPropagation()}
        autoComplete="off"
        autoCorrect="off"
        autoCapitalize="off"
      />
      <span className={`${styles.Suffix} ${enabled ? "" : styles.Disabled}`}>
        {suffix}
      </span>
      {showCheckbox ? (
        <Checkbox
          className={styles.Checkbox}
          checked={enabled}
          disabled={!checkboxEnabled}
          onClick={(event) => event.stopPropagation()}
          onChange={(event) => onEnable?.(event.target.checked)}
        />
      ) : (
        <span />
      )}
    </>
  );
}

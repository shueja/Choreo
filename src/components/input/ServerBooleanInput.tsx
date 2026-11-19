import { Checkbox, Tooltip } from "@mui/material";
import styles from "./InputList.module.css";

interface Props {
  title: string;
  value: boolean;
  enabled?: boolean;
  titleTooltip?: string;
  onChange: (value: boolean) => void;
}

export default function ServerBooleanInput({
  title,
  value,
  enabled = true,
  titleTooltip,
  onChange
}: Props) {
  return (
    <>
      <Tooltip disableInteractive title={titleTooltip ?? ""}>
        <span
          className={`${styles.Title} ${enabled ? "" : styles.Disabled} ${titleTooltip ? styles.Tooltip : ""}`}
        >
          {title}
        </span>
      </Tooltip>
      <Checkbox
        className={styles.Checkbox}
        checked={value}
        disabled={!enabled}
        onClick={(event) => event.stopPropagation()}
        onChange={(event) => onChange(event.target.checked)}
      />
    </>
  );
}

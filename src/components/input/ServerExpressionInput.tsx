import { Tooltip } from "@mui/material";
import { parse } from "mathjs";
import { useEffect, useRef, useState } from "react";
import styles from "./InputList.module.css";

export interface ServerExpression<Value = unknown> {
  exp: string;
  val: Value;
}

interface Props<Value> {
  title: string;
  value: ServerExpression<Value>;
  enabled?: boolean;
  titleTooltip?: string;
  maxWidthCharacters?: number;
  hideTitle?: boolean;
  ariaLabel?: string;
  stretch?: boolean;
  onCommit: (expression: ServerExpression<Value>) => void;
}

export default function ServerExpressionInput<Value>({
  title,
  value,
  enabled = true,
  titleTooltip,
  maxWidthCharacters,
  hideTitle = false,
  ariaLabel,
  stretch = false,
  onCommit
}: Props<Value>) {
  const [text, setText] = useState(value.exp);
  const [isValid, setIsValid] = useState(true);
  const inputRef = useRef<HTMLInputElement | null>(null);

  useEffect(() => {
    setText(value.exp);
    setIsValid(true);
  }, [value.exp]);

  function validate(source: string): boolean {
    try {
      parse(source);
      return source.trim().length > 0;
    } catch {
      return false;
    }
  }

  function commit() {
    const valid = validate(text);
    setIsValid(valid);
    if (valid && text.trim() !== value.exp) {
      onCommit({ ...value, exp: text.trim() });
    }
  }

  const width = Math.min(text.length + 3, maxWidthCharacters ?? 48);

  return (
    <>
      {!hideTitle && (
        <Tooltip disableInteractive title={titleTooltip ?? ""}>
          <span
            className={`${styles.Title} ${enabled ? "" : styles.Disabled} ${titleTooltip ? styles.Tooltip : ""}`}
          >
            {title}
          </span>
        </Tooltip>
      )}
      <input
        ref={inputRef}
        aria-label={ariaLabel ?? title}
        type="text"
        className={`${styles.Number} ${styles.ShowWhenDisabled} ${isValid ? "" : styles.Invalid}`}
        style={
          stretch ? { width: "100%", minWidth: 0 } : { minWidth: `${width}ch` }
        }
        disabled={!enabled}
        value={text}
        autoComplete="off"
        autoCorrect="off"
        autoCapitalize="off"
        spellCheck={false}
        onChange={(event) => {
          const next = event.target.value;
          setText(next);
          setIsValid(validate(next));
        }}
        onBlur={commit}
        onKeyDown={(event) => {
          if (event.key === "Enter") {
            inputRef.current?.blur();
          }
          if (event.key === "Escape") {
            setText(value.exp);
            setIsValid(true);
            inputRef.current?.blur();
          }
        }}
        onClick={(event) => event.stopPropagation()}
      />
    </>
  );
}

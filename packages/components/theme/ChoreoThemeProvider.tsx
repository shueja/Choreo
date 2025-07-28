import { ThemeOptions, ThemeProvider, createTheme } from "@mui/material/styles";
import CssBaseline from "@mui/material/CssBaseline";
import { OverridesStyleRules } from "@mui/material/styles/overrides";
import { ButtonClasses, Theme } from "@mui/material";
import { ReactElement, JSXElementConstructor, ReactNode, ReactPortal } from "react";

const buttonOverrides: Partial<
    OverridesStyleRules<
      keyof ButtonClasses,
      "MuiButton",
      Omit<Theme, "components">
    >
  > = {
    // Name of the slot
    root: ({ ownerState, theme }) => ({
      variants: [],
      // Some CSS
      fontSize: "1rem",
      color: "white",
      borderRadius: "10px",
      marginInline: "0.3rem",
      boxSizing: "border-box",
      backgroundColor:
        ownerState.color === "primary" && theme.palette.primary.main,
      "&:hover": {
        backgroundColor:
          ownerState.color === "primary" && theme.palette.secondary.main
      }
    })
  };
  const iconButtonOverrides: Partial<
    OverridesStyleRules<
      keyof ButtonClasses,
      "MuiIconButton",
      Omit<Theme, "components">
    >
  > = {
    // Name of the slot
    root: ({ ownerState, theme }) => ({
      variants: [],
      // Some CSS
      fontSize: "1rem",
      color: "white",
      borderRadius: "10px",
      marginInline: "0.3rem",
      boxSizing: "border-box",
      backgroundColor:
        ownerState.color === "primary" && theme.palette.primary.main,
      "&:hover": {
        backgroundColor:
          ownerState.color === "primary" && theme.palette.secondary.main
      }
    })
  };
  const checkboxOverrides = {
    root: {
      // Some CSS
      color: "white",
      width: 24,
      height: 24
    }
  };
  // theming for mui components
  const themeOptions: ThemeOptions = {
    palette: {
      mode: "dark",

      primary: { main: "rgb(125, 115, 231)" },
      secondary: { main: "rgb(95, 85, 205)" }
    },
    components: {
      // Name of the component
      MuiButton: { styleOverrides: buttonOverrides },
      MuiIconButton: { styleOverrides: iconButtonOverrides },
      MuiCheckbox: { styleOverrides: checkboxOverrides }
    }
  };

  export function ChoreoThemeProvider(props: { children: string | number | bigint | boolean | ReactElement<unknown, string | JSXElementConstructor<any>> | Iterable<ReactNode> | ReactPortal | Promise<string | number | bigint | boolean | ReactPortal | ReactElement<unknown, string | JSXElementConstructor<any>> | Iterable<ReactNode> | null | undefined> | null | undefined; }) {
    return     <CssBaseline><ThemeProvider theme={createTheme(themeOptions)}>{props.children}</ThemeProvider></CssBaseline>
  }
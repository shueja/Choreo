import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import path from "path";
// https://vitejs.dev/config/
export default defineConfig(async () => ({
  plugins: [react()],
  build: {
    minify:false,
    lib: {
      entry: "./apps/ext_core/extension.ts",
      fileName: "extension",
      name: "extension",
      module: true
    },
    terserOptions: {
      compress: false,
      mangle: false,
    },

    rollupOptions: {
      external: ["vscode"]
    },

    sourcemap: true,

    outDir: "./apps/ext_core/out",
    emptyOutDir: false
  }
}));

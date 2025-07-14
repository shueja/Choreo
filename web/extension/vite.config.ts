import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import path from 'path';
// https://vitejs.dev/config/
export default defineConfig(async () => ({
  plugins: [react()],
  resolve: {
    alias: {
      "$src": path.resolve(__dirname, '../../src'),
    },
  },
  build: {

    lib: {

      entry: "./web/extension/extension.ts",
      fileName: "extension",
      name: "extension",
      module: true
    },

    rollupOptions: {

      external: ["vscode"],

    },

    sourcemap: true,

    outDir: "./web/extension/out",
    emptyOutDir: false
  },
}));

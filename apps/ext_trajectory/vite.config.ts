import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import path from "path";
// https://vitejs.dev/config/
export default defineConfig(async () => ({
  plugins: [react()],
  build: {
    manifest: true,
    rollupOptions: {
      input: {
        main: "/apps/ext_trajectory/index.html"
      },
      output: {
        manualChunks: undefined,
        entryFileNames: `assets/index.js`,
        chunkFileNames: `assets/index-chunk.js`,
        assetFileNames: `assets/[name].[ext]`
      },
      external: ["index.css"],

    },
    terserOptions: {
      compress: false,
      mangle: false,
    },
    minify: false,
    // assetsDir: "./web/extension/out/vscode-chor",

    outDir: "./apps/ext_trajectory/out/vscode-traj",
    emptyOutDir: false
  }
}));

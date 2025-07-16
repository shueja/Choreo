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
    manifest: true,
    rollupOptions: {
      input: {
        main: '/web/vscode-chor/index.html'
      },
      output: {
        manualChunks: undefined,
        entryFileNames: `assets/index.js`,
        chunkFileNames: `assets/index-chunk.js`,
        assetFileNames: `assets/[name].[ext]`
        
      },
      external: ["index.css"]
    
    },
    // assetsDir: "./web/extension/out/vscode-chor",
    
    outDir: "./web/extension/out/vscode-chor",
    emptyOutDir: false
}}));

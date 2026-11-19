import { spawn } from "node:child_process";
import { existsSync } from "node:fs";
import { resolve } from "node:path";
import { createServer } from "vite";

const binaryName =
  process.platform === "win32" ? "choreo_server.exe" : "choreo_server";
const localBinary = resolve("cpp/choreo_app/build-clangcl", binaryName);
const binary = process.env.CHOREO_SERVER_BIN
  ? resolve(process.env.CHOREO_SERVER_BIN)
  : existsSync(localBinary)
    ? localBinary
    : binaryName;

async function main() {
  const vite = await createServer({
    mode: "choreo-server",
    server: { host: "127.0.0.1" }
  });
  try {
    await vite.listen();
    const child = spawn(
      binary,
      ["--web-root", "cpp/choreo_app/dev-web", ...process.argv.slice(2)],
      { stdio: "inherit" }
    );
    let shutdownCode;
    const stop = (signal) => {
      shutdownCode ??= signal === "SIGINT" ? 130 : 143;
      child.kill();
    };
    process.on("SIGINT", stop);
    process.on("SIGTERM", stop);
    try {
      const code = await new Promise((resolveExit, rejectExit) => {
        child.once("error", rejectExit);
        child.once("close", resolveExit);
      });
      process.exitCode = shutdownCode ?? code ?? 1;
    } finally {
      process.off("SIGINT", stop);
      process.off("SIGTERM", stop);
    }
  } finally {
    await vite.close();
  }
}

try {
  await main();
} catch (error) {
  console.error(`Choreo development server: ${error.message}`);
  if (error.code === "ENOENT" && error.path === binary) {
    console.error(
      "Build choreo_server or set CHOREO_SERVER_BIN to its executable path."
    );
  }
  process.exitCode = 1;
}

// Build a gpui2 example, dispatching to the toolchain for this machine:
// cmd/build-windows.ts (MSVC) on Windows, cmd/build-linux.ts (g++/clang++)
// on Linux. Every flag is passed through untouched.
//
//   bun cmd/build.ts                         # print example list
//   bun cmd/build.ts -rel system_monitor
//   bun cmd/build.ts -dbg -all
//
// To build the other platform's binaries from here, see cmd/wsl-run.ts.

import { dirname, join, resolve } from "node:path";

const root = resolve(dirname(Bun.main), "..");
process.chdir(root);

const script = process.platform === "win32" ? "cmd/build-windows.ts" : "cmd/build-linux.ts";

if (process.platform !== "win32" && process.platform !== "linux") {
  console.error(`Unsupported platform: ${process.platform}. gpui2 builds on Windows and Linux.`);
  process.exit(1);
}

const r = Bun.spawnSync(["bun", join(root, script), ...Bun.argv.slice(2)], {
  cwd: root,
  stdout: "inherit",
  stderr: "inherit",
});
process.exit(r.exitCode ?? 1);

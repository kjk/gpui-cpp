// Build and launch a gpui2 example, dispatching to the runner for this
// machine: cmd/run-windows.ts on Windows, cmd/run-linux.ts on Linux. Every
// flag is passed through untouched, so the platform-specific ones
// (-windbg on Windows, -gdb on Linux) reach the right script.
//
//   bun cmd/run.ts                         # print example list
//   bun cmd/run.ts -rel system_monitor
//   bun cmd/run.ts -dbg hello_world
//
// To run the Linux build from a Windows checkout, use cmd/wsl-run.ts.

import { dirname, join, resolve } from "node:path";

const root = resolve(dirname(Bun.main), "..");
process.chdir(root);

if (process.platform !== "win32" && process.platform !== "linux") {
  console.error(`Unsupported platform: ${process.platform}. gpui2 runs on Windows and Linux.`);
  process.exit(1);
}

const script = process.platform === "win32" ? "cmd/run-windows.ts" : "cmd/run-linux.ts";

const r = Bun.spawnSync(["bun", join(root, script), ...Bun.argv.slice(2)], {
  cwd: root,
  stdout: "inherit",
  stderr: "inherit",
});
process.exit(r.exitCode ?? 1);

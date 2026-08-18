// Build a gpui2 example, dispatching to the toolchain for this machine:
// cmd/build-windows.ts (MSVC), cmd/build-linux.ts (g++/clang++) or
// cmd/build-mac.ts (clang++). Every flag is passed through untouched.
//
//   bun cmd/build.ts                         # print example list
//   bun cmd/build.ts -rel system_monitor
//   bun cmd/build.ts -dbg -all
//
// To build another platform's binaries from here, see cmd/wsl-run.ts (Linux)
// and cmd/mac-build.ts (macOS).

import { dirname, join, resolve } from "node:path";

const root = resolve(dirname(Bun.main), "..");
process.chdir(root);

const scripts: Record<string, string> = {
  win32: "cmd/build-windows.ts",
  linux: "cmd/build-linux.ts",
  darwin: "cmd/build-mac.ts",
};

const script = scripts[process.platform];
if (!script) {
  console.error(`Unsupported platform: ${process.platform}. gpui2 builds on Windows, Linux and macOS.`);
  process.exit(1);
}

const r = Bun.spawnSync(["bun", join(root, script), ...Bun.argv.slice(2)], {
  cwd: root,
  stdout: "inherit",
  stderr: "inherit",
});
process.exit(r.exitCode ?? 1);

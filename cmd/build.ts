// Build a gpui2 example, dispatching to the toolchain for this machine:
// cmd/build-windows.ts (MSVC), cmd/build-linux.ts (g++/clang++) or
// cmd/build-mac.ts (clang++). Every flag is passed through untouched.
//
//   bun cmd/build.ts                         # print example list
//   bun cmd/build.ts -rel system_monitor
//   bun cmd/build.ts -dbg -all
//   bun cmd/build.ts -wasm hello_world       # emscripten, on any host
//
// -wasm is the one target that does not depend on the host: emscripten runs
// everywhere, so it is asked for by name rather than picked by platform. To
// build another *native* platform's binaries from here, see cmd/wsl-run.ts
// (Linux) and cmd/mac-build.ts (macOS).

import { dirname, join, resolve } from "node:path";

const root = resolve(dirname(Bun.main), "..");
process.chdir(root);

const scripts: Record<string, string> = {
  win32: "cmd/build-windows.ts",
  linux: "cmd/build-linux.ts",
  darwin: "cmd/build-mac.ts",
};

// -wasm is a target, not a host: emscripten builds the same page from
// Windows, Linux or macOS, so it is chosen by the flag and never by
// process.platform. The flag goes through to the script with everything else,
// which parses and ignores it.
const args = Bun.argv.slice(2);
const script = args.includes("-wasm") ? "cmd/build-wasm.ts" : scripts[process.platform];
if (!script) {
  console.error(`Unsupported platform: ${process.platform}. gpui2 builds on Windows, Linux and macOS.`);
  process.exit(1);
}

const r = Bun.spawnSync(["bun", join(root, script), ...args], {
  cwd: root,
  stdout: "inherit",
  stderr: "inherit",
});
process.exit(r.exitCode ?? 1);

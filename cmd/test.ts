// Build and run the test suite: bun cmd/test.ts [-dbg|-rel] [-asan] [-clean]
//
// The tests live in tests/ and are ports of the pure-logic ones in
// .work/gpui-component at the SHA in cmd/versions.ts. The runner is an
// ordinary build target, so every flag build.ts takes works here too; the
// binary prints its own report and exits nonzero on the first failure.

import { dirname, join, resolve } from "node:path";

const root = resolve(dirname(Bun.main), "..");
process.chdir(root);

const argv = Bun.argv.slice(2);
let debug = false;
let asan = false;
for (const a of argv) {
  if (a === "-dbg" || a === "--dbg") {
    debug = true;
  } else if (a === "-rel" || a === "--rel") {
    debug = false;
  } else if (a === "-asan" || a === "--asan") {
    asan = true;
  }
}

const build = Bun.spawnSync(["bun", join("cmd", "build.ts"), ...argv, "tests"], {
  cwd: root,
  stdout: "inherit",
  stderr: "inherit",
});
if (build.exitCode !== 0) {
  process.exit(build.exitCode ?? 1);
}

// Same layout rule the build scripts use: Linux and macOS keep their own tree
// so a checkout built for more than one platform does not collide.
const base = debug ? "dbg" : "rel";
const cfg = asan ? `${base}_asan` : base;
const outDir =
  process.platform === "win32" ? join("out", cfg) : join("out", process.platform === "darwin" ? "mac" : "linux", cfg);
const exe = join(root, outDir, process.platform === "win32" ? "tests.exe" : "tests");

const run = Bun.spawnSync([exe], { cwd: join(root, outDir), stdout: "inherit", stderr: "inherit" });
process.exit(run.exitCode ?? 1);

// Build and run the layout benchmarks: bun cmd/bench.ts [-dbg|-rel] [-small]
//                                                       [-large] [-n=<count>]
//                                                       [<filter>]
//
// The benchmarks live in bench/. The layout ones are ports of taffy's
// benches/ directory, which is a crate of its own and not part of the
// published crate — see port-upstream.md for the checkout. The markdown ones
// are ours: markdown-rs carries none to port. The runner is an ordinary build
// target, so every flag build.ts takes works here too.
//
//   bun cmd/bench.ts markdown     # just the parser, after changing it
//
// Release is the default and the only setting worth reading a number from; a
// debug build measures the assertions, not the layout.

import { dirname, join, resolve } from "node:path";

const root = resolve(dirname(Bun.main), "..");
process.chdir(root);

const argv = Bun.argv.slice(2);
let debug = false;
let asan = false;
// Flags the build script knows about; everything else goes to the binary.
const buildFlags: string[] = [];
const runFlags: string[] = [];
for (const a of argv) {
  if (a === "-dbg" || a === "--dbg") {
    debug = true;
    buildFlags.push(a);
  } else if (a === "-rel" || a === "--rel") {
    debug = false;
    buildFlags.push(a);
  } else if (a === "-asan" || a === "--asan") {
    asan = true;
    buildFlags.push(a);
  } else if (a === "-clean" || a === "--clean") {
    buildFlags.push(a);
  } else {
    runFlags.push(a);
  }
}

const build = Bun.spawnSync(["bun", join("cmd", "build.ts"), ...buildFlags, "bench"], {
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
const exe = join(root, outDir, process.platform === "win32" ? "bench.exe" : "bench");

const run = Bun.spawnSync([exe, ...runFlags], { cwd: join(root, outDir), stdout: "inherit", stderr: "inherit" });
process.exit(run.exitCode ?? 1);

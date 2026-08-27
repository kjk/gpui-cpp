// Build hello_world against the library's individual src/**/*.cpp files.
// This is a deliberately small smoke test for ordinary header hygiene; the
// normal build continues to use the generated .work/gpui.cpp amalgam.

import { build, checkBuildFlags, defaultBuildFlags, platformFor, takeBuildFlag } from "./build.ts";

function die(msg: string): never {
  console.error(msg);
  process.exit(1);
}

const flags = defaultBuildFlags();
flags.nonAmalgam = true;
for (const arg of Bun.argv.slice(2)) {
  if (!arg.startsWith("-")) {
    die(`Unexpected argument: ${arg}`);
  }
  if (!takeBuildFlag(arg, flags)) {
    die(`Unknown flag: ${arg}`);
  }
}

const plat = platformFor(flags, die);
checkBuildFlags(flags, plat, die);
await build({ names: ["hello_world"], plat, flags, fail: die });

// Where emscripten is, and which node it ships. Its own module because both
// cmd/build-wasm.ts and cmd/run-wasm.ts need it and neither may import the
// other: they are scripts, and importing one runs it.
//
// Emscripten is found through $EMCC, then $EMSDK, then a sibling emsdk
// checkout, then PATH. It is em++ rather than emcc: the link needs the C++
// runtime and emcc leaves it out. If none of those has it:
//
//   git clone https://github.com/emscripten-core/emsdk ../.emsdk
//   cd ../.emsdk && ./emsdk install latest && ./emsdk activate latest

import { existsSync, readdirSync, statSync } from "node:fs";
import { dirname, join, resolve } from "node:path";

const root = resolve(dirname(Bun.main), "..");

export type Emcc = { exe: string; env: Record<string, string> };

function isFile(p: string): boolean {
  try {
    return statSync(p).isFile();
  } catch {
    return false;
  }
}

function firstExisting(...cands: string[]): string | null {
  for (const c of cands) {
    if (c && isFile(c)) {
      return c;
    }
  }
  return null;
}

function whichEmcc(): string | null {
  const finder = process.platform === "win32" ? "where" : "which";
  const r = Bun.spawnSync([finder, "em++"], { stdout: "pipe", stderr: "pipe" });
  if ((r.exitCode ?? 1) !== 0) {
    return null;
  }
  const out = new TextDecoder().decode(r.stdout).trim();
  const first = out.split(/\r?\n/)[0]?.trim();
  return first && first.length > 0 ? first : null;
}

// The emsdk root a checkout could plausibly be at: $EMSDK, then .emsdk beside
// this repo, then one inside it.
function emsdkRoots(): string[] {
  const roots: string[] = [];
  if (process.env["EMSDK"]) {
    roots.push(process.env["EMSDK"]!);
  }
  roots.push(resolve(root, "..", ".emsdk"));
  roots.push(resolve(root, "..", "emsdk"));
  roots.push(join(root, ".emsdk"));
  return roots;
}

export function findEmcc(): Emcc {
  const env: Record<string, string> = {};
  const fromEnv = process.env["EMCC"];
  if (fromEnv && isFile(fromEnv)) {
    return { exe: fromEnv, env };
  }
  for (const sdk of emsdkRoots()) {
    const em = join(sdk, "upstream", "emscripten");
    const exe = firstExisting(join(em, "em++.exe"), join(em, "em++"));
    if (!exe) {
      continue;
    }
    // An emsdk that was activated without --permanent leaves its config in
    // the checkout rather than in the environment; point at it explicitly so
    // this works from any shell.
    const cfg = join(sdk, ".emscripten");
    if (isFile(cfg)) {
      // Forward slashes: emscripten stamps the config's directory into its
      // cache sanity file, and a backslash spelling looks like a different
      // SDK to the next run, which then clears and rebuilds the cache.
      env["EM_CONFIG"] = cfg.replaceAll("\\", "/");
    }
    return { exe, env };
  }
  const onPath = whichEmcc();
  if (onPath) {
    return { exe: onPath, env };
  }
  console.error("No emscripten found. Set $EMCC or $EMSDK, or install it:");
  console.error("  git clone https://github.com/emscripten-core/emsdk ../.emsdk");
  console.error("  cd ../.emsdk && ./emsdk install latest && ./emsdk activate latest");
  process.exit(1);
}

// The node emsdk installed beside the compiler, which is the one the modules
// it builds were tested against. Falls back to whatever is on PATH.
export function emsdkNode(emcc: Emcc): string {
  const sdk = resolve(dirname(emcc.exe), "..", "..");
  const nodeDir = join(sdk, "node");
  if (!existsSync(nodeDir)) {
    return "node";
  }
  let found = "node";
  for (const d of readdirSync(nodeDir)) {
    // Windows puts node at the top of the version directory; the POSIX
    // packages put it under bin/.
    for (const cand of [join(nodeDir, d, "node.exe"), join(nodeDir, d, "bin", "node")]) {
      if (existsSync(cand)) {
        found = cand;
      }
    }
  }
  return found;
}

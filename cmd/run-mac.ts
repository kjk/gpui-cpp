// Build then launch a gpui2 example as a detached Cocoa process (does not
// wait). The macOS counterpart of cmd/run-windows.ts; reached through
// cmd/run.ts. Same flags as build.ts, plus -lldb / -compare.
//   bun cmd/run.ts                         # print example list
//   bun cmd/run.ts app_assets
//   bun cmd/run.ts -dbg hello_world
//   bun cmd/run.ts -rel -lldb showcase
//   bun cmd/run.ts -rel -compare hello_world

import { existsSync, mkdirSync, statSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";
import { printSizeTable } from "./sizes.ts";
import { ensureRustTree, rustTreeDir } from "./versions.ts";

const root = resolve(dirname(Bun.main), "..");
process.chdir(root);

const simpleExamples = [
  "hello_world",
  "window_title",
  "root_borderless",
  "dialog_overlay",
  "focus_trap",
  "fps_monitor",
  "input",
  "sidebar",
  "tooltip_top_edge",
  "table_in_scrollable",
  "text_selection",
  "markdown_table",
  "rich_text",
  "stream_markdown",
  "markdown",
  "html",
  "large_text",
  "dock",
  "tiles",
  "brush",
  "editor",
];

const knownTargets = ["system_monitor", "app_assets", "showcase", "story", ...simpleExamples] as const;
type Target = (typeof knownTargets)[number];

const usage = `Usage: bun cmd/run.ts [-rel|-dbg] [-asan] [-clean] [-lldb] [-compare] <example>

  -rel      release (default)
  -dbg      debug
  -asan     AddressSanitizer; combines with -rel or -dbg
  -clean    delete out/<dir>/ before building
  -lldb     run the binary under lldb instead of detaching it
  -compare  also cargo-build and launch the Rust example from .work/gpui-component
            (cloned at the SHA in cmd/versions.ts if missing); prints both
            binary sizes, then launches Rust left and C++ right

Builds with cmd/build.ts, then launches out/mac/<cfg>/<target> and exits.
The example name is the last argument.

Run this from any terminal in a logged-in macOS desktop session, including
Wave Terminal. Plain ssh sessions normally have no window server.`;

function printExamples(msg?: string): never {
  if (msg) {
    console.error(msg);
    console.error("");
  }
  console.error(usage);
  console.error("");
  console.error("Examples:");
  for (const n of knownTargets) {
    console.error(`  ${n}`);
  }
  process.exit(1);
}

function die(msg?: string): never {
  if (msg) {
    console.error(msg);
  }
  console.error(usage);
  process.exit(1);
}

function parseArgs(argv: string[]): {
  target: Target;
  debug: boolean;
  asan: boolean;
  clean: boolean;
  lldb: boolean;
  compare: boolean;
} {
  if (argv.length === 0 || argv[argv.length - 1]!.startsWith("-")) {
    printExamples();
  }
  const last = argv[argv.length - 1]!;
  const name = last.toLowerCase();
  if (!(knownTargets as readonly string[]).includes(name)) {
    printExamples(`Unknown example: ${last}`);
  }
  const target = name as Target;

  let sawRel = false;
  let sawDbg = false;
  let asan = false;
  let clean = false;
  let lldb = false;
  let compare = false;
  for (const raw of argv.slice(0, -1)) {
    if (raw === "-rel") {
      sawRel = true;
      continue;
    }
    if (raw === "-dbg") {
      sawDbg = true;
      continue;
    }
    if (raw === "-asan") {
      asan = true;
      continue;
    }
    if (raw === "-clean") {
      clean = true;
      continue;
    }
    if (raw === "-lldb") {
      lldb = true;
      continue;
    }
    if (raw === "-compare") {
      compare = true;
      continue;
    }
    if (raw.startsWith("-")) {
      die(`Unknown flag: ${raw}`);
    }
    die(`Unknown argument: ${raw}`);
  }
  if (sawRel && sawDbg) {
    die("Cannot combine -rel and -dbg");
  }
  return { target, debug: sawDbg, asan, clean, lldb, compare };
}

// Matches cmd/build-mac.ts: the macOS tree is its own so a build of the same
// checkout for another OS survives.
function outDirName(debug: boolean, asan: boolean): string {
  const base = debug ? "dbg" : "rel";
  return join("mac", asan ? `${base}_asan` : base);
}

// Repo-relative if it lives here (the rust tree is under .work/), absolute
// otherwise. Keeps the size table readable.
function repoPath(p: string): string {
  const rel = relative(root, p);
  return rel.startsWith("..") ? p : rel;
}

function which(name: string): string | null {
  const r = Bun.spawnSync(["which", name], { stdout: "pipe", stderr: "pipe" });
  if ((r.exitCode ?? 1) !== 0) {
    return null;
  }
  const out = new TextDecoder().decode(r.stdout).trim();
  return out.length > 0 ? out.split("\n")[0]! : null;
}

function run(cmd: string[], cwd: string): number {
  const r = Bun.spawnSync(cmd, { cwd, stdout: "inherit", stderr: "inherit" });
  return r.exitCode ?? 1;
}

// A detached, unreferenced process outlives this script and the shell that
// started it. Bun handles the new process group; macOS does not ship setsid.
function launchDetached(cmd: string[], cwd: string, env?: Record<string, string>): ReturnType<typeof Bun.spawn> {
  const proc = Bun.spawn(cmd, {
    cwd,
    env,
    stdin: "ignore",
    stdout: "ignore",
    stderr: "ignore",
    detached: true,
  });
  proc.unref();
  return proc;
}

// Neither GPUI implementation exposes its NSWindow through macOS
// Accessibility. Inject this tiny Cocoa shim into each locally-built process
// so it can place its own first window when it becomes visible.
function ensureCompareWindowPlacer(): string {
  const source = join(root, "cmd/mac-window-place.m");
  const outputDir = join(root, ".work/mac-window-place");
  const output = join(outputDir, "mac-window-place.dylib");
  const needsBuild = !existsSync(output) || statSync(source).mtimeMs > statSync(output).mtimeMs;
  if (!needsBuild) {
    return output;
  }

  mkdirSync(outputDir, { recursive: true });
  console.log("Building macOS compare window placer");
  const exit = run(["xcrun", "clang", "-fobjc-arc", "-dynamiclib", "-framework", "Cocoa", "-o", output, source], root);
  if (exit !== 0 || !existsSync(output)) {
    die("Could not build the macOS compare window placer. Install the command line tools: xcode-select --install");
  }
  return output;
}

function compareWindowEnv(placer: string, half: "left" | "right"): Record<string, string> {
  const env: Record<string, string> = {};
  for (const [name, value] of Object.entries(process.env)) {
    if (value !== undefined) {
      env[name] = value;
    }
  }
  const oldInsert = env["DYLD_INSERT_LIBRARIES"];
  env["DYLD_INSERT_LIBRARIES"] = oldInsert ? `${placer}:${oldInsert}` : placer;
  env["GPUI_COMPARE_WINDOW_HALF"] = half;
  return env;
}

function findCargo(): string | null {
  const fromPath = which("cargo");
  if (fromPath) {
    return fromPath;
  }
  const home = process.env["HOME"] ?? "";
  const local = join(home, ".cargo", "bin", "cargo");
  return existsSync(local) ? local : null;
}

function rustBuildArgs(target: Target, debug: boolean): string[] {
  const prof = debug ? [] : ["--release"];
  if (target === "showcase") {
    return ["build", ...prof, "-p", "gpui-base", "--example", "base_components"];
  }
  if (target === "story") {
    return ["build", ...prof, "-p", "gpui-component-story"];
  }
  return ["build", ...prof, "-p", target];
}

function rustExePath(target: Target, debug: boolean): string {
  const prof = debug ? "debug" : "release";
  const dir = rustTreeDir(root);
  if (target === "showcase") {
    return join(dir, "target", prof, "examples", "base_components");
  }
  if (target === "story") {
    return join(dir, "target", prof, "gpui-component-story");
  }
  return join(dir, "target", prof, target);
}

const { target, debug, asan, clean, lldb, compare } = parseArgs(Bun.argv.slice(2));

const buildArgs: string[] = [debug ? "-dbg" : "-rel"];
if (asan) {
  buildArgs.push("-asan");
}
if (clean) {
  buildArgs.push("-clean");
}
buildArgs.push(target);

const buildExit = run(["bun", join(root, "cmd/build.ts"), ...buildArgs], root);
if (buildExit !== 0) {
  process.exit(buildExit);
}

const outDir = join("out", outDirName(debug, asan));
const exe = join(root, outDir, target);
if (!existsSync(exe)) {
  die(`Missing ${outDir}/${target} after build`);
}

let rustExe: string | null = null;
if (compare) {
  let rustRoot: string;
  try {
    rustRoot = ensureRustTree(root);
  } catch (e) {
    die(e instanceof Error ? e.message : String(e));
  }
  const cargo = findCargo();
  if (!cargo) {
    die(`Cannot use -compare because Cargo was not found.

Wave Terminal is supported; this error is unrelated to the macOS window server.

Install Rust and try again:
  brew install rust
  bun cmd/run.ts ${debug ? "-dbg" : "-rel"} -compare ${target}

To launch only the C++ port, omit -compare:
  bun cmd/run.ts ${debug ? "-dbg" : "-rel"} ${target}`);
  }
  const cargoArgs = rustBuildArgs(target, debug);
  console.log(`Building rust: cargo ${cargoArgs.join(" ")}`);
  const rustBuildExit = run([cargo, ...cargoArgs], rustRoot);
  if (rustBuildExit !== 0) {
    process.exit(rustBuildExit);
  }
  rustExe = rustExePath(target, debug);
  if (!existsSync(rustExe)) {
    die(`Missing rust binary after cargo build: ${rustExe}`);
  }
  console.log("");
  printSizeTable([
    { label: repoPath(exe), path: exe },
    { label: repoPath(rustExe), path: rustExe },
  ]);
  console.log("");
}

const cwd = join(root, outDir);
if (lldb) {
  if (!which("lldb")) {
    die("lldb is not installed. Install the command line tools: xcode-select --install");
  }
  // Foreground, so the prompt lands in this terminal.
  console.log(`Launching lldb ${exe}`);
  process.exit(run(["lldb", "-o", "run", "--", exe], cwd));
}

if (rustExe) {
  const placer = ensureCompareWindowPlacer();
  console.log(`Launching rust ${rustExe} (left)`);
  launchDetached([rustExe], rustTreeDir(root), compareWindowEnv(placer, "left"));
  console.log(`Launching ${exe} (right)`);
  launchDetached([exe], cwd, compareWindowEnv(placer, "right"));
} else {
  console.log(`Launching ${exe}`);
  launchDetached([exe], cwd);
}
process.exit(0);

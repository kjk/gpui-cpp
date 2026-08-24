// Build then launch a gpui2 example as a detached X11 process (does not wait).
// The Linux counterpart of cmd/run-windows.ts; reached through cmd/run.ts.
// Same flags as build.ts, plus -gdb / -compare.
//   bun cmd/run.ts                         # print example list
//   bun cmd/run.ts app_assets
//   bun cmd/run.ts -dbg hello_world
//   bun cmd/run.ts -rel -gdb showcase
//   bun cmd/run.ts -rel -compare hello_world

import { existsSync } from "node:fs";
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
];

const knownTargets = ["system_monitor", "app_assets", "showcase", "story", ...simpleExamples] as const;
type Target = (typeof knownTargets)[number];

const usage = `Usage: bun cmd/run.ts [-rel|-dbg] [-asan] [-clean] [-gdb] [-compare] <example>

  -rel      release (default)
  -dbg      debug
  -asan     AddressSanitizer; combines with -rel or -dbg
  -clean    delete out/<dir>/ before building
  -gdb      run the binary under gdb instead of detaching it
  -compare  also cargo-build and launch the Rust example from .work/gpui-component
            (cloned at the SHA in cmd/versions.ts if missing); prints both
            binary sizes, then launches both

Builds with cmd/build.ts, then launches out/<dir>/<target> and exits.
The example name is the last argument.

Needs an X display. Under WSL that is WSLg, which sets DISPLAY for you;
over SSH use X forwarding.`;

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
  gdb: boolean;
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
  let gdb = false;
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
    if (raw === "-gdb") {
      gdb = true;
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
  return { target, debug: sawDbg, asan, clean, gdb, compare };
}

// out/linux/, not out/: a checkout is often built for both platforms (see
// cmd/wsl-run.ts) and the two must not overwrite each other's binaries,
// object files or -clean.
function outDirName(debug: boolean, asan: boolean): string {
  const base = debug ? "dbg" : "rel";
  return join("linux", asan ? `${base}_asan` : base);
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

// setsid, so the app outlives this script and the shell that started it.
function launchDetached(cmd: string[], cwd: string): ReturnType<typeof Bun.spawn> {
  const proc = Bun.spawn(["setsid", ...cmd], {
    cwd,
    stdin: "ignore",
    stdout: "ignore",
    stderr: "ignore",
    detached: true,
  });
  proc.unref();
  return proc;
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

const { target, debug, asan, clean, gdb, compare } = parseArgs(Bun.argv.slice(2));

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

if (!process.env["DISPLAY"] && !process.env["WAYLAND_DISPLAY"]) {
  console.error("DISPLAY is not set: there is no X server to open a window on.");
  console.error("Under WSL, make sure WSLg is available (wsl --update).");
  process.exit(1);
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
    die("Could not find cargo. Install Rust (https://rustup.rs) and ensure cargo is on PATH.");
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
if (gdb) {
  if (!which("gdb")) {
    die("gdb is not installed. Run: bash cmd/ubuntu-install-deps.sh");
  }
  // Foreground, so the prompt lands in this terminal.
  console.log(`Launching gdb ${exe}`);
  process.exit(run(["gdb", "-q", "-ex", "run", "--args", exe], cwd));
}

console.log(`Launching ${exe}`);
launchDetached([exe], cwd);
if (rustExe) {
  console.log(`Launching rust ${rustExe}`);
  launchDetached([rustExe], rustTreeDir(root));
}
process.exit(0);

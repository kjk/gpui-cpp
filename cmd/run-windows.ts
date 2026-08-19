// Build then launch a gpui2 example as a detached Windows process (does not wait).
// Same flags as build.ts (always amalgamates .work/gpui.h + .work/gpui.cpp),
// plus -windbg / -compare.
//   bun cmd/run.ts                         # print example list
//   bun cmd/run.ts app_assets
//   bun cmd/run.ts -dbg hello_world
//   bun cmd/run.ts -rel -windbg showcase
//   bun cmd/run.ts -rel -compare hello_world
//   bun cmd/run.ts -rel -asan system_monitor
//   bun cmd/run.ts -rel -compare story     # rust left half, ours right half

import { existsSync, lstatSync, readdirSync, statSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";
import {
  findVisibleClassWindows,
  placeOnWorkAreaHalf,
  setProcessDpiAware,
  waitForNewClassWindow,
  waitForPidWindow,
} from "./winapi.ts";
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
];

const knownTargets = ["system_monitor", "app_assets", "showcase", "story", ...simpleExamples] as const;
type Target = (typeof knownTargets)[number];

const usage = `Usage: bun cmd/run.ts [-rel|-dbg] [-asan] [-clean] [-windbg] [-compare] <example>

  -rel      release (default)
  -dbg      debug
  -asan     AddressSanitizer; combines with -rel or -dbg
  -clean    delete out/<dir>/ before building
  -windbg   run the C++ exe under windbgx.exe (-G, ignore first-chance C++ EH)
  -compare  also cargo-build and launch the Rust example from .work/gpui-component
            (cloned at the SHA in cmd/versions.ts if missing)
            prints both exe sizes, then rust on the left half of the screen,
            ours on the right

Builds with cmd/build.ts, then launches out/<dir>/<target>.exe and exits.
The example name is the last argument.`;

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
  windbg: boolean;
  compare: boolean;
} {
  if (argv.length === 0 || argv[argv.length - 1].startsWith("-")) {
    printExamples();
  }
  const last = argv[argv.length - 1];
  const name = last.toLowerCase();
  if (!(knownTargets as readonly string[]).includes(name)) {
    printExamples(`Unknown example: ${last}`);
  }
  const target = name as Target;

  let sawRel = false;
  let sawDbg = false;
  let asan = false;
  let clean = false;
  let windbg = false;
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
    if (raw === "-windbg") {
      windbg = true;
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
  return { target, debug: sawDbg, asan, clean, windbg, compare };
}

function outDirName(debug: boolean, asan: boolean): string {
  const base = debug ? "dbg" : "rel";
  return asan ? `${base}_asan` : base;
}

// Repo-relative if it lives here (the rust tree is under .work/), absolute
// otherwise. Keeps the size table readable.
function repoPath(p: string): string {
  const rel = relative(root, p);
  return rel.startsWith("..") ? p : rel.replaceAll("/", "\\");
}

function firstWhereLine(stdout: Uint8Array): string | null {
  const first = new TextDecoder()
    .decode(stdout)
    .split(/\r?\n/)
    .find((l) => l.trim().length > 0);
  return first ? first.trim() : null;
}

function pathLooksPresent(p: string): boolean {
  try {
    lstatSync(p);
    return true;
  } catch {
    return false;
  }
}

function whereExe(name: string): string | null {
  const r = Bun.spawnSync(["where", name], { stdout: "pipe", stderr: "pipe" });
  if (r.exitCode !== 0) {
    return null;
  }
  return firstWhereLine(r.stdout);
}

// Store/WinGet WinDbgX.exe is a 0-byte execution alias. Bun's spawn stats
// the path and fails with ENOENT. DbgX.Shell.exe in the Appx package is real.
function isSpawnableExe(p: string): boolean {
  try {
    const st = statSync(p);
    return st.isFile() && st.size > 0;
  } catch {
    return false;
  }
}

function appxWindbgDir(): string | null {
  const r = Bun.spawnSync(
    [
      "powershell.exe",
      "-NoProfile",
      "-NonInteractive",
      "-Command",
      "(Get-AppxPackage -Name Microsoft.WinDbg | Select-Object -First 1).InstallLocation",
    ],
    { stdout: "pipe", stderr: "pipe" },
  );
  if (r.exitCode !== 0) {
    return null;
  }
  const dir = new TextDecoder().decode(r.stdout).trim();
  return dir.length > 0 ? dir : null;
}

function windbgInDir(dir: string): string | null {
  for (const name of ["DbgX.Shell.exe", "WinDbgX.exe", "windbgx.exe"]) {
    const p = join(dir, name);
    if (isSpawnableExe(p)) {
      return p;
    }
  }
  return null;
}

function findUnderWindowsApps(): string | null {
  const apps = join(process.env["ProgramFiles"] ?? "C:\\Program Files", "WindowsApps");
  if (!existsSync(apps)) {
    return null;
  }
  let ents: string[];
  try {
    ents = readdirSync(apps);
  } catch {
    return null;
  }
  for (const ent of ents) {
    if (!ent.toLowerCase().startsWith("microsoft.windbg")) {
      continue;
    }
    const found = windbgInDir(join(apps, ent));
    if (found) {
      return found;
    }
  }
  return null;
}

function findWindbgx(): string | null {
  const pkg = appxWindbgDir();
  if (pkg) {
    const found = windbgInDir(pkg);
    if (found) {
      return found;
    }
  }

  const under = findUnderWindowsApps();
  if (under) {
    return under;
  }

  const fromPath = whereExe("WinDbgX") ?? whereExe("windbgx") ?? whereExe("WinDbgX.exe") ?? whereExe("windbgx.exe");
  if (fromPath && isSpawnableExe(fromPath)) {
    return fromPath;
  }

  const local = process.env["LOCALAPPDATA"] ?? "";
  const candidates = [
    "C:\\Debugger\\windbgx.exe",
    join(local, "Microsoft", "WindowsApps", "WinDbgX.exe"),
    join(local, "Microsoft", "WindowsApps", "windbgx.exe"),
  ];
  for (const p of candidates) {
    if (p && isSpawnableExe(p)) {
      return p;
    }
  }
  // Alias path: not spawnable by Bun, but cmd.exe can resolve it.
  if (fromPath && pathLooksPresent(fromPath)) {
    return fromPath;
  }
  for (const p of candidates) {
    if (p && pathLooksPresent(p)) {
      return p;
    }
  }
  return null;
}

function run(cmd: string[], cwd: string): number {
  const r = Bun.spawnSync(cmd, {
    cwd,
    stdout: "inherit",
    stderr: "inherit",
  });
  return r.exitCode ?? 1;
}

function launchDetached(cmd: string[], cwd: string): ReturnType<typeof Bun.spawn> {
  const proc = Bun.spawn(cmd, {
    cwd,
    stdin: "ignore",
    stdout: "ignore",
    stderr: "ignore",
    detached: true,
  });
  proc.unref();
  return proc;
}

// All gpui2 apps share this WNDCLASS (src/gpui/window_common.cpp). WinDbg's UI does not.
const cppWndClass = "Gpui2SystemMonitor";

async function placeHwnd(hwnd: number, side: "left" | "right", label: string): Promise<void> {
  if (!hwnd) {
    console.error(`${label} window did not appear`);
    return;
  }
  if (!placeOnWorkAreaHalf(hwnd, side)) {
    console.error(`Failed to place ${label} window on the ${side} half`);
  }
}

async function placeLaunched(proc: ReturnType<typeof Bun.spawn>, side: "left" | "right", label: string): Promise<void> {
  const pid = proc.pid ?? 0;
  const hwnd = await waitForPidWindow(pid, 45000);
  if (!hwnd) {
    console.error(`${label} window did not appear (pid ${pid})`);
    return;
  }
  await placeHwnd(hwnd, side, label);
}

function rustDir(): string {
  return rustTreeDir(root);
}

function findCargo(): string | null {
  const fromPath = whereExe("cargo") ?? whereExe("cargo.exe");
  if (fromPath && isSpawnableExe(fromPath)) {
    return fromPath;
  }
  const home = process.env["USERPROFILE"] ?? process.env["HOME"] ?? "";
  const local = join(home, ".cargo", "bin", "cargo.exe");
  if (isSpawnableExe(local)) {
    return local;
  }
  return fromPath;
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
  if (target === "showcase") {
    return join(rustDir(), "target", prof, "examples", "base_components.exe");
  }
  if (target === "story") {
    return join(rustDir(), "target", prof, "gpui-component-story.exe");
  }
  return join(rustDir(), "target", prof, `${target}.exe`);
}

const { target, debug, asan, clean, windbg, compare } = parseArgs(Bun.argv.slice(2));

const buildArgs: string[] = [];
if (debug) {
  buildArgs.push("-dbg");
} else {
  buildArgs.push("-rel");
}
if (asan) {
  buildArgs.push("-asan");
}
if (clean) {
  buildArgs.push("-clean");
}
buildArgs.push(target);

const buildExit = run(["bun", "cmd/build.ts", ...buildArgs], root);
if (buildExit !== 0) {
  process.exit(buildExit);
}

const outDir = join("out", outDirName(debug, asan));
const exe = join(root, outDir, `${target}.exe`);
if (!existsSync(exe)) {
  die(`Missing ${outDir}\\${target}.exe after build`);
}

function cppCmd(): { cmd: string[]; cwd: string } {
  if (!windbg) {
    return { cmd: [exe], cwd: join(root, outDir) };
  }
  const dbg = findWindbgx();
  if (!dbg) {
    console.error(
      "Could not find windbgx.exe. Install WinDbg (winget install Microsoft.WinDbg) and ensure WinDbgX is on PATH.",
    );
    process.exit(1);
  }
  // -c at the initial break: sxd eh = second-chance only for C++ EH
  // (Windows/COM/DWrite throw-and-catch e06d7363 a lot). Then g.
  // -G still ignores the process-exit breakpoint. Do not use -g here —
  // it skips the initial break and -c would never run.
  if (!isSpawnableExe(dbg)) {
    return { cmd: ["cmd.exe", "/c", `"${dbg}" -c "sxd eh; g" -G "${exe}"`], cwd: join(root, outDir) };
  }
  return { cmd: [dbg, "-c", "sxd eh; g", "-G", exe], cwd: join(root, outDir) };
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
    die(`Missing rust exe after cargo build: ${rustExe}`);
  }
  // Both binaries are built now; how big each one came out is the first thing
  // a comparison run wants to know.
  console.log("");
  printSizeTable([
    { label: repoPath(exe), path: exe },
    { label: repoPath(rustExe), path: rustExe },
  ]);
  console.log("");
}

const cpp = cppCmd();
console.log(`Launching ${cpp.cmd.join(" ")}`);

if (!compare || !rustExe) {
  launchDetached(cpp.cmd, cpp.cwd);
  process.exit(0);
}

setProcessDpiAware();
const existingCpp = new Set(findVisibleClassWindows(cppWndClass));
console.log(`Launching rust ${rustExe}`);
const rustProc = launchDetached([rustExe], rustDir());
console.log(`Launching c++ (will wait for ${cppWndClass})`);
launchDetached(cpp.cmd, cpp.cwd);
const cppWaitMs = windbg ? 120000 : 45000;
await Promise.all([
  placeLaunched(rustProc, "left", "rust"),
  (async () => {
    const hwnd = await waitForNewClassWindow(cppWndClass, existingCpp, cppWaitMs);
    if (!hwnd) {
      console.error(`c++ window did not appear (${cppWndClass}, waited ${cppWaitMs}ms)`);
      return;
    }
    await placeHwnd(hwnd, "right", "c++");
  })(),
]);
process.exit(0);

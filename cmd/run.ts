// Build a gpui example and launch it — as a detached desktop process, under
// a debugger, or, with -wasm, as a page off a local server.
//
//   bun cmd/run.ts                         # print example list
//   bun cmd/run.ts app_assets
//   bun cmd/run.ts -dbg hello_world
//   bun cmd/run.ts -rel -asan system_monitor
//   bun cmd/run.ts -debugger showcase      # windbg / lldb / gdb, whichever is here
//   bun cmd/run.ts -windbg showcase        # force one (also -cdb, -gdb, -lldb)
//   bun cmd/run.ts -rel -compare story     # rust left half, ours right half
//   bun cmd/run.ts -wasm story             # build, serve, open a tab
//
// The build is cmd/build.ts, imported rather than spawned, so -rel / -dbg /
// -asan / -clang / -clean / -wasm mean exactly what they mean there and land
// in the same out/ directory. -dbg is the debug *build*; -debugger is what
// runs it under a debugger.
//
// Nothing is downloaded that the run does not need: the Rust spec tree under
// .work/ is cloned only for -compare, and emscripten is only looked for with
// -wasm.
//
// To run the Linux build from a Windows checkout, use cmd/wsl-run.ts.

import { existsSync, lstatSync, mkdirSync, readdirSync, statSync } from "node:fs";
import { extname, join, relative } from "node:path";
import {
  build,
  checkBuildFlags,
  consoleTargets,
  defaultBuildFlags,
  emsdkNode,
  examplesFor,
  findEmcc,
  isKnownTarget,
  outDir,
  outFilePath,
  platformFor,
  printSizeTable,
  root,
  takeBuildFlag,
  type BuildFlags,
  type Platform,
} from "./build.ts";
import { ensureRustTree, rustTreeDir } from "./versions.ts";

// ─── command line ─────────────────────────────────────────────────────────

/** Named on the command line to force one debugger; null means "any". */
type DebuggerKind = "windbg" | "cdb" | "gdb" | "lldb";

const usage = `Usage: bun cmd/run.ts [-rel|-dbg] [-asan] [-clang] [-wasm] [-clean]
                     [-debugger|-windbg|-cdb|-gdb|-lldb] [-compare]
                     [-no-build] [-no-open] [-port N] <example>

  -rel        release (default)
  -dbg        debug build (this is the build, not the debugger)
  -asan       AddressSanitizer; combines with -rel or -dbg
  -clang      Windows: build with clang-cl instead of cl.exe
  -clean      delete out/<dir>/ before building
  -no-build   launch what is already in out/, without compiling

  -debugger   run under whichever debugger this machine has
              Windows: windbg, then cdb.  Linux: gdb, then lldb.  macOS: lldb.
  -windbg     Windows: force WinDbg (windbgx / DbgX.Shell)
  -cdb        Windows: force cdb, the console debugger
  -gdb        Linux, macOS: force gdb
  -lldb       Linux, macOS: force lldb

  -compare    also cargo-build and launch the Rust example from
              .work/gpui-component (cloned at the SHA in cmd/versions.ts if
              missing); prints both binary sizes, then puts rust on the left
              half of the screen and ours on the right

  -wasm       build for the browser, serve out/wasm/<cfg>/ and open a tab
  -no-open    -wasm: do not launch a browser
  -port N     -wasm: listen on N (default 8000; the next free port if taken)

The example name is the last argument. -all is not accepted — pick one binary.`;

function die(msg?: string): never {
  if (msg) {
    console.error(msg);
    console.error("");
  }
  console.error(usage);
  process.exit(1);
}

function printExamples(plat: Platform, msg?: string): never {
  if (msg) {
    console.error(msg);
    console.error("");
  }
  console.error(usage);
  console.error("");
  console.error("Examples:");
  for (const n of examplesFor(plat)) {
    console.error(`  ${n}`);
  }
  process.exit(1);
}

type RunArgs = {
  target: string;
  flags: BuildFlags;
  plat: Platform;
  /** -debugger, or one of the forcing flags. */
  debugger: "any" | DebuggerKind | null;
  compare: boolean;
  noBuild: boolean;
  open: boolean;
  port: number;
};

function parseArgs(argv: string[]): RunArgs {
  const flags = defaultBuildFlags();
  let dbgr: "any" | DebuggerKind | null = null;
  let compare = false;
  let noBuild = false;
  let open = true;
  let port = 8000;
  const names: string[] = [];
  for (let i = 0; i < argv.length; i++) {
    const raw = argv[i]!;
    if (takeBuildFlag(raw, flags)) {
      continue;
    }
    switch (raw) {
      case "-debugger":
        dbgr = "any";
        continue;
      case "-windbg":
        dbgr = "windbg";
        continue;
      case "-cdb":
        dbgr = "cdb";
        continue;
      case "-gdb":
        dbgr = "gdb";
        continue;
      case "-lldb":
        dbgr = "lldb";
        continue;
      case "-compare":
        compare = true;
        continue;
      case "-no-build":
        noBuild = true;
        continue;
      case "-no-open":
        open = false;
        continue;
      case "-port":
        port = Number(argv[++i]);
        if (!Number.isFinite(port) || port <= 0) {
          die("-port wants a number");
        }
        continue;
      case "-all":
        die("cmd/run.ts launches one binary; -all is a build-only flag.");
    }
    if (raw.startsWith("-")) {
      die(`Unknown flag: ${raw}`);
    }
    names.push(raw.toLowerCase());
  }

  const plat = platformFor(flags, die);
  checkBuildFlags(flags, plat, die);
  if (names.length === 0) {
    printExamples(plat);
  }
  if (names.length !== 1) {
    die("Pass one example name");
  }
  const target = names[0]!;
  if (!isKnownTarget(target, plat)) {
    printExamples(plat, `Unknown example: ${target}`);
  }
  if (plat === "wasm") {
    if (dbgr) {
      die("A wasm build runs in a browser; debug it with the browser's own devtools.");
    }
    if (compare) {
      die("-compare launches two desktop apps side by side, which the wasm target has no way to do.");
    }
  }
  // Before the build, not after it: naming the wrong platform's debugger is a
  // typo, and a typo should not cost a compile first.
  if (dbgr && dbgr !== "any") {
    const isWinDbgr = dbgr === "windbg" || dbgr === "cdb";
    if (isWinDbgr && plat !== "win") {
      die(`-${dbgr} is a Windows debugger; this is ${process.platform}. Use -gdb or -lldb.`);
    }
    if (!isWinDbgr && plat === "win") {
      die(`-${dbgr} is not a Windows debugger. Use -windbg or -cdb.`);
    }
  }
  return { target, flags, plat, debugger: dbgr, compare, noBuild, open, port };
}

// ─── small helpers ────────────────────────────────────────────────────────

function decode(buf: Uint8Array | undefined): string {
  return buf ? new TextDecoder().decode(buf) : "";
}

function whichExe(name: string): string | null {
  const finder = process.platform === "win32" ? "where" : "which";
  const r = Bun.spawnSync([finder, name], { stdout: "pipe", stderr: "pipe" });
  if ((r.exitCode ?? 1) !== 0) {
    return null;
  }
  const first = decode(r.stdout)
    .split(/\r?\n/)
    .map((l) => l.trim())
    .find((l) => l.length > 0);
  return first && first.length > 0 ? first : null;
}

// A Store/WinGet execution alias is a 0-byte reparse point. Bun's spawn stats
// the path and fails with ENOENT, so only a real file can be spawned directly.
function isSpawnableExe(p: string): boolean {
  try {
    const st = statSync(p);
    return st.isFile() && st.size > 0;
  } catch {
    return false;
  }
}

function pathLooksPresent(p: string): boolean {
  try {
    lstatSync(p);
    return true;
  } catch {
    return false;
  }
}

function run(cmd: string[], cwd: string): number {
  const r = Bun.spawnSync(cmd, { cwd, stdout: "inherit", stderr: "inherit" });
  return r.exitCode ?? 1;
}

/** Outlives this script and the shell that started it. */
function launchDetached(cmd: string[], cwd: string, env?: Record<string, string>): ReturnType<typeof Bun.spawn> {
  // Linux has setsid for the new session; Bun's own detach is enough on
  // Windows and macOS, neither of which ships setsid.
  const argv = process.platform === "linux" ? ["setsid", ...cmd] : cmd;
  const proc = Bun.spawn(argv, {
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

/** Repo-relative if it lives here (the rust tree is under .work/), absolute otherwise. */
function repoPath(p: string): string {
  const rel = relative(root, p);
  if (rel.startsWith("..")) {
    return p;
  }
  return process.platform === "win32" ? rel.replaceAll("/", "\\") : rel;
}

// ─── debuggers ────────────────────────────────────────────────────────────

type DebugLaunch = {
  kind: DebuggerKind;
  /** The full argv that starts the target under the debugger. */
  cmd: string[];
  /** A console debugger takes over this terminal; a GUI one is detached. */
  foreground: boolean;
};

// --- Windows: windbg ------------------------------------------------------

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
  if ((r.exitCode ?? 1) !== 0) {
    return null;
  }
  const dir = decode(r.stdout).trim();
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

function findWindbg(): string | null {
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
  const fromPath = whichExe("WinDbgX") ?? whichExe("windbgx") ?? whichExe("WinDbgX.exe") ?? whichExe("windbgx.exe");
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

// --- Windows: cdb ---------------------------------------------------------

function findCdb(): string | null {
  const onPath = whichExe("cdb.exe");
  if (onPath && isSpawnableExe(onPath)) {
    return onPath;
  }
  // Debugging Tools for Windows, installed with the Windows SDK.
  for (const base of [
    process.env["ProgramFiles(x86)"] ?? "C:\\Program Files (x86)",
    process.env["ProgramFiles"] ?? "C:\\Program Files",
  ]) {
    for (const kit of ["10", "8.1"]) {
      const p = join(base, "Windows Kits", kit, "Debuggers", "x64", "cdb.exe");
      if (isSpawnableExe(p)) {
        return p;
      }
    }
  }
  return null;
}

const debuggerHelp: Record<DebuggerKind, string> = {
  windbg: "Install WinDbg:\n  winget install Microsoft.WinDbg",
  cdb:
    "cdb ships with the Debugging Tools for Windows:\n" +
    "  winget install Microsoft.WinDbg          (the modern UI, and cdb with it)\n" +
    "  or add Debugging Tools for Windows from the Windows SDK installer",
  gdb:
    process.platform === "darwin"
      ? "Install gdb:\n  brew install gdb\n" +
        "It also has to be code-signed to control another process; lldb needs none of that,\n" +
        "so prefer -lldb on macOS."
      : "Install gdb:\n  sudo apt install gdb\n  (or: bash cmd/ubuntu-install-deps.sh)",
  lldb:
    process.platform === "darwin"
      ? "lldb ships with the Xcode command line tools:\n  xcode-select --install"
      : "Install lldb:\n  sudo apt install lldb",
};

/**
 * The debugger to use. `want` is "any" for -debugger, or the one a forcing
 * flag named — and a named debugger that is not installed is an error with
 * the command that installs it, never a silent fallback to another one.
 */
function findDebugger(want: "any" | DebuggerKind, plat: Platform, exe: string): DebugLaunch {
  const order: DebuggerKind[] =
    want !== "any" ? [want] : plat === "win" ? ["windbg", "cdb"] : plat === "mac" ? ["lldb", "gdb"] : ["gdb", "lldb"];

  for (const kind of order) {
    if (kind === "windbg") {
      const dbg = findWindbg();
      if (!dbg) {
        continue;
      }
      // -c at the initial break: `sxd eh` makes C++ EH second-chance only
      // (Windows, COM and DWrite throw and catch e06d7363 constantly), then
      // `g` runs. -G still ignores the process-exit breakpoint. Do not use -g
      // here: it skips the initial break, and -c would never run.
      const cmd = isSpawnableExe(dbg)
        ? [dbg, "-c", "sxd eh; g", "-G", exe]
        : ["cmd.exe", "/c", `"${dbg}" -c "sxd eh; g" -G "${exe}"`];
      return { kind, cmd, foreground: false };
    }
    if (kind === "cdb") {
      const dbg = findCdb();
      if (!dbg) {
        continue;
      }
      // Console debugger: it owns this terminal, so it runs in the
      // foreground. Same second-chance-only rule for C++ EH.
      return { kind, cmd: [dbg, "-c", "sxd eh; g", "-G", exe], foreground: true };
    }
    if (kind === "gdb") {
      if (!whichExe("gdb")) {
        continue;
      }
      return { kind, cmd: ["gdb", "-q", "-ex", "run", "--args", exe], foreground: true };
    }
    if (!whichExe("lldb")) {
      continue;
    }
    return { kind, cmd: ["lldb", "-o", "run", "--", exe], foreground: true };
  }

  if (want !== "any") {
    die(`${want} is not installed.\n\n${debuggerHelp[want]}`);
  }
  const tried = order.join(", ");
  die(`No debugger found (looked for ${tried}).\n\n${debuggerHelp[order[0]!]}`);
  // Unreachable; keeps the checker happy about `cwd` being used.
  void cwd;
}

// ─── the Rust side of -compare ────────────────────────────────────────────

function findCargo(): string | null {
  const fromPath = whichExe(process.platform === "win32" ? "cargo.exe" : "cargo");
  if (fromPath && (process.platform !== "win32" || isSpawnableExe(fromPath))) {
    return fromPath;
  }
  const home = process.env["USERPROFILE"] ?? process.env["HOME"] ?? "";
  const local = join(home, ".cargo", "bin", process.platform === "win32" ? "cargo.exe" : "cargo");
  if (existsSync(local)) {
    return local;
  }
  return fromPath;
}

function rustBuildArgs(target: string, debug: boolean): string[] {
  const prof = debug ? [] : ["--release"];
  if (target === "showcase") {
    return ["build", ...prof, "-p", "gpui-base", "--example", "components"];
  }
  if (target === "story") {
    return ["build", ...prof, "-p", "gpui-component-story"];
  }
  return ["build", ...prof, "-p", target];
}

function rustExePath(target: string, debug: boolean): string {
  const prof = debug ? "debug" : "release";
  const dir = rustTreeDir(root);
  const ext = process.platform === "win32" ? ".exe" : "";
  if (target === "showcase") {
    return join(dir, "target", prof, "examples", `components${ext}`);
  }
  if (target === "story") {
    return join(dir, "target", prof, `gpui-component-story${ext}`);
  }
  return join(dir, "target", prof, `${target}${ext}`);
}

/** cargo-build the Rust twin and return its binary, or exit saying why not. */
function buildRustTwin(target: string, debug: boolean): string {
  let rustRoot: string;
  try {
    // The only thing in this tree that clones .work/gpui-component. A plain
    // build never needs it.
    rustRoot = ensureRustTree(root);
  } catch (e) {
    die(e instanceof Error ? e.message : String(e));
  }
  const cargo = findCargo();
  if (!cargo) {
    die(
      "Could not find cargo, so -compare has nothing to build the Rust side with.\n\n" +
        "Install Rust (https://rustup.rs), or drop -compare to launch only the C++ port.",
    );
  }
  const args = rustBuildArgs(target, debug);
  console.log(`Building rust: cargo ${args.join(" ")}`);
  const rc = run([cargo, ...args], rustRoot);
  if (rc !== 0) {
    process.exit(rc);
  }
  const exe = rustExePath(target, debug);
  if (!existsSync(exe)) {
    die(`Missing rust binary after cargo build: ${exe}`);
  }
  return exe;
}

// macOS: neither GPUI implementation exposes its NSWindow through
// Accessibility, so a tiny Cocoa shim is injected into each locally-built
// process to place its own first window when that window becomes visible.
function ensureMacWindowPlacer(): string {
  const source = join(root, "cmd/mac-window-place.m");
  const outputDir = join(root, ".work/mac-window-place");
  const output = join(outputDir, "mac-window-place.dylib");
  if (existsSync(output) && statSync(source).mtimeMs <= statSync(output).mtimeMs) {
    return output;
  }
  mkdirSync(outputDir, { recursive: true });
  console.log("Building macOS compare window placer");
  const rc = run(["xcrun", "clang", "-fobjc-arc", "-dynamiclib", "-framework", "Cocoa", "-o", output, source], root);
  if (rc !== 0 || !existsSync(output)) {
    die("Could not build the macOS compare window placer. Install the command line tools: xcode-select --install");
  }
  return output;
}

function macPlacerEnv(placer: string, half: "left" | "right"): Record<string, string> {
  const env: Record<string, string> = {};
  for (const [name, value] of Object.entries(process.env)) {
    if (value !== undefined) {
      env[name] = value;
    }
  }
  const old = env["DYLD_INSERT_LIBRARIES"];
  env["DYLD_INSERT_LIBRARIES"] = old ? `${placer}:${old}` : placer;
  env["GPUI_COMPARE_WINDOW_HALF"] = half;
  return env;
}

// ─── native run ───────────────────────────────────────────────────────────

// All gpui apps share this WNDCLASS (src/gpui/window_common.cpp). WinDbg's
// own UI does not, which is how the launched app is told apart from it.
const cppWndClass = "GpuiSystemMonitor";

async function runNative(a: RunArgs): Promise<never> {
  if (!a.noBuild) {
    build({ names: [a.target], plat: a.plat, flags: a.flags, fail: die, quiet: true });
  }
  const dir = outDir(a.plat, a.flags);
  const exe = outFilePath(a.plat, a.flags, a.target);
  if (!existsSync(exe)) {
    die(a.noBuild ? `Missing ${repoPath(exe)}. Drop -no-build to compile it.` : `Missing ${repoPath(exe)} after build`);
  }
  const cwd = join(root, dir);

  if (a.plat === "linux" && !process.env["DISPLAY"] && !process.env["WAYLAND_DISPLAY"]) {
    console.error("DISPLAY is not set: there is no X server to open a window on.");
    console.error("Under WSL, make sure WSLg is available (wsl --update).");
    process.exit(1);
  }

  const rustExe = a.compare ? buildRustTwin(a.target, a.flags.debug) : null;
  if (rustExe) {
    // Both binaries exist now; how big each came out is the first thing a
    // comparison run wants to know.
    console.log("");
    printSizeTable([
      { label: repoPath(exe), path: exe },
      { label: repoPath(rustExe), path: rustExe },
    ]);
    console.log("");
  }

  const dbg = a.debugger ? findDebugger(a.debugger, a.plat, exe) : null;
  const cppCmd = dbg ? dbg.cmd : [exe];

  if (dbg?.foreground) {
    // The debugger owns this terminal, so nothing can be placed beside it.
    if (rustExe) {
      console.log(`Launching rust ${rustExe}`);
      launchDetached([rustExe], rustTreeDir(root));
    }
    console.log(`Launching ${dbg.kind} ${exe}`);
    process.exit(run(cppCmd, cwd));
  }

  console.log(`Launching ${cppCmd.join(" ")}`);
  if (!rustExe) {
    launchDetached(cppCmd, cwd);
    process.exit(0);
  }

  if (a.plat === "win") {
    await placeWindowsPair(cppCmd, cwd, rustExe, dbg !== null);
    process.exit(0);
  }
  if (a.plat === "mac") {
    const placer = ensureMacWindowPlacer();
    console.log(`Launching rust ${rustExe} (left)`);
    launchDetached([rustExe], rustTreeDir(root), macPlacerEnv(placer, "left"));
    launchDetached(cppCmd, cwd, macPlacerEnv(placer, "right"));
    process.exit(0);
  }
  // Linux has no window placement here; the window manager decides.
  launchDetached(cppCmd, cwd);
  console.log(`Launching rust ${rustExe}`);
  launchDetached([rustExe], rustTreeDir(root));
  process.exit(0);
}

// Windows only, and imported only here: cmd/winapi.ts dlopens user32 at
// import time, which no other platform can do.
async function placeWindowsPair(cppCmd: string[], cwd: string, rustExe: string, underDebugger: boolean): Promise<void> {
  const { findVisibleClassWindows, placeOnWorkAreaHalf, setProcessDpiAware, waitForNewClassWindow, waitForPidWindow } =
    await import("./winapi.ts");

  async function place(hwnd: number, side: "left" | "right", label: string): Promise<void> {
    if (!hwnd) {
      console.error(`${label} window did not appear`);
      return;
    }
    if (!placeOnWorkAreaHalf(hwnd, side)) {
      console.error(`Failed to place ${label} window on the ${side} half`);
    }
  }

  setProcessDpiAware();
  const existingCpp = new Set(findVisibleClassWindows(cppWndClass));
  console.log(`Launching rust ${rustExe}`);
  const rustProc = launchDetached([rustExe], rustTreeDir(root));
  console.log(`Launching c++ (will wait for ${cppWndClass})`);
  launchDetached(cppCmd, cwd);
  // A debugger stops at the initial break first, so its window takes longer.
  const cppWaitMs = underDebugger ? 120000 : 45000;
  await Promise.all([
    (async () => {
      const pid = rustProc.pid ?? 0;
      const hwnd = await waitForPidWindow(pid, 45000);
      if (!hwnd) {
        console.error(`rust window did not appear (pid ${pid})`);
        return;
      }
      await place(hwnd, "left", "rust");
    })(),
    (async () => {
      const hwnd = await waitForNewClassWindow(cppWndClass, existingCpp, cppWaitMs);
      if (!hwnd) {
        console.error(`c++ window did not appear (${cppWndClass}, waited ${cppWaitMs}ms)`);
        return;
      }
      await place(hwnd, "right", "c++");
    })(),
  ]);
}

// ─── wasm run ─────────────────────────────────────────────────────────────

const mime: Record<string, string> = {
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".mjs": "text/javascript; charset=utf-8",
  ".wasm": "application/wasm",
  ".data": "application/octet-stream",
  ".json": "application/json",
  ".css": "text/css; charset=utf-8",
  ".svg": "image/svg+xml",
  ".png": "image/png",
  ".map": "application/json",
};

/**
 * A wasm module has to come off a server: a browser refuses to instantiate
 * one fetched from a file:// URL, so there is no double-clicking the .html.
 * This is that server and nothing more — no caching, no compression, no
 * directory listing.
 */
function runWasm(a: RunArgs): never {
  if (!a.noBuild) {
    build({ names: [a.target], plat: "wasm", flags: a.flags, fail: die, quiet: true });
  }
  const dir = join(root, outDir("wasm", a.flags));

  // tests and bench print and exit; there is nothing to serve. emsdk ships
  // the node they were built against, so use that one when it is there.
  if (consoleTargets.has(a.target)) {
    const js = join(dir, `${a.target}.js`);
    if (!existsSync(js)) {
      die(`${repoPath(js)} not built`);
    }
    process.exit(run([emsdkNode(findEmcc()), js], dir));
  }

  const page = join(dir, `${a.target}.html`);
  if (!existsSync(page)) {
    die(`${repoPath(page)} not built`);
  }

  function serve(p: number) {
    return Bun.serve({
      port: p,
      fetch(req) {
        let path = new URL(req.url).pathname;
        if (path === "/" || path === "") {
          path = `/${a.target}.html`;
        }
        // Everything is served out of the one output directory: no traversal
        // out of it, and nothing else on the disk is reachable.
        const abs = join(dir, path.replace(/^\/+/, ""));
        if (!abs.startsWith(dir) || !existsSync(abs) || !statSync(abs).isFile()) {
          return new Response("not found", { status: 404 });
        }
        return new Response(Bun.file(abs), {
          headers: {
            "content-type": mime[extname(abs).toLowerCase()] ?? "application/octet-stream",
            // A rebuild while the tab is open should be one reload away.
            "cache-control": "no-store",
          },
        });
      },
    });
  }

  let server: ReturnType<typeof serve> | null = null;
  for (let p = a.port; p < a.port + 20 && !server; p++) {
    try {
      server = serve(p);
    } catch {
      // In use; try the next one.
    }
  }
  if (!server) {
    die(`No free port in ${a.port}..${a.port + 19}`);
  }

  const url = `http://localhost:${server.port}/`;
  console.log(`serving ${repoPath(dir)} at ${url}`);
  console.log("ctrl-c to stop");

  if (a.open) {
    const cmd =
      process.platform === "win32"
        ? ["cmd", "/c", "start", "", url]
        : process.platform === "darwin"
          ? ["open", url]
          : ["xdg-open", url];
    Bun.spawn(cmd, { stdout: "ignore", stderr: "ignore" });
  }
  // Bun.serve keeps the process alive; this never returns.
  return undefined as never;
}

// ─── main ─────────────────────────────────────────────────────────────────

const args = parseArgs(Bun.argv.slice(2));
if (args.plat === "wasm") {
  runWasm(args);
} else {
  await runNative(args);
}

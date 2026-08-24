// GCC/Clang build for gpui2 examples. Requires g++ (or clang++), pkg-config
// and the X11 / cairo / pango dev packages — see cmd/ubuntu-install-deps.sh.
// Reached through cmd/build.ts, which dispatches by host platform.
//   bun cmd/build.ts                         # print example list
//   bun cmd/build.ts app_assets
//   bun cmd/build.ts -dbg -all
//   bun cmd/build.ts -rel -asan system_monitor
//   bun cmd/build.ts -rel -clean showcase

import { existsSync, mkdirSync, cpSync, rmSync, readdirSync, statSync, readFileSync, writeFileSync } from "node:fs";
import { basename, dirname, join, resolve } from "node:path";
import { amalgamDir, amalgamIsWork, buildDist } from "./build-dist.ts";
import { printSizeTable } from "./sizes.ts";
import { ensureRustTree } from "./versions.ts";

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
];

const knownTargets = [
  "system_monitor",
  "app_assets",
  "showcase",
  "story",
  "tests",
  "bench",
  ...simpleExamples,
] as const;
type Target = (typeof knownTargets)[number];

const usage = `Usage: bun cmd/build.ts [-rel|-dbg] [-asan] [-clean] [-all] [<example>]

  -rel    release (default)
  -dbg    debug
  -asan   AddressSanitizer; combines with -rel or -dbg
  -clean  delete out/<dir>/ before building
  -all    build every example (amalgamation + compile); print total elapsed

Always writes .work/gpui.h and .work/gpui.cpp, then compiles examples
against them.

Outputs (out/linux/, so a Windows build of the same checkout survives):
  out/linux/rel/          release
  out/linux/dbg/          debug
  out/linux/rel_asan/     release + asan
  out/linux/dbg_asan/     debug + asan`;

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
  target: Target | null;
  all: boolean;
  debug: boolean;
  asan: boolean;
  clean: boolean;
} {
  let sawRel = false;
  let sawDbg = false;
  let asan = false;
  let clean = false;
  let all = false;
  const names: string[] = [];
  for (const raw of argv) {
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
    if (raw === "-all") {
      all = true;
      continue;
    }
    if (raw.startsWith("-")) {
      die(`Unknown flag: ${raw}`);
    }
    names.push(raw.toLowerCase());
  }
  if (names.includes("all")) {
    all = true;
    const rest = names.filter((n) => n !== "all");
    if (rest.length > 0) {
      die("Cannot combine -all with an example name");
    }
  }
  if (sawRel && sawDbg) {
    die("Cannot combine -rel and -dbg");
  }
  if (all) {
    if (names.length > 0 && !names.every((n) => n === "all")) {
      die("Cannot combine -all with an example name");
    }
    return { target: null, all: true, debug: sawDbg, asan, clean };
  }
  if (names.length === 0) {
    printExamples();
  }
  if (names.length !== 1) {
    die("Pass one example name, or -all");
  }
  const name = names[0]!;
  if (!(knownTargets as readonly string[]).includes(name)) {
    printExamples(`Unknown example: ${name}`);
  }
  return { target: name as Target, all: false, debug: sawDbg, asan, clean };
}

// out/linux/, not out/: a checkout is often built for both platforms (see
// cmd/wsl-run.ts) and the two must not overwrite each other's binaries,
// object files or -clean.
function outDirName(debug: boolean, asan: boolean): string {
  const base = debug ? "dbg" : "rel";
  const name = asan ? `${base}_asan` : base;
  // A build against a published amalgam keeps its own objects, so it neither
  // clobbers an ordinary build nor makes the next one recompile everything.
  return join("linux", amalgamIsWork() ? name : `${name}_dist`);
}

// The whole library, the platform halves included; one file, see
// cmd/build-dist.ts.
const amalgamSrc = [`${amalgamDir()}/gpui.cpp`];

function cppDir(rel: string): string[] {
  const dir = join(root, rel);
  if (!existsSync(dir)) {
    return [];
  }
  return readdirSync(dir)
    .filter((f) => f.endsWith(".cpp"))
    .map((f) => `${rel}/${f}`)
    .sort();
}

function sourcesFor(name: string): string[] | null {
  if (name === "system_monitor") {
    return [...amalgamSrc, "examples/system_monitor.cpp"];
  }
  if (name === "app_assets") {
    return [...amalgamSrc, "examples/app_assets.cpp"];
  }
  if (name === "showcase") {
    return [...amalgamSrc, ...cppDir("examples/showcase")];
  }
  if (name === "story") {
    return [...amalgamSrc, ...cppDir("examples/story")];
  }
  if (name === "tests") {
    return [...amalgamSrc, ...cppDir("tests")];
  }
  if (name === "bench") {
    return [...amalgamSrc, ...cppDir("bench")];
  }
  if (simpleExamples.includes(name)) {
    return [...amalgamSrc, `examples/${name}.cpp`];
  }
  return null;
}

function copyAssets(outDir: string) {
  const src = join(root, "assets");
  if (!existsSync(src)) {
    return;
  }
  const dst = join(root, outDir, "assets");
  mkdirSync(dst, { recursive: true });
  cpSync(src, dst, { recursive: true });
}

function which(name: string): string | null {
  const r = Bun.spawnSync(["which", name], { stdout: "pipe", stderr: "pipe" });
  if ((r.exitCode ?? 1) !== 0) {
    return null;
  }
  const out = new TextDecoder().decode(r.stdout).trim();
  return out.length > 0 ? out.split("\n")[0]! : null;
}

function findCompiler(): string {
  const fromEnv = process.env["CXX"];
  if (fromEnv) {
    return fromEnv;
  }
  for (const name of ["g++", "clang++"]) {
    if (which(name)) {
      return name;
    }
  }
  console.error("No C++ compiler found. Run: bash cmd/ubuntu-install-deps.sh");
  process.exit(1);
}

// x11 for the window, cairo + pangocairo for everything drawn in it.
const pkgs = ["x11", "cairo", "pangocairo"];

function pkgConfigFor(names: string[], kind: "--cflags" | "--libs"): string[] {
  const r = Bun.spawnSync(["pkg-config", kind, ...names], { stdout: "pipe", stderr: "pipe" });
  if ((r.exitCode ?? 1) !== 0) {
    console.error(new TextDecoder().decode(r.stderr).trim());
    console.error(`pkg-config ${kind} ${names.join(" ")} failed. Run: bash cmd/ubuntu-install-deps.sh`);
    process.exit(1);
  }
  return new TextDecoder()
    .decode(r.stdout)
    .trim()
    .split(/\s+/)
    .filter((s) => s.length > 0);
}

function pkgConfig(kind: "--cflags" | "--libs"): string[] {
  return pkgConfigFor(pkgs, kind);
}

const cxx = findCompiler();
const pkgCflags = pkgConfig("--cflags");
const pkgLibs = pkgConfig("--libs");

// libcurl is sys/http_linux.cpp's client, and the only soft dependency here:
// a machine without libcurl4-openssl-dev still builds, it just cannot fetch,
// and a remote image renders as its alt text the way it did before there was
// an HTTP client at all. GPUI_HAVE_CURL is what the source switches on.
function haveCurl(): boolean {
  const r = Bun.spawnSync(["pkg-config", "--exists", "libcurl"], {
    stdout: "pipe",
    stderr: "pipe",
  });
  return (r.exitCode ?? 1) === 0;
}

const curlFound = haveCurl();
const curlCflags = curlFound ? [...pkgConfigFor(["libcurl"], "--cflags"), "-DGPUI_HAVE_CURL=1"] : [];
const curlLibs = curlFound ? pkgConfigFor(["libcurl"], "--libs") : [];
if (!curlFound) {
  console.log("libcurl not found: remote images will not load. " + "Install it with: bash cmd/ubuntu-install-deps.sh");
}

function run(cmd: string[]) {
  const r = Bun.spawnSync(cmd, { cwd: root, stdout: "inherit", stderr: "inherit" });
  if ((r.exitCode ?? 1) !== 0) {
    process.exit(r.exitCode ?? 1);
  }
}

function mtimeMs(rel: string): number {
  try {
    return statSync(join(root, rel)).mtimeMs;
  } catch {
    return 0;
  }
}

const includeRe = /^\s*#\s*include\s+"([^"]+)"/gm;

function quotedIncludes(rel: string, memo: Map<string, string[]>): string[] {
  const hit = memo.get(rel);
  if (hit) {
    return hit;
  }
  memo.set(rel, []);
  const abs = join(root, rel);
  if (!existsSync(abs)) {
    return [];
  }
  const text = readFileSync(abs, "utf8");
  const dir = dirname(rel).replaceAll("\\", "/");
  const deps: string[] = [];
  includeRe.lastIndex = 0;
  let m: RegExpExecArray | null;
  while ((m = includeRe.exec(text))) {
    const inc = m[1]!.replaceAll("\\", "/");
    const candidates = [`${dir}/${inc}`, `${amalgamDir()}/${inc}`, `src/${inc}`];
    for (const raw of candidates) {
      const norm = raw.replace(/\/\.\//g, "/").replace(/^\.\//, "");
      if (!existsSync(join(root, norm))) {
        continue;
      }
      deps.push(norm);
      for (const d of quotedIncludes(norm, memo)) {
        deps.push(d);
      }
      break;
    }
  }
  const uniq = [...new Set(deps)];
  memo.set(rel, uniq);
  return uniq;
}

function needsCompile(src: string, obj: string, includes: string[]): boolean {
  const ot = mtimeMs(obj);
  if (ot === 0) {
    return true;
  }
  if (mtimeMs(src) > ot) {
    return true;
  }
  for (const h of includes) {
    if (mtimeMs(h) > ot) {
      return true;
    }
  }
  return false;
}

// src/ui/button.cpp and examples/showcase/button.cpp would both write
// button.o, so each group gets its own object directory.
function objGroup(f: string): string {
  if (f.startsWith("ext/")) {
    return "ext";
  }
  if (f.startsWith(`${amalgamDir()}/gpui`) || f.startsWith("src/gpui/")) {
    return "gpui";
  }
  if (f.startsWith("examples/showcase/")) {
    return "showcase";
  }
  if (f.startsWith("examples/story/")) {
    return "story";
  }
  if (f.startsWith("tests/")) {
    return "tests";
  }
  if (f.startsWith("bench/")) {
    return "bench";
  }
  return "ex";
}

function buildOne(name: string, debug: boolean, asan: boolean) {
  const src = sourcesFor(name);
  if (!src) {
    die(`Unknown target: ${name}`);
  }

  const outDir = join("out", outDirName(debug, asan));
  mkdirSync(join(root, outDir), { recursive: true });

  const cflags = [
    "-std=c++20",
    "-I",
    amalgamDir(),
    "-Wall",
    "-Wextra",
    "-Werror",
    "-fno-rtti",
    "-g",
    ...(debug ? ["-O0", "-DDEBUG"] : ["-O2", "-DNDEBUG"]),
    ...pkgCflags,
    ...curlCflags,
  ];
  const ldflags = [...pkgLibs, ...curlLibs, "-lm", "-lpthread"];
  if (asan) {
    cflags.push("-fsanitize=address", "-fno-omit-frame-pointer");
    ldflags.push("-fsanitize=address");
  }

  const cfg = `${debug ? "dbg" : "rel"}${asan ? "+asan" : ""}`;
  const exe = join(outDir, name);
  console.log(`Building ${name} (${cfg}) -> ${exe}`);

  const stampPath = join(outDir, "obj", "cflags.txt");
  const flagsKey = [cxx, ...cflags].join(" ");
  let flagsChanged = true;
  if (existsSync(join(root, stampPath))) {
    flagsChanged = readFileSync(join(root, stampPath), "utf8") !== flagsKey;
  }

  const objs: string[] = [];
  const includeMemo = new Map<string, string[]>();
  const dirty: string[] = [];
  let skipped = 0;
  // AppLog.cpp implements log() for every example.
  for (const srcFile of ["examples/AppLog.cpp", ...src]) {
    const objDir = join(outDir, "obj", objGroup(srcFile));
    mkdirSync(join(root, objDir), { recursive: true });
    const obj = join(objDir, basename(srcFile).replace(/\.(cpp|c)$/i, ".o"));
    objs.push(obj);
    const deps = quotedIncludes(srcFile, includeMemo);
    if (flagsChanged || needsCompile(srcFile, obj, deps)) {
      dirty.push(srcFile);
    } else {
      skipped++;
    }
  }
  for (const srcFile of dirty) {
    const objDir = join(outDir, "obj", objGroup(srcFile));
    const obj = join(objDir, basename(srcFile).replace(/\.(cpp|c)$/i, ".o"));
    run([cxx, ...cflags, "-c", srcFile, "-o", obj]);
  }
  mkdirSync(join(root, outDir, "obj"), { recursive: true });
  writeFileSync(join(root, stampPath), flagsKey);

  const exeTime = mtimeMs(exe);
  let linkNeeded = dirty.length > 0 || exeTime === 0;
  if (!linkNeeded) {
    for (const obj of objs) {
      if (mtimeMs(obj) > exeTime) {
        linkNeeded = true;
        break;
      }
    }
  }

  console.log(`compile ${dirty.length}, skip ${skipped}${linkNeeded ? "" : ", link skipped"}`);

  if (linkNeeded) {
    run([cxx, ...objs, "-o", exe, ...ldflags]);
  }
  copyAssets(outDir);
  console.log(`Built ${exe}`);
}

function formatElapsed(ms: number): string {
  const total = Math.max(0, Math.round(ms));
  const m = Math.floor(total / 60000);
  const s = Math.floor((total % 60000) / 1000);
  const milli = total % 1000;
  if (m > 0) {
    return `${m}m ${s}s ${milli}ms`;
  }
  if (s > 0) {
    return `${s}s ${milli}ms`;
  }
  return `${milli}ms`;
}

function printExeTable(outDir: string, names: string[]) {
  printSizeTable(
    names.map((n) => ({
      label: join(outDir, n),
      path: join(root, outDir, n),
    })),
  );
}

const started = performance.now();
const { target, all, debug, asan, clean } = parseArgs(Bun.argv.slice(2));
try {
  ensureRustTree(root);
} catch (e) {
  // The Rust spec tree is a reading reference, not a build input; a missing
  // clone must not stop a Linux build.
  console.error(e instanceof Error ? e.message : e);
}
// A plain build regenerates .work/ and compiles that. When GPUI_AMALGAM_DIR
// names another copy — cmd/build-dist.ts pointing at the dist repo — the build
// compiles that copy exactly as it was published, and does not rewrite it.
if (amalgamIsWork()) {
  const amalgam = buildDist({ outDir: ".work" });
  console.log(
    `amalgam ${amalgam.headerPath} + ${amalgam.sourcePath} ` +
      `(${amalgam.headerCount} headers, ${amalgam.sourceCount} + ${amalgam.platformSourceCount} sources)`,
  );
} else {
  for (const f of ["gpui.h", "gpui.cpp"]) {
    if (!existsSync(join(root, amalgamDir(), f))) {
      console.error(`missing ${amalgamDir()}/${f}`);
      process.exit(1);
    }
  }
  console.log(`amalgam ${amalgamDir()}/gpui.h + ${amalgamDir()}/gpui.cpp (as published)`);
}
const outDir = join("out", outDirName(debug, asan));
if (clean) {
  const abs = join(root, outDir);
  if (existsSync(abs)) {
    console.log(`Cleaning ${outDir}/`);
    rmSync(abs, { recursive: true, force: true });
  }
}
const built: string[] = [];
if (all) {
  // Every example. The test and benchmark runners are targets but not
  // examples, so -all leaves them to cmd/test.ts and cmd/bench.ts.
  built.push("system_monitor", "app_assets", "showcase", "story", ...simpleExamples);
} else if (target) {
  built.push(target);
}
for (const n of built) {
  buildOne(n, debug, asan);
}

console.log("");
printExeTable(outDir, built);
console.log(`elapsed ${formatElapsed(performance.now() - started)}`);

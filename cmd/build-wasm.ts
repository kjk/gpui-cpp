// Emscripten build for gpui2 examples: the same amalgamated gpui.cpp every
// other platform compiles, with src/**/*_wasm.cpp as its platform half and
// Canvas2D where Direct2D, cairo and Core Graphics are.
//
//   bun cmd/build-wasm.ts                    # print example list
//   bun cmd/build-wasm.ts hello_world
//   bun cmd/build-wasm.ts -dbg -all
//   bun cmd/build-wasm.ts -rel -clean showcase
//
// Reached through `bun cmd/build.ts -wasm <example>` too, which is how the
// other three toolchains are spelled. Serve and open the result with
// `bun cmd/run-wasm.ts <example>`: a wasm module has to come off a server,
// not off the filesystem.
//
// Emscripten is found through $EMCC, then $EMSDK, then PATH, then a sibling
// emsdk checkout. It is em++ rather than emcc: the link needs the C++ runtime
// and emcc leaves it out. If none of those has it:
//
//   git clone https://github.com/emscripten-core/emsdk ../.emsdk
//   cd ../.emsdk && ./emsdk install latest && ./emsdk activate latest

import { existsSync, mkdirSync, rmSync, readdirSync, statSync, readFileSync, writeFileSync } from "node:fs";
import { basename, dirname, join, resolve } from "node:path";
import { amalgamDir, amalgamIsWork, buildDist } from "./build-dist.ts";
import { printSizeTable } from "./sizes.ts";
import { ensureRustTree } from "./versions.ts";
import { findEmcc } from "./emsdk.ts";

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

// tests and bench have no window: they run under node and print. Everything
// else is a page.
const consoleTargets = new Set<string>(["tests", "bench"]);

const usage = `Usage: bun cmd/build-wasm.ts [-rel|-dbg] [-clean] [-all] [<example>]

  -rel    release (default)
  -dbg    debug (-O0, ASSERTIONS, SAFE_HEAP off)
  -clean  delete out/wasm/<dir>/ before building
  -all    build every example; print total elapsed

Always writes .work/gpui.h and .work/gpui.cpp, then compiles examples
against them.

Outputs (out/wasm/, so the native builds of the same checkout survive):
  out/wasm/rel/<name>.html + .js + .wasm + .data
  out/wasm/dbg/<name>.html + .js + .wasm + .data`;

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

function parseArgs(argv: string[]): { target: Target | null; all: boolean; debug: boolean; clean: boolean } {
  let sawRel = false;
  let sawDbg = false;
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
    if (raw === "-clean") {
      clean = true;
      continue;
    }
    if (raw === "-all") {
      all = true;
      continue;
    }
    // Accepted and ignored: cmd/build.ts passes every flag through untouched,
    // and -wasm is how it was told to come here.
    if (raw === "-wasm") {
      continue;
    }
    if (raw.startsWith("-")) {
      die(`Unknown flag: ${raw}`);
    }
    names.push(raw.toLowerCase());
  }
  if (names.includes("all")) {
    all = true;
    names.length = 0;
  }
  if (sawRel && sawDbg) {
    die("Cannot combine -rel and -dbg");
  }
  if (all) {
    if (names.length > 0) {
      die("Cannot combine -all with an example name");
    }
    return { target: null, all: true, debug: sawDbg, clean };
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
  return { target: name as Target, all: false, debug: sawDbg, clean };
}

function outDirName(debug: boolean): string {
  const name = debug ? "dbg" : "rel";
  return join("wasm", amalgamIsWork() ? name : `${name}_dist`);
}

// ─── sources ──────────────────────────────────────────────────────────────

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
  if (name === "showcase" || name === "story") {
    return [...amalgamSrc, ...cppDir(`examples/${name}`)];
  }
  if (name === "tests" || name === "bench") {
    return [...amalgamSrc, ...cppDir(name)];
  }
  if (name === "system_monitor" || name === "app_assets" || simpleExamples.includes(name)) {
    return [...amalgamSrc, `examples/${name}.cpp`];
  }
  return null;
}

// ─── incremental compile ──────────────────────────────────────────────────

const emcc = findEmcc();

function run(cmd: string[]) {
  const r = Bun.spawnSync(cmd, {
    cwd: root,
    stdout: "inherit",
    stderr: "inherit",
    env: { ...process.env, ...emcc.env },
  });
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
  if (ot === 0 || mtimeMs(src) > ot) {
    return true;
  }
  return includes.some((h) => mtimeMs(h) > ot);
}

function objGroup(f: string): string {
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

function buildOne(name: string, debug: boolean) {
  const src = sourcesFor(name);
  if (!src) {
    die(`Unknown target: ${name}`);
  }
  const outDir = join("out", outDirName(debug));
  mkdirSync(join(root, outDir), { recursive: true });

  const cflags = [
    "-std=c++20",
    "-I",
    amalgamDir(),
    "-Wall",
    "-Wextra",
    "-Werror",
    "-fno-rtti",
    ...(debug ? ["-O0", "-g", "-DDEBUG"] : ["-O2", "-DNDEBUG"]),
  ];

  // The element tree, the taffy solver and the markdown parser all recurse as
  // deep as the document does, and emscripten's default stack is 64 KB — a
  // tenth of what every one of the hosted platforms gives a thread.
  const ldflags = [
    "-sALLOW_MEMORY_GROWTH=1",
    "-sSTACK_SIZE=8MB",
    "-sENVIRONMENT=web,worker,node",
    // EM_JS bodies are the whole platform layer here, and they reach for the
    // heap views by name.
    "-sEXPORTED_RUNTIME_METHODS=HEAPU8,HEAP32,HEAPF32",
    ...(debug ? ["-O0", "-g", "-sASSERTIONS=2"] : ["-O2", "-sASSERTIONS=0"]),
  ];

  const isPage = !consoleTargets.has(name);
  if (isPage) {
    // The examples read their icons and images off disk; MEMFS is the disk.
    if (existsSync(join(root, "assets"))) {
      ldflags.push("--preload-file", "assets@/assets");
    }
    ldflags.push("--shell-file", "web/shell.html");
  } else {
    // No canvas, no assets: these print and exit.
    ldflags.push("-sEXIT_RUNTIME=1");
  }

  const ext = isPage ? ".html" : ".js";
  const outFile = join(outDir, name + ext);
  const cfg = debug ? "dbg" : "rel";
  console.log(`Building ${name} (${cfg}) -> ${outFile}`);

  const stampPath = join(outDir, "obj", "cflags.txt");
  const flagsKey = [emcc.exe, ...cflags].join(" ");
  let flagsChanged = true;
  if (existsSync(join(root, stampPath))) {
    flagsChanged = readFileSync(join(root, stampPath), "utf8") !== flagsKey;
  }

  const objs: string[] = [];
  const includeMemo = new Map<string, string[]>();
  const dirty: string[] = [];
  let skipped = 0;
  for (const srcFile of ["examples/AppLog.cpp", ...src]) {
    const objDir = join(outDir, "obj", objGroup(srcFile));
    mkdirSync(join(root, objDir), { recursive: true });
    const obj = join(objDir, basename(srcFile).replace(/\.(cpp|c)$/i, ".o"));
    objs.push(obj);
    if (flagsChanged || needsCompile(srcFile, obj, quotedIncludes(srcFile, includeMemo))) {
      dirty.push(srcFile);
    } else {
      skipped++;
    }
  }
  for (const srcFile of dirty) {
    const obj = join(outDir, "obj", objGroup(srcFile), basename(srcFile).replace(/\.(cpp|c)$/i, ".o"));
    run([emcc.exe, ...cflags, "-c", srcFile, "-o", obj]);
  }
  mkdirSync(join(root, outDir, "obj"), { recursive: true });
  writeFileSync(join(root, stampPath), flagsKey);

  const outTime = mtimeMs(outFile);
  let linkNeeded = dirty.length > 0 || outTime === 0;
  if (!linkNeeded) {
    linkNeeded = objs.some((o) => mtimeMs(o) > outTime);
  }
  if (!linkNeeded && isPage && mtimeMs("web/shell.html") > outTime) {
    linkNeeded = true;
  }
  console.log(`compile ${dirty.length}, skip ${skipped}${linkNeeded ? "" : ", link skipped"}`);

  if (linkNeeded) {
    run([emcc.exe, ...objs, "-o", outFile, ...ldflags]);
  }
  console.log(`Built ${outFile}`);
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

const started = performance.now();
const { target, all, debug, clean } = parseArgs(Bun.argv.slice(2));
try {
  ensureRustTree(root);
} catch (e) {
  // The Rust spec tree is a reading reference, not a build input.
  console.error(e instanceof Error ? e.message : e);
}
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

const outDir = join("out", outDirName(debug));
if (clean) {
  const abs = join(root, outDir);
  if (existsSync(abs)) {
    console.log(`Cleaning ${outDir}/`);
    rmSync(abs, { recursive: true, force: true });
  }
}

const built: string[] = [];
if (all) {
  built.push("system_monitor", "app_assets", "showcase", "story", ...simpleExamples);
} else if (target) {
  built.push(target);
}
for (const n of built) {
  buildOne(n, debug);
}

console.log("");
printSizeTable(
  built.flatMap((n) => {
    const ext = consoleTargets.has(n) ? ".js" : ".html";
    return [".wasm", ext].map((e) => ({
      label: join(outDir, n + e),
      path: join(root, outDir, n + e),
    }));
  }),
);
console.log(`elapsed ${formatElapsed(performance.now() - started)}`);

// MSVC build for gpui2 examples. Requires cl.exe on PATH.
// Reached through cmd/build.ts, which dispatches by host platform.
//   bun cmd/build.ts                         # print example list
//   bun cmd/build.ts app_assets
//   bun cmd/build.ts -dbg -all
//   bun cmd/build.ts -rel -asan system_monitor
//   bun cmd/build.ts -rel -clean showcase

import { existsSync, mkdirSync, cpSync, rmSync, readdirSync, statSync, readFileSync, writeFileSync } from "node:fs";
import { basename, dirname, join, resolve } from "node:path";
import { buildDist } from "./build-dist.ts";
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
];

const knownTargets = ["system_monitor", "app_assets", "showcase", "story", ...simpleExamples] as const;
type Target = (typeof knownTargets)[number];

const usage = `Usage: bun cmd/build.ts [-rel|-dbg] [-asan] [-clean] [-all] [<example>]

  -rel    release (default)
  -dbg    debug
  -asan   AddressSanitizer; combines with -rel or -dbg
  -clean  delete out/<dir>/ before building
  -all    build every example (amalgamation + compile); print total elapsed

Always writes .work/gpui.h and .work/gpui.cpp, then compiles examples
against that pair.

Outputs:
  out/rel/          release
  out/dbg/          debug
  out/rel_asan/     release + asan
  out/dbg_asan/     debug + asan`;

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

function outDirName(debug: boolean, asan: boolean): string {
  const base = debug ? "dbg" : "rel";
  return asan ? `${base}_asan` : base;
}

// The portable core plus the Windows half; see cmd/build-dist.ts.
// ext/md4c is C and stays its own translation unit.
const amalgamSrc = [".work/gpui.cpp", ".work/gpui_win.cpp", "ext/md4c/md4c.c"];

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

const libs = [
  "d2d1.lib",
  "dwrite.lib",
  "dwmapi.lib",
  "psapi.lib",
  "ole32.lib",
  "windowscodecs.lib",
  "user32.lib",
  "gdi32.lib",
  "gdiplus.lib",
  "shlwapi.lib",
  "uxtheme.lib",
  "comctl32.lib",
  "oleaut32.lib",
  "shell32.lib",
];

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

function clDir(): string | null {
  const r = Bun.spawnSync(["where", "cl"], { stdout: "pipe", stderr: "pipe" });
  if (r.exitCode !== 0) {
    return null;
  }
  const first = new TextDecoder()
    .decode(r.stdout)
    .split(/\r?\n/)
    .find((l) => l.trim().length > 0);
  if (!first) {
    return null;
  }
  return dirname(first.trim());
}

// ASan's own runtime (not the VC++ redistributable). Needed next to the exe.
function copyAsanDll(outDir: string) {
  const dir = clDir();
  if (!dir) {
    return;
  }
  const name = "clang_rt.asan_dynamic-x86_64.dll";
  const src = join(dir, name);
  if (!existsSync(src)) {
    return;
  }
  cpSync(src, join(root, outDir, name));
}

function groupSources(files: string[]): { key: string; files: string[] }[] {
  const buckets: Record<string, string[]> = {
    ext: [],
    gpui: [],
    showcase: [],
    story: [],
    ex: [],
  };
  for (const f of files) {
    if (f.startsWith("ext/")) {
      buckets.ext.push(f);
    } else if (f.startsWith(".work/gpui") || f.startsWith("src/gpui/")) {
      buckets.gpui.push(f);
    } else if (f.startsWith("examples/showcase/")) {
      buckets.showcase.push(f);
    } else if (f.startsWith("examples/story/")) {
      buckets.story.push(f);
    } else {
      buckets.ex.push(f);
    }
  }
  return Object.entries(buckets)
    .filter(([, list]) => list.length > 0)
    .map(([key, list]) => ({ key, files: list }));
}

function runCl(args: string[]) {
  const r = Bun.spawnSync(["cl", ...args], {
    cwd: root,
    stdout: "inherit",
    stderr: "inherit",
  });
  if (r.exitCode !== 0) {
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
    const candidates = [`${dir}/${inc}`, `.work/${inc}`, `src/${inc}`];
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

function buildOne(name: string, debug: boolean, asan: boolean) {
  const src = sourcesFor(name);
  if (!src) {
    die(`Unknown target: ${name}`);
  }

  const outName = outDirName(debug, asan);
  const outDir = join("out", outName);
  mkdirSync(join(root, outDir), { recursive: true });

  const cflags = [
    "/nologo",
    "/std:c++20",
    "/EHsc",
    "/utf-8",
    "/I",
    ".work",
    "/I",
    "ext/md4c",
    "/DUNICODE",
    "/D_UNICODE",
    "/W4",
    "/WX",
    "/wd4996",
    "/MP",
    "/FS",
    "/Zi",
    // /MT /MTd: static CRT. Do not use /MD — that pulls vcruntime140.dll.
    // /Gy /Gw: one COMDAT per function/global so the linker can drop unused
    // code and fold identical functions. /DEBUG would otherwise disable that.
    ...(debug ? ["/Od", "/MTd", "/DDEBUG"] : ["/O2", "/Gy", "/Gw", "/MT", "/DNDEBUG"]),
  ];
  if (asan) {
    cflags.push("/fsanitize=address");
  }

  const cfg = `${debug ? "dbg" : "rel"}${asan ? "+asan" : ""}`;
  console.log(`Building ${name} (${cfg}) -> ${outDir}\\${name}.exe`);

  const stampPath = join(outDir, "obj", "cflags.txt");
  const flagsKey = cflags.join(" ");
  let flagsChanged = true;
  if (existsSync(join(root, stampPath))) {
    flagsChanged = readFileSync(join(root, stampPath), "utf8") !== flagsKey;
  }

  // Separate /Fo dirs so src/ui/Button.cpp and examples/showcase/button.cpp
  // do not both write button.obj.
  const objs: string[] = [];
  const includeMemo = new Map<string, string[]>();
  let compiled = 0;
  let skipped = 0;
  // AppLog.cpp implements log() for every example.
  for (const g of groupSources(["examples/AppLog.cpp", ...src])) {
    const objDir = join(outDir, "obj", g.key);
    mkdirSync(join(root, objDir), { recursive: true });
    const dirty: string[] = [];
    for (const srcFile of g.files) {
      const obj = join(objDir, basename(srcFile).replace(/\.(cpp|c)$/i, ".obj"));
      objs.push(obj);
      const deps = quotedIncludes(srcFile, includeMemo);
      if (flagsChanged || needsCompile(srcFile, obj, deps)) {
        dirty.push(srcFile);
      } else {
        skipped++;
      }
    }
    if (dirty.length > 0) {
      // md4c is C, and it is not ours to keep warning-clean.
      const flags =
        g.key === "ext"
          ? cflags
              .filter((f) => f !== "/std:c++20" && f !== "/EHsc" && f !== "/W4" && f !== "/WX")
              .concat(["/TC", "/std:c17", "/w"])
          : cflags;
      runCl([...flags, "/c", `/Fo${objDir}\\`, `/Fd${objDir}\\`, ...dirty]);
      compiled += dirty.length;
    }
  }
  mkdirSync(join(root, outDir, "obj"), { recursive: true });
  writeFileSync(join(root, stampPath), flagsKey);

  const exe = join(outDir, `${name}.exe`);
  const exeTime = mtimeMs(exe);
  let linkNeeded = compiled > 0 || exeTime === 0;
  if (!linkNeeded) {
    for (const obj of objs) {
      if (mtimeMs(obj) > exeTime) {
        linkNeeded = true;
        break;
      }
    }
  }

  console.log(`compile ${compiled}, skip ${skipped}${linkNeeded ? "" : ", link skipped"}`);

  if (linkNeeded) {
    const link = [
      "/link",
      "/SUBSYSTEM:WINDOWS",
      "/ENTRY:wWinMainCRTStartup",
      "/NODEFAULTLIB:msvcrt.lib",
      "/NODEFAULTLIB:msvcrtd.lib",
      "/NODEFAULTLIB:ucrt.lib",
      "/NODEFAULTLIB:ucrtd.lib",
      "/NODEFAULTLIB:vcruntime.lib",
      "/NODEFAULTLIB:vcruntimed.lib",
      ...libs,
    ];
    if (!debug) {
      // /DEBUG implies /OPT:NOREF unless we opt back in.
      link.push("/INCREMENTAL:NO", "/OPT:REF", "/OPT:ICF");
    } else if (asan) {
      link.push("/INCREMENTAL:NO");
    }
    link.push("/DEBUG", `/PDB:${outDir}\\${name}.pdb`);
    runCl(["/nologo", `/Fe${exe}`, `/Fd${outDir}\\`, ...objs, ...link]);
  }

  if (asan) {
    copyAsanDll(outDir);
  }
  copyAssets(outDir);
  console.log(`Built ${exe.replaceAll("/", "\\")}`);
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
      label: join(outDir, `${n}.exe`).replaceAll("/", "\\"),
      path: join(root, outDir, `${n}.exe`),
    })),
  );
}

const started = performance.now();
const { target, all, debug, asan, clean } = parseArgs(Bun.argv.slice(2));
try {
  ensureRustTree(root);
} catch (e) {
  // The Rust spec tree is a reading reference, not a build input; a missing
  // clone must not stop a build.
  console.error(e instanceof Error ? e.message : e);
}
const amalgam = buildDist({ outDir: ".work", platform: "win" });
console.log(
  `amalgam ${amalgam.headerPath} + ${amalgam.sourcePath} + ${amalgam.platformSourcePath} ` +
    `(${amalgam.headerCount} headers, ${amalgam.sourceCount} + ${amalgam.platformSourceCount} sources)`,
);
const outDir = join("out", outDirName(debug, asan));
if (clean) {
  const abs = join(root, outDir);
  if (existsSync(abs)) {
    console.log(`Cleaning ${outDir}\\`);
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
  buildOne(n, debug, asan);
}

console.log("");
printExeTable(outDir, built);
console.log(`elapsed ${formatElapsed(performance.now() - started)}`);

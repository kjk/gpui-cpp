// Amalgamate src/**/*.h and src/**/*.cpp into gpui.h plus two .cpp files: the
// portable core (gpui.cpp) and the platform half (gpui_win.cpp, gpui_linux.cpp
// or gpui_mac.cpp). The split is what keeps <windows.h>, <X11/*> and <Cocoa/*>
// out of the same translation unit as everything else.
//
// A source file belongs to a platform by suffix: _win.cpp, _linux.cpp,
// _mac.cpp, and _posix.cpp for the Linux and macOS halves both.
//
//   bun cmd/build-dist.ts           # write dist/, then time dbg+rel compile
//   bun cmd/build-dist.ts -work     # write .work/ instead of dist/
//   bun cmd/build-dist.ts -linux    # amalgamate the Linux platform half
//   bun cmd/build-dist.ts -mac      # ... the macOS one
//   bun cmd/build-dist.ts -no-bench # skip the compile timing
//
// import { buildDist } from "./build-dist.ts";
// buildDist();                 // dist/gpui.h + gpui.cpp + gpui_<plat>.cpp
// buildDist({ outDir: ".work" });

import { existsSync, mkdirSync, readdirSync, readFileSync, writeFileSync, statSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";

const root = resolve(import.meta.dir, "..");

export type DistOutDir = "dist" | ".work";

export type Platform = "win" | "linux" | "mac";

export function hostPlatform(): Platform {
  if (process.platform === "win32") {
    return "win";
  }
  return process.platform === "darwin" ? "mac" : "linux";
}

export type BuildDistOpts = {
  /** Destination directory relative to the repo root. Default: "dist". */
  outDir?: DistOutDir;
  /** Which platform half to emit. Default: the host. */
  platform?: Platform;
};

export type BuildDistResult = {
  outDir: string;
  platform: Platform;
  headerPath: string;
  sourcePath: string;
  /** The <name>_win.cpp / <name>_linux.cpp amalgam. */
  platformSourcePath: string;
  headerBytes: number;
  sourceBytes: number;
  platformSourceBytes: number;
  headerCount: number;
  sourceCount: number;
  platformSourceCount: number;
};

// Which platform halves a source file belongs to. Empty means it is portable
// and goes in gpui.cpp; _posix.cpp belongs to two.
function filePlatforms(rel: string): Platform[] {
  if (/_win\.cpp$/.test(rel)) {
    return ["win"];
  }
  if (/_linux\.cpp$/.test(rel)) {
    return ["linux"];
  }
  if (/_mac\.cpp$/.test(rel)) {
    return ["mac"];
  }
  if (/_posix\.cpp$/.test(rel)) {
    return ["linux", "mac"];
  }
  return [];
}

const quotedIncRe = /^\s*#\s*include\s+"([^"]+)"/;
const angleIncRe = /^\s*#\s*include\s+<([^>]+)>/;
const pragmaOnceRe = /^\s*#\s*pragma\s+once\b/;
const staticDeclRe = /^static\b[^=\n{]*?\b([A-Za-z_]\w*)\s*(?=\(|\[|=|;)/gm;

function walkFiles(dir: string, ext: string, out: string[]): void {
  if (!existsSync(dir)) {
    return;
  }
  const ents = readdirSync(dir, { withFileTypes: true });
  ents.sort((a, b) => a.name.localeCompare(b.name));
  for (const ent of ents) {
    const abs = join(dir, ent.name);
    if (ent.isDirectory()) {
      walkFiles(abs, ext, out);
      continue;
    }
    if (ent.name.endsWith(ext)) {
      out.push(abs);
    }
  }
}

function srcRel(abs: string): string {
  return relative(root, abs).replaceAll("\\", "/");
}

function listSrc(ext: string): string[] {
  const files: string[] = [];
  walkFiles(join(root, "src"), ext, files);
  return files.map(srcRel);
}

function readLf(rel: string): string {
  return readFileSync(join(root, rel), "utf8").replace(/\r\n/g, "\n").replace(/\r/g, "\n");
}

function resolveQuoted(fromRel: string, inc: string): string | null {
  const incNorm = inc.replaceAll("\\", "/");
  const fromDir = dirname(fromRel).replaceAll("\\", "/");
  const candidates = [`src/${incNorm}`, `${fromDir}/${incNorm}`, `src/${incNorm.split("/").pop()}`];
  for (const raw of candidates) {
    const norm = raw.replace(/\/\.\//g, "/").replace(/^\.\//, "");
    if (existsSync(join(root, norm))) {
      return norm;
    }
  }
  return null;
}

function quotedDeps(rel: string, text: string): string[] {
  const deps: string[] = [];
  for (const line of text.split("\n")) {
    const m = quotedIncRe.exec(line);
    if (!m) {
      continue;
    }
    const hit = resolveQuoted(rel, m[1]!);
    if (hit) {
      deps.push(hit);
    }
  }
  return deps;
}

function topoHeaders(headers: string[]): string[] {
  const texts = new Map<string, string>();
  for (const h of headers) {
    texts.set(h, readLf(h));
  }
  const seen = new Set<string>();
  const order: string[] = [];
  const visit = (rel: string) => {
    if (seen.has(rel)) {
      return;
    }
    seen.add(rel);
    const text = texts.get(rel);
    if (text) {
      for (const d of quotedDeps(rel, text)) {
        visit(d);
      }
    }
    order.push(rel);
  };
  const preferred = [
    "src/Base.h",
    "src/gpui/Gpui.h",
    "src/gpui/Assets.h",
    "src/gpui/Svg.h",
    "src/ui/Primitive.h",
    "src/ui/Ui.h",
    "src/component/Common.h",
    "src/component/Component.h",
    "src/sys/SysInfo.h",
  ];
  for (const p of preferred) {
    if (texts.has(p)) {
      visit(p);
    }
  }
  for (const h of headers) {
    visit(h);
  }
  return order;
}

function stripInternalIncludes(fromRel: string, text: string): string {
  const lines = text.split("\n");
  const keep: string[] = [];
  for (const line of lines) {
    if (pragmaOnceRe.test(line)) {
      continue;
    }
    const m = quotedIncRe.exec(line);
    if (m && resolveQuoted(fromRel, m[1]!)) {
      continue;
    }
    keep.push(line);
  }
  return keep.join("\n");
}

function stripComments(src: string): string {
  let out = "";
  let i = 0;
  const n = src.length;
  while (i < n) {
    const c = src[i]!;
    const d = i + 1 < n ? src[i + 1]! : "";
    if (c === '"') {
      out += c;
      i++;
      while (i < n) {
        const ch = src[i]!;
        out += ch;
        i++;
        if (ch === "\\" && i < n) {
          out += src[i]!;
          i++;
          continue;
        }
        if (ch === '"') {
          break;
        }
        if (ch === "\n") {
          break;
        }
      }
      continue;
    }
    if (c === "'") {
      out += c;
      i++;
      while (i < n) {
        const ch = src[i]!;
        out += ch;
        i++;
        if (ch === "\\" && i < n) {
          out += src[i]!;
          i++;
          continue;
        }
        if (ch === "'") {
          break;
        }
        if (ch === "\n") {
          break;
        }
      }
      continue;
    }
    if (c === "/" && d === "/") {
      i += 2;
      while (i < n && src[i] !== "\n") {
        i++;
      }
      continue;
    }
    if (c === "/" && d === "*") {
      i += 2;
      while (i + 1 < n && !(src[i] === "*" && src[i + 1] === "/")) {
        i++;
      }
      i = i + 1 < n ? i + 2 : n;
      out += " ";
      continue;
    }
    out += c;
    i++;
  }
  return out;
}

function collapseBlankLines(src: string): string {
  const lines = src.split("\n").map((l) => l.replace(/[ \t]+$/g, ""));
  const out: string[] = [];
  let blank = false;
  for (const line of lines) {
    if (line.length === 0) {
      if (!blank) {
        out.push("");
      }
      blank = true;
      continue;
    }
    blank = false;
    out.push(line);
  }
  while (out.length > 0 && out[0] === "") {
    out.shift();
  }
  while (out.length > 0 && out[out.length - 1] === "") {
    out.pop();
  }
  return out.join("\n") + "\n";
}

function staticNames(src: string): string[] {
  const names: string[] = [];
  staticDeclRe.lastIndex = 0;
  let m: RegExpExecArray | null;
  while ((m = staticDeclRe.exec(src))) {
    names.push(m[1]!);
  }
  return names;
}

function filePrefix(rel: string): string {
  return rel
    .replace(/^src\//, "")
    .replace(/\.[^.]+$/, "")
    .replace(/[^A-Za-z0-9]+/g, "_");
}

function renameIdents(src: string, names: Set<string>, prefix: string): string {
  if (names.size === 0) {
    return src;
  }
  const re = new RegExp(`\\b(${[...names].sort().join("|")})\\b`, "g");
  return src.replace(re, (name) => `${prefix}_${name}`);
}

function preferredCppOrder(cpps: string[]): string[] {
  const rank = (rel: string): number => {
    if (rel === "src/Base.cpp") {
      return 0;
    }
    if (rel.startsWith("src/gpui/")) {
      return 1;
    }
    if (rel.startsWith("src/ui/")) {
      return 2;
    }
    if (rel.startsWith("src/component/")) {
      return 3;
    }
    if (rel.startsWith("src/sys/")) {
      return 4;
    }
    return 5;
  };
  return [...cpps].sort((a, b) => {
    const d = rank(a) - rank(b);
    return d !== 0 ? d : a.localeCompare(b);
  });
}

function finish(text: string): string {
  return collapseBlankLines(stripComments(text));
}

export function buildDist(opts?: BuildDistOpts): BuildDistResult {
  const outDir = opts?.outDir ?? "dist";
  const platform = opts?.platform ?? hostPlatform();
  const headers = listSrc(".h");
  const allCpps = preferredCppOrder(listSrc(".cpp"));
  const cpps = allCpps.filter((f) => filePlatforms(f).length === 0);
  const platCpps = allCpps.filter((f) => filePlatforms(f).includes(platform));
  if (headers.length === 0 || cpps.length === 0) {
    throw new Error("no src/**/*.h or src/**/*.cpp to amalgamate");
  }
  if (platCpps.length === 0) {
    throw new Error(`no src/**/*_${platform}.cpp to amalgamate`);
  }

  const headerOrder = topoHeaders(headers);
  const headerChunks: string[] = [
    "#ifndef GPUI_H_",
    "#define GPUI_H_",
    "#ifndef GPUI_AMALGAM",
    "#define GPUI_AMALGAM 1",
    "#endif",
    "",
  ];
  for (const rel of headerOrder) {
    const body = stripInternalIncludes(rel, readLf(rel)).trim();
    if (!body) {
      continue;
    }
    headerChunks.push(`#line 1 "${rel}"`, body, "");
  }
  headerChunks.push("#endif");

  const cppTexts = new Map<string, string>();
  const nameToFiles = new Map<string, string[]>();
  for (const rel of [...cpps, ...platCpps]) {
    const body = stripInternalIncludes(rel, readLf(rel));
    cppTexts.set(rel, body);
    for (const name of staticNames(body)) {
      const list = nameToFiles.get(name) ?? [];
      list.push(rel);
      nameToFiles.set(name, list);
    }
  }
  const colliding = new Set<string>();
  for (const [name, files] of nameToFiles) {
    if (new Set(files).size > 1) {
      colliding.add(name);
    }
  }

  const concat = (list: string[]): string => {
    const chunks: string[] = ['#include "gpui.h"', ""];
    for (const rel of list) {
      let body = cppTexts.get(rel) ?? "";
      const local = new Set(staticNames(body).filter((n) => colliding.has(n)));
      body = renameIdents(body, local, filePrefix(rel)).trim();
      if (!body) {
        continue;
      }
      chunks.push(`#line 1 "${rel}"`, body, "");
    }
    return finish(chunks.join("\n"));
  };

  const headerText = finish(headerChunks.join("\n"));
  const sourceText = concat(cpps);
  const platformText = concat(platCpps);

  const absOut = join(root, outDir);
  mkdirSync(absOut, { recursive: true });
  const headerPath = join(absOut, "gpui.h");
  const sourcePath = join(absOut, "gpui.cpp");
  const platformSourcePath = join(absOut, `gpui_${platform}.cpp`);
  const writeIfChanged = (path: string, text: string) => {
    if (existsSync(path) && readFileSync(path, "utf8") === text) {
      return;
    }
    writeFileSync(path, text, "utf8");
  };
  writeIfChanged(headerPath, headerText);
  writeIfChanged(sourcePath, sourceText);
  writeIfChanged(platformSourcePath, platformText);

  return {
    outDir,
    platform,
    headerPath: srcRel(headerPath),
    sourcePath: srcRel(sourcePath),
    platformSourcePath: srcRel(platformSourcePath),
    headerBytes: Buffer.byteLength(headerText, "utf8"),
    sourceBytes: Buffer.byteLength(sourceText, "utf8"),
    platformSourceBytes: Buffer.byteLength(platformText, "utf8"),
    headerCount: headerOrder.length,
    sourceCount: cpps.length,
    platformSourceCount: platCpps.length,
  };
}

export type CompileDistOpts = {
  outDir?: DistOutDir;
  debug: boolean;
};

export type CompileDistResult = {
  debug: boolean;
  ms: number;
  objPath: string;
  exitCode: number;
};

function clFlags(debug: boolean, includeDir: string): string[] {
  return [
    "/nologo",
    "/std:c++20",
    "/EHsc",
    "/utf-8",
    "/I",
    includeDir,
    "/DUNICODE",
    "/D_UNICODE",
    "/W3",
    "/wd4996",
    "/wd4244",
    "/wd4267",
    "/c",
    "/Zi",
    ...(debug ? ["/Od", "/MTd", "/DDEBUG"] : ["/O2", "/MT", "/DNDEBUG"]),
  ];
}

export function compileDist(opts: CompileDistOpts): CompileDistResult {
  const outDir = opts.outDir ?? "dist";
  const src = join(outDir, "gpui.cpp");
  if (!existsSync(join(root, src))) {
    throw new Error(`missing ${src}; run buildDist() first`);
  }
  const objDir = join("out", opts.debug ? "amalgam_dbg" : "amalgam_rel");
  mkdirSync(join(root, objDir), { recursive: true });
  const objPath = join(objDir, "gpui.obj");
  const args = [...clFlags(opts.debug, outDir), `/Fo${objPath}`, `/Fd${objDir}\\`, src];
  const t0 = performance.now();
  const r = Bun.spawnSync(["cl", ...args], {
    cwd: root,
    stdout: "pipe",
    stderr: "pipe",
  });
  const ms = performance.now() - t0;
  const stdout = new TextDecoder().decode(r.stdout);
  const stderr = new TextDecoder().decode(r.stderr);
  if (stdout.trim()) {
    console.log(stdout.trimEnd());
  }
  if (stderr.trim()) {
    console.error(stderr.trimEnd());
  }
  return { debug: opts.debug, ms, objPath, exitCode: r.exitCode ?? 1 };
}

function formatBytes(n: number): string {
  if (n < 1024) {
    return `${n} b`;
  }
  if (n < 1024 * 1024) {
    return `${(n / 1024).toFixed(1)} KB`;
  }
  return `${(n / (1024 * 1024)).toFixed(2)} MB`;
}

function formatMs(ms: number): string {
  if (ms < 1000) {
    return `${Math.round(ms)} ms`;
  }
  return `${(ms / 1000).toFixed(2)} s`;
}

function die(msg: string): never {
  console.error(msg);
  process.exit(1);
}

function parseCli(argv: string[]): { outDir: DistOutDir; bench: boolean; platform: Platform } {
  let outDir: DistOutDir = "dist";
  let bench = true;
  let platform = hostPlatform();
  for (const raw of argv) {
    if (raw === "-work" || raw === "--work") {
      outDir = ".work";
      continue;
    }
    if (raw === "-win" || raw === "--win") {
      platform = "win";
      continue;
    }
    if (raw === "-linux" || raw === "--linux") {
      platform = "linux";
      continue;
    }
    if (raw === "-mac" || raw === "--mac") {
      platform = "mac";
      continue;
    }
    if (raw === "-no-bench" || raw === "--no-bench") {
      bench = false;
      continue;
    }
    if (raw === "-bench" || raw === "--bench") {
      bench = true;
      continue;
    }
    if (raw.startsWith("-")) {
      die(`unknown option: ${raw}\nusage: bun cmd/build-dist.ts [-work] [-win|-linux|-mac] [-no-bench]`);
    }
    die(`unknown argument: ${raw}\nusage: bun cmd/build-dist.ts [-work] [-win|-linux|-mac] [-no-bench]`);
  }
  return { outDir, bench, platform };
}

function main(): void {
  const { outDir, bench, platform } = parseCli(Bun.argv.slice(2));
  const built = buildDist({ outDir, platform });
  console.log(`wrote ${built.headerPath} (${formatBytes(built.headerBytes)}, ${built.headerCount} headers)`);
  console.log(`wrote ${built.sourcePath} (${formatBytes(built.sourceBytes)}, ${built.sourceCount} sources)`);
  console.log(
    `wrote ${built.platformSourcePath} (${formatBytes(built.platformSourceBytes)}, ${built.platformSourceCount} sources)`,
  );
  if (!bench) {
    return;
  }
  if (process.platform !== "win32") {
    console.log("skipping the compile bench: it shells out to cl.exe");
    return;
  }
  for (const debug of [true, false]) {
    const label = debug ? "debug" : "release";
    console.log(`compiling ${built.sourcePath} (${label})...`);
    const r = compileDist({ outDir, debug });
    if (r.exitCode !== 0) {
      die(`cl.exe failed (${label}, exit ${r.exitCode})`);
    }
    const objBytes = existsSync(join(root, r.objPath)) ? statSync(join(root, r.objPath)).size : 0;
    console.log(`  ${label}: ${formatMs(r.ms)}  -> ${r.objPath} (${formatBytes(objBytes)})`);
  }
}

if (import.meta.main) {
  process.chdir(root);
  main();
}

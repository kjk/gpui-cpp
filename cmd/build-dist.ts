// Amalgamate src/**/*.h and src/**/*.cpp, plus the vendored md4c, into two
// files: gpui.h and gpui.cpp. Both are the same on every platform.
//
// A source file belongs to a platform by suffix: _win.cpp, _linux.cpp,
// _mac.cpp, and _posix.cpp for the Linux and macOS halves both. Each of those
// goes into gpui.cpp inside its own `#if GPUI_OS_*`, so <windows.h>, <X11/*>
// and <Cocoa/*> still never reach the same translation unit — the preprocessor
// drops the two halves that are not this platform's before anything parses
// them. On macOS the whole file is Objective-C++, because the mac half is.
//
// md4c is the tail of both outputs: md4c.h at the end of gpui.h, md4c.c at the
// end of gpui.cpp. `ext/md4c` stays pristine, so the handful of edits C++
// needs that C did not — casting malloc/realloc away from void* — are applied
// here, each one asserted to match exactly once so an md4c refresh that moves
// them fails loudly instead of silently.
//
//   bun cmd/build-dist.ts           # write dist/, then time dbg+rel compile
//   bun cmd/build-dist.ts -work     # write .work/ instead of dist/
//   bun cmd/build-dist.ts -no-bench # skip the compile timing
//
// import { buildDist } from "./build-dist.ts";
// buildDist();                 // dist/gpui.h + dist/gpui.cpp
// buildDist({ outDir: ".work" });

import { existsSync, mkdirSync, readdirSync, readFileSync, writeFileSync, statSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";

const root = resolve(import.meta.dir, "..");

export type DistOutDir = "dist" | ".work";

export type Platform = "win" | "linux" | "mac";

export const allPlatforms: Platform[] = ["win", "linux", "mac"];

export type BuildDistOpts = {
  /** Destination directory relative to the repo root. Default: "dist". */
  outDir?: DistOutDir;
};

export type BuildDistResult = {
  outDir: string;
  headerPath: string;
  sourcePath: string;
  headerBytes: number;
  sourceBytes: number;
  headerCount: number;
  /** Portable sources. */
  sourceCount: number;
  /** The _win / _linux / _mac / _posix ones, all of them. */
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

const osMacro: Record<Platform, string> = {
  win: "GPUI_OS_WINDOWS",
  linux: "GPUI_OS_LINUX",
  mac: "GPUI_OS_MAC",
};

// src/Base.h defines all three, exactly one of them 1, so a plain #if is all a
// platform chunk needs to be there for its own platform and nowhere else.
function guardFor(plats: Platform[]): string {
  return `#if ${plats.map((p) => osMacro[p]).join(" || ")}`;
}

// The vendored parser, appended to the tail of each output. See ext/md4c.
const md4cHeader = "ext/md4c/md4c.h";
const md4cSource = "ext/md4c/md4c.c";

// md4c is C, and C++ will not convert a void* to a typed pointer on its own.
// These six are every place that needs a cast — five malloc/realloc results and
// one md_mark_get_ptr. Each must match exactly once.
const md4cCppFixes: [string, string][] = [
  [
    "            new_buffer = realloc(ctx->buffer, new_size);",
    "            new_buffer = (CHAR*) realloc(ctx->buffer, new_size);",
  ],
  [
    "    ctx->ref_def_hashtable = malloc(ctx->ref_def_hashtable_size * sizeof(void*));",
    "    ctx->ref_def_hashtable = (void**) malloc(ctx->ref_def_hashtable_size * sizeof(void*));",
  ],
  [
    "        new_marks = realloc(ctx->marks, ctx->alloc_marks * sizeof(MD_MARK));",
    "        new_marks = (MD_MARK*) realloc(ctx->marks, ctx->alloc_marks * sizeof(MD_MARK));",
  ],
  [
    "                                md_mark_get_ptr(ctx, (int)(title_mark - ctx->marks)),",
    "                                (const CHAR*) md_mark_get_ptr(ctx, (int)(title_mark - ctx->marks)),",
  ],
  [
    "    align = malloc(col_count * sizeof(MD_ALIGN));",
    "    align = (MD_ALIGN*) malloc(col_count * sizeof(MD_ALIGN));",
  ],
  [
    "        new_containers = realloc(ctx->containers, ctx->alloc_containers * sizeof(MD_CONTAINER));",
    "        new_containers = (MD_CONTAINER*) realloc(ctx->containers, ctx->alloc_containers * sizeof(MD_CONTAINER));",
  ],
];

// The tree compiles with /W4 /WX and -Wall -Wextra -Werror. md4c is not ours
// to keep clean under those, so its two chunks are bracketed by these.
const warnPush = [
  "#if defined(_MSC_VER)",
  "#pragma warning(push, 0)",
  // C4701 and C4702 are decided after code generation, so the level-0 push
  // above does not reach them; they have to be named.
  "#pragma warning(disable : 4701 4702)",
  "#elif defined(__GNUC__) || defined(__clang__)",
  "#pragma GCC diagnostic push",
  '#pragma GCC diagnostic ignored "-Wmissing-field-initializers"',
  "#endif",
].join("\n");

const warnPop = [
  "#if defined(_MSC_VER)",
  "#pragma warning(pop)",
  "#elif defined(__GNUC__) || defined(__clang__)",
  "#pragma GCC diagnostic pop",
  "#endif",
].join("\n");

// md4c defines these two unguarded, and glib (through Pango) already has them,
// which gcc reports with a warning that has no -W switch to turn off. Nothing
// after md4c uses either, so dropping the earlier definition is enough.
const md4cUndefs = ["#undef MIN", "#undef MAX"];

// `pop` is optional because md4c is the last chunk of whichever file it lands
// in: the header has to restore the includer's warning state, but the .cpp
// must not, since MSVC decides C4701 and C4702 after code generation and reads
// the state as it stands at the end of the translation unit.
function md4cChunk(rel: string, fixes: [string, string][], pop: boolean): string {
  let body = stripInternalIncludes(rel, readLf(rel));
  for (const [from, to] of fixes) {
    const n = body.split(from).length - 1;
    if (n !== 1) {
      throw new Error(`${rel}: expected 1 match, found ${n}, for: ${from.trim()}`);
    }
    body = body.split(from).join(to);
  }
  const tail = pop ? [warnPop, ""] : [""];
  const undefs = rel.endsWith(".c") ? md4cUndefs : [];
  return [warnPush, ...undefs, `#line 1 "${rel}"`, body, ...tail].join("\n");
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
  const candidates = [
    `src/${incNorm}`,
    `${fromDir}/${incNorm}`,
    `src/${incNorm.split("/").pop()}`,
    // md4c.h is part of the amalgam too, so the include of it is internal.
    `ext/md4c/${incNorm.split("/").pop()}`,
  ];
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

// Every transform here replaces rather than removes lines: an amalgam chunk
// starts with `#line 1 "<original>"`, and that is only true if the chunk has
// exactly as many lines as the file it came from. A compiler diagnostic then
// points at the real line of the real file, which -Werror makes worth the
// handful of blank lines it costs.
function stripInternalIncludes(fromRel: string, text: string): string {
  const lines = text.split("\n");
  const keep: string[] = [];
  for (const line of lines) {
    if (pragmaOnceRe.test(line)) {
      keep.push("");
      continue;
    }
    const m = quotedIncRe.exec(line);
    if (m && resolveQuoted(fromRel, m[1]!)) {
      keep.push("");
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
      const start = i;
      i += 2;
      while (i + 1 < n && !(src[i] === "*" && src[i + 1] === "/")) {
        i++;
      }
      i = i + 1 < n ? i + 2 : n;
      // Keep the newlines the comment spanned, so the #line directives above
      // each chunk stay true and a compiler diagnostic points at the real
      // line of the real file.
      let nl = "";
      for (let k = start; k < i; k++) {
        if (src[k] === "\n") {
          nl += "\n";
        }
      }
      out += nl.length > 0 ? nl : " ";
      continue;
    }
    out += c;
    i++;
  }
  return out;
}

function trimTrailingSpace(src: string): string {
  const lines = src.split("\n").map((l) => l.replace(/[ \t]+$/g, ""));
  while (lines.length > 0 && lines[lines.length - 1] === "") {
    lines.pop();
  }
  return lines.join("\n") + "\n";
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
  return trimTrailingSpace(stripComments(text));
}

export function buildDist(opts?: BuildDistOpts): BuildDistResult {
  const outDir = opts?.outDir ?? "dist";
  const headers = listSrc(".h");
  const allCpps = preferredCppOrder(listSrc(".cpp"));
  const cpps = allCpps.filter((f) => filePlatforms(f).length === 0);
  const platCpps = allCpps.filter((f) => filePlatforms(f).length > 0);
  if (headers.length === 0 || cpps.length === 0) {
    throw new Error("no src/**/*.h or src/**/*.cpp to amalgamate");
  }
  for (const p of allPlatforms) {
    if (!platCpps.some((f) => filePlatforms(f).includes(p))) {
      throw new Error(`no src/**/*_${p}.cpp to amalgamate`);
    }
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
    const body = stripInternalIncludes(rel, readLf(rel));
    if (!body.trim()) {
      continue;
    }
    headerChunks.push(`#line 1 "${rel}"`, body, "");
  }
  headerChunks.push(md4cChunk(md4cHeader, [], true));
  headerChunks.push("#endif");

  const cppTexts = new Map<string, string>();
  const fileNames = new Map<string, string[]>();
  for (const rel of [...cpps, ...platCpps]) {
    const body = stripInternalIncludes(rel, readLf(rel));
    cppTexts.set(rel, body);
    fileNames.set(rel, staticNames(body));
  }
  // Two files only clash if they can be visible at once, and the three
  // platform halves never are: Window_win.cpp and Window_linux.cpp may both
  // have a `ClientDecorated` and neither has to be renamed for it. So the
  // collision set is whatever repeats within one platform's view of the tree.
  const colliding = new Set<string>();
  for (const p of allPlatforms) {
    const visible = [...cpps, ...platCpps.filter((f) => filePlatforms(f).includes(p))];
    const nameToFiles = new Map<string, Set<string>>();
    for (const rel of visible) {
      for (const name of fileNames.get(rel) ?? []) {
        const set = nameToFiles.get(name) ?? new Set<string>();
        set.add(rel);
        nameToFiles.set(name, set);
      }
    }
    for (const [name, files] of nameToFiles) {
      if (files.size > 1) {
        colliding.add(name);
      }
    }
  }

  const chunkFor = (rel: string): string | null => {
    let body = cppTexts.get(rel) ?? "";
    const local = new Set((fileNames.get(rel) ?? []).filter((n) => colliding.has(n)));
    body = renameIdents(body, local, filePrefix(rel));
    if (!body.trim()) {
      return null;
    }
    const plats = filePlatforms(rel);
    if (plats.length === 0) {
      return [`#line 1 "${rel}"`, body, ""].join("\n");
    }
    return [guardFor(plats), `#line 1 "${rel}"`, body, "#endif", ""].join("\n");
  };

  const chunks: string[] = ['#include "gpui.h"', ""];
  for (const rel of [...cpps, ...platCpps]) {
    const chunk = chunkFor(rel);
    if (chunk) {
      chunks.push(chunk);
    }
  }
  chunks.push(md4cChunk(md4cSource, md4cCppFixes, false));

  const headerText = finish(headerChunks.join("\n"));
  const sourceText = finish(chunks.join("\n"));

  const absOut = join(root, outDir);
  mkdirSync(absOut, { recursive: true });
  const headerPath = join(absOut, "gpui.h");
  const sourcePath = join(absOut, "gpui.cpp");
  const writeIfChanged = (path: string, text: string) => {
    if (existsSync(path) && readFileSync(path, "utf8") === text) {
      return;
    }
    writeFileSync(path, text, "utf8");
  };
  writeIfChanged(headerPath, headerText);
  writeIfChanged(sourcePath, sourceText);

  return {
    outDir,
    headerPath: srcRel(headerPath),
    sourcePath: srcRel(sourcePath),
    headerBytes: Buffer.byteLength(headerText, "utf8"),
    sourceBytes: Buffer.byteLength(sourceText, "utf8"),
    headerCount: headerOrder.length + 1,
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

function parseCli(argv: string[]): { outDir: DistOutDir; bench: boolean } {
  let outDir: DistOutDir = "dist";
  let bench = true;
  const usage = "usage: bun cmd/build-dist.ts [-work] [-no-bench]";
  for (const raw of argv) {
    if (raw === "-work" || raw === "--work") {
      outDir = ".work";
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
    const what = raw.startsWith("-") ? "unknown option" : "unknown argument";
    die(`${what}: ${raw}\n${usage}`);
  }
  return { outDir, bench };
}

function main(): void {
  const { outDir, bench } = parseCli(Bun.argv.slice(2));
  const built = buildDist({ outDir });
  console.log(`wrote ${built.headerPath} (${formatBytes(built.headerBytes)}, ${built.headerCount} headers)`);
  console.log(
    `wrote ${built.sourcePath} (${formatBytes(built.sourceBytes)}, ` +
      `${built.sourceCount} + ${built.platformSourceCount} sources)`,
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

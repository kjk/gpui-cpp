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
// The published pair lives in a repo of its own and this script run by hand is
// the only thing that writes it. Nothing automatic may: the three platform
// builds, the test runner and CI all amalgamate into .work/, which is
// gitignored, so an ordinary build never touches what gets published. `outDir`
// is required for that reason — there is no default to fall into.
//
//   bun cmd/build-dist.ts              # sync, build, check, readme, publish
//   bun cmd/build-dist.ts -no-publish  # everything but the commit and push
//   bun cmd/build-dist.ts -work        # just the .work/ pair a build compiles
//
// import { buildDist } from "./build-dist.ts";
// buildDist({ outDir: ".work" });   // what a build script may do
//
// The two destinations differ. dist/ is what a reader opens: the comments are
// stripped, runs of blank lines collapse to one, and the #include lines are
// lifted out of the chunks to the top of gpui.cpp and de-duplicated, since one
// translation unit only needs each header once. .work/ is the copy every build
// compiles, and it is the sources concatenated and nothing else -- comments and
// all -- so a line in it is the line the `#line 1 "src/..."` marker above it
// says, and a debugger or a compiler diagnostic lands where you expect.

import { existsSync, mkdirSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";

const root = resolve(import.meta.dir, "..");

// Where the source lives, and where the published pair lives. The two files
// are the whole of gpui-cpp-dist: it has no history of its own worth reading,
// it is a snapshot of this repo you can drop into a project.
export const srcRepoUrl = "https://github.com/kjk/gpui-cpp";
export const srcBranch = "main";
export const distRepoUrl = "https://github.com/kjk/gpui-cpp-dist.git";
export const distRepoDir = ".work/gpui-cpp-dist";

export type DistOutDir = ".work" | typeof distRepoDir;

// The amalgam a build compiles against. cmd/build-dist.ts sets
// GPUI_AMALGAM_DIR to the dist repo checkout, so the examples get built
// against the published copy as its correctness check; every other build
// leaves it unset and uses .work/, which it regenerates itself.
export function amalgamDir(): string {
  return process.env.GPUI_AMALGAM_DIR ?? ".work";
}

export function amalgamIsWork(): boolean {
  return amalgamDir() === ".work";
}

export type Platform = "win" | "linux" | "mac";

export const allPlatforms: Platform[] = ["win", "linux", "mac"];

export type BuildDistOpts = {
  /**
   * Destination directory relative to the repo root. Required: the dist repo
   * checkout is what gets published and only `bun cmd/build-dist.ts` may write
   * it, so a build script has to say ".work" out loud.
   */
  outDir: DistOutDir;
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

// src/base.h defines all three, exactly one of them 1, so a plain #if is all a
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
    "src/base.h",
    "src/gpui/gpui.h",
    "src/gpui/assets.h",
    "src/gpui/svg.h",
    "src/base/element_ext.h",
    "src/base/lib.h",
    "src/ui/sizing.h",
    "src/ui/lib.h",
    "src/sys/sysinfo.h",
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
    if (rel === "src/base.cpp") {
      return 0;
    }
    if (rel.startsWith("src/gpui/")) {
      return 1;
    }
    if (rel.startsWith("src/base/")) {
      return 2;
    }
    if (rel.startsWith("src/ui/")) {
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

function finish(text: string, strip: boolean): string {
  return trimTrailingSpace(strip ? stripComments(text) : text);
}

const includeRe = /^[ \t]*#[ \t]*(?:include|import)[ \t]*[<"]/;
const condOpenRe = /^[ \t]*#[ \t]*(?:if|ifdef|ifndef)\b/;
const condCloseRe = /^[ \t]*#[ \t]*endif\b/;

// The #include and #import lines a chunk asks for, taken out of it. Only the
// ones at the top level of the file: an include inside a #if is there for a
// reason, and hoisting it would answer a question the source meant to ask.
function liftIncludes(body: string): { body: string; includes: string[] } {
  const kept: string[] = [];
  const includes: string[] = [];
  let depth = 0;
  for (const line of body.split("\n")) {
    if (condOpenRe.test(line)) {
      depth++;
    } else if (condCloseRe.test(line)) {
      depth--;
    } else if (depth === 0 && includeRe.test(line)) {
      includes.push(line.trim());
      continue;
    }
    kept.push(line);
  }
  return { body: kept.join("\n"), includes };
}

// <sys/stat.h> before "md4c.h", each name once, so the block reads as a list.
function sortedIncludes(lines: Iterable<string>): string[] {
  const key = (l: string) => {
    const angle = l.includes("<");
    const name = l.replace(/^[^<"]*[<"]/, "").replace(/[>"].*$/, "");
    return `${angle ? 0 : 1}${name}`;
  };
  return [...new Set(lines)].sort((a, b) => key(a).localeCompare(key(b)));
}

// Stripping the comments out of 100-odd files leaves runs of blank lines
// behind. dist/ collapses them, since it is read as one document; .work/ keeps
// them, so its line numbers stay in step with the `#line` markers the builds
// compile against.
function collapseBlankRuns(text: string): string {
  return text.replace(/\n{3,}/g, "\n\n");
}

export function buildDist(opts: BuildDistOpts): BuildDistResult {
  const outDir = opts.outDir;
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

  // dist/ lifts the includes; .work/ leaves every chunk exactly as its file
  // reads. Whatever is lifted lands in `lifted`, keyed by the guard it needs.
  // .work/ is the copy a build compiles; every other destination is published.
  const tidy = outDir !== ".work";
  const portableIncludes = new Set<string>();
  const platIncludes = new Map<string, { plats: Platform[]; lines: Set<string> }>();
  const takeIncludes = (plats: Platform[], lines: string[]) => {
    if (plats.length === 0) {
      for (const l of lines) {
        portableIncludes.add(l);
      }
      return;
    }
    const key = plats.join("+");
    const group = platIncludes.get(key) ?? { plats, lines: new Set<string>() };
    for (const l of lines) {
      group.lines.add(l);
    }
    platIncludes.set(key, group);
  };

  const chunkFor = (rel: string): string | null => {
    let body = cppTexts.get(rel) ?? "";
    const local = new Set((fileNames.get(rel) ?? []).filter((n) => colliding.has(n)));
    body = renameIdents(body, local, filePrefix(rel));
    if (!body.trim()) {
      return null;
    }
    const plats = filePlatforms(rel);
    if (tidy) {
      const lifted = liftIncludes(body);
      body = lifted.body;
      takeIncludes(plats, lifted.includes);
    }
    if (plats.length === 0) {
      return [`#line 1 "${rel}"`, body, ""].join("\n");
    }
    return [guardFor(plats), `#line 1 "${rel}"`, body, "#endif", ""].join("\n");
  };

  // The portable chunks first, then the platform ones, which is what lets a
  // platform's headers be lifted at all: <X11/Xlib.h> defines None and Window,
  // and Cocoa brings its own crowd, so they must not stand in front of the
  // portable code the way the truly portable headers can.
  const portableChunks: string[] = [];
  for (const rel of cpps) {
    const chunk = chunkFor(rel);
    if (chunk) {
      portableChunks.push(chunk);
    }
  }
  let md4c = md4cChunk(md4cSource, md4cCppFixes, false);
  if (tidy) {
    const lifted = liftIncludes(md4c);
    md4c = lifted.body;
    takeIncludes([], lifted.includes);
  }
  const platformChunks: string[] = [];
  for (const rel of platCpps) {
    const chunk = chunkFor(rel);
    if (chunk) {
      platformChunks.push(chunk);
    }
  }

  // Comments do not survive into dist/, so the blocks below carry none: what
  // they are is documented at the top of this script instead.
  const chunks: string[] = ['#include "gpui.h"'];
  if (portableIncludes.size > 0) {
    chunks.push("", ...sortedIncludes(portableIncludes));
  }
  chunks.push("");
  chunks.push(...portableChunks);
  for (const group of platIncludes.values()) {
    // A header the portable block already pulled in unconditionally does not
    // need pulling in again behind a guard.
    const lines = [...group.lines].filter((l) => !portableIncludes.has(l));
    if (lines.length === 0) {
      continue;
    }
    chunks.push(guardFor(group.plats), ...sortedIncludes(lines), "#endif", "");
  }
  chunks.push(...platformChunks);
  chunks.push(md4c);

  const headerText = tidy
    ? collapseBlankRuns(finish(headerChunks.join("\n"), true))
    : finish(headerChunks.join("\n"), false);
  const sourceText = tidy ? collapseBlankRuns(finish(chunks.join("\n"), true)) : finish(chunks.join("\n"), false);

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

function formatBytes(n: number): string {
  if (n < 1024) {
    return `${n} b`;
  }
  if (n < 1024 * 1024) {
    return `${(n / 1024).toFixed(1)} KB`;
  }
  return `${(n / (1024 * 1024)).toFixed(2)} MB`;
}

function die(msg: string): never {
  console.error(msg);
  process.exit(1);
}

function run(cmd: string[], opts?: { cwd?: string; env?: Record<string, string> }): number {
  const r = Bun.spawnSync(cmd, {
    cwd: opts?.cwd ?? root,
    env: { ...process.env, ...(opts?.env ?? {}) },
    stdout: "inherit",
    stderr: "inherit",
  });
  return r.exitCode ?? 1;
}

function capture(cmd: string[], cwd?: string): string {
  const r = Bun.spawnSync(cmd, { cwd: cwd ?? root, stdout: "pipe", stderr: "pipe" });
  return new TextDecoder().decode(r.stdout).trim();
}

// Clone the dist repo on the first run, fast-forward it after that. A failure
// to reach GitHub is a warning, not the end: the amalgam still builds against
// whatever is on disk.
function syncDistRepo(): void {
  const abs = join(root, distRepoDir);
  if (!existsSync(join(abs, ".git"))) {
    console.log(`cloning ${distRepoUrl} into ${distRepoDir}`);
    mkdirSync(dirname(abs), { recursive: true });
    if (run(["git", "clone", distRepoUrl, distRepoDir]) !== 0) {
      die(`git clone ${distRepoUrl} failed`);
    }
    return;
  }
  console.log(`updating ${distRepoDir}`);
  if (run(["git", "-C", distRepoDir, "pull", "--ff-only"]) !== 0) {
    console.error(`warning: could not fast-forward ${distRepoDir}; using what is on disk`);
  }
}

// Everything above this line in the readme is written once and left alone;
// everything from it down says which commit this copy came from.
const readmeMarker = "## This copy";

const readmeHead = `# gpui-cpp-dist

The single-file build of [gpui-cpp](${srcRepoUrl}): \`gpui.h\` and \`gpui.cpp\`,
amalgamated from that repo's \`src/**\` plus the vendored md4c by its
\`cmd/build-dist.ts\`. Nothing here is written by hand, so issues and pull
requests belong in the source repo.

## Use it

Drop both files into your tree, \`#include "gpui.h"\` where you need the API,
and compile \`gpui.cpp\` as one more source file. It is C++20, and the platform
halves are already inside it behind \`GPUI_OS_*\` guards, so the same pair
builds on all three:

- **Windows** — \`cl /std:c++20 /EHsc /utf-8 /DUNICODE /D_UNICODE\`, static CRT;
  links against the Win32, Direct2D and DirectWrite import libraries.
- **Linux** — \`g++ -std=c++20\` with \`pkg-config --cflags --libs x11 cairo pangocairo\`.
- **macOS** — \`clang++ -std=c++20 -x objective-c++\` with the Cocoa, CoreText and
  IOKit frameworks. The file is Objective-C++ because the mac half is.

No other dependencies, no build system, no STL containers.`;

function writeDistReadme(sha: string, subject: string): string {
  const abs = join(root, distRepoDir, "readme.md");
  const short = sha.slice(0, 12);
  const head = existsSync(abs) ? (readFileSync(abs, "utf8").split(readmeMarker)[0] ?? "").trimEnd() : readmeHead;
  const body = [
    readmeMarker,
    "",
    `Amalgamated from gpui-cpp [\`${short}\`](${srcRepoUrl}/commit/${sha}) — ${subject}`,
    "",
    `[What has changed in gpui-cpp since](${srcRepoUrl}/compare/${sha}...${srcBranch})`,
    "shows every commit this copy is behind by; if that page is empty, it is current.",
    "",
  ].join("\n");
  const text = `${head}\n\n${body}`;
  writeFileSync(abs, text, "utf8");
  return srcRel(abs);
}

// Build every example against the amalgam that was just written, which is the
// only check that matters: it is what someone downloading these two files
// does. GPUI_AMALGAM_DIR points the platform build at this copy instead of
// .work/, and its objects go to their own out/ tree.
function checkExamples(): void {
  const script =
    process.platform === "win32"
      ? "cmd/build-windows.ts"
      : process.platform === "darwin"
        ? "cmd/build-mac.ts"
        : "cmd/build-linux.ts";
  console.log(`building every example against ${distRepoDir}`);
  const code = run(["bun", script, "-rel", "-all"], { env: { GPUI_AMALGAM_DIR: distRepoDir } });
  if (code !== 0) {
    die(`the examples do not build against ${distRepoDir} (exit ${code})`);
  }
}

// The commit that carries this snapshot. The message names the source commit
// in full, so the dist repo's log reads as a list of what it is a copy of.
function publishDistRepo(sha: string): void {
  const cwd = join(root, distRepoDir);
  const msg = `updating to ${srcRepoUrl} ${sha}`;
  if (run(["git", "add", "-A"], { cwd }) !== 0) {
    die("git add failed");
  }
  if (run(["git", "commit", "-m", msg], { cwd }) !== 0) {
    die("git commit failed");
  }
  // -u origin HEAD, so the first push of a fresh clone sets its upstream and
  // every push after it is the same command.
  if (run(["git", "push", "-u", "origin", "HEAD"], { cwd }) !== 0) {
    die("git push failed");
  }
  console.log(`published ${distRepoDir}: ${msg}`);
}

function parseCli(argv: string[]): {
  outDir: DistOutDir;
  check: boolean;
  sync: boolean;
  publish: boolean;
} {
  let outDir: DistOutDir = distRepoDir;
  let check = true;
  let sync = true;
  let publish = true;
  const usage = "usage: bun cmd/build-dist.ts [-work] [-no-check] [-no-sync] [-no-publish]";
  for (const raw of argv) {
    if (raw === "-work" || raw === "--work") {
      outDir = ".work";
      check = false;
      sync = false;
      publish = false;
      continue;
    }
    if (raw === "-no-publish" || raw === "--no-publish") {
      publish = false;
      continue;
    }
    if (raw === "-no-check" || raw === "--no-check") {
      check = false;
      continue;
    }
    if (raw === "-no-sync" || raw === "--no-sync") {
      sync = false;
      continue;
    }
    const what = raw.startsWith("-") ? "unknown option" : "unknown argument";
    die(`${what}: ${raw}\n${usage}`);
  }
  return { outDir, check, sync, publish };
}

function main(): void {
  const { outDir, check, sync, publish } = parseCli(Bun.argv.slice(2));
  if (sync) {
    syncDistRepo();
  }
  const built = buildDist({ outDir });
  console.log(`wrote ${built.headerPath} (${formatBytes(built.headerBytes)}, ${built.headerCount} headers)`);
  console.log(
    `wrote ${built.sourcePath} (${formatBytes(built.sourceBytes)}, ` +
      `${built.sourceCount} + ${built.platformSourceCount} sources)`,
  );
  if (check) {
    checkExamples();
  }
  if (outDir !== distRepoDir) {
    return;
  }
  const sha = capture(["git", "rev-parse", "HEAD"]);
  const subject = capture(["git", "log", "-1", "--pretty=%s"]);
  const readme = writeDistReadme(sha, subject);
  console.log(`wrote ${readme} for ${sha.slice(0, 12)}`);
  if (capture(["git", "branch", "-r", "--contains", sha]) === "") {
    console.error(
      `warning: ${sha.slice(0, 12)} is not on any remote yet, so the links in ` +
        "the readme stay broken until gpui-cpp is pushed",
    );
  }
  const dirty = capture(["git", "status", "--porcelain"], join(root, distRepoDir));
  if (dirty === "") {
    console.log(`${distRepoDir} is unchanged; nothing to publish`);
    return;
  }
  if (!publish) {
    console.log(`${distRepoDir} is ready; -no-publish, so it is not committed`);
    return;
  }
  publishDistRepo(sha);
}

if (import.meta.main) {
  process.chdir(root);
  main();
}

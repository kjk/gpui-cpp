// Side-by-side story compare: Rust original (left) vs C++ port (right).
//
//   bun cmd/compare-story.ts
//   bun cmd/compare-story.ts introduction
//   bun cmd/compare-story.ts -rel accordion
//   bun cmd/compare-story.ts -nobuild introduction
//
// Screenshots: out/compare-story/<slug>-rust.png and <slug>-cpp.png

import { existsSync, mkdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import {
  captureWindowToPng,
  getWindowRect,
  getWorkArea,
  killAndWait,
  moveWindow,
  setCursorPos,
  setForegroundWindow,
  setProcessDpiAware,
  sleep,
  waitForPidWindow,
} from "./winapi.ts";
import { ensureRustTree, rustTreeDir } from "./versions.ts";

const root = resolve(dirname(Bun.main), "..");
process.chdir(root);

const slugs = [
  "introduction",
  "accordion",
  "alert",
  "alert-dialog",
  "avatar",
  "badge",
  "breadcrumb",
  "button",
  "calendar",
  "chart",
  "checkbox",
  "clipboard",
  "collapsible",
  "color-picker",
  "combobox",
  "data-table",
  "date-picker",
  "description-list",
  "dialog",
  "dropdown-button",
  "editor",
  "form",
  "group-box",
  "hover-card",
  "icon",
  "image",
  "input",
  "kbd",
  "label",
  "list",
  "menu",
  "native-menu",
  "notification",
  "number-input",
  "otp-input",
  "pagination",
  "popover",
  "progress",
  "radio",
  "rating",
  "resizable",
  "scrollbar",
  "select",
  "separator",
  "settings",
  "sheet",
  "sidebar",
  "skeleton",
  "slider",
  "spinner",
  "status-bar",
  "stepper",
  "switch",
  "table",
  "tabs",
  "tag",
  "textarea",
  "theme-colors",
  "toggle",
  "tooltip",
  "tree",
  "virtual-list",
] as const;

function die(msg?: string): never {
  if (msg) {
    console.error(msg);
  }
  console.error("Usage: bun cmd/compare-story.ts [-rel|-dbg] [-nobuild] [slug]");
  process.exit(1);
}

function parseArgs(argv: string[]): { debug: boolean; nobuild: boolean; pages: string[] } {
  let debug = false;
  let nobuild = false;
  const pages: string[] = [];
  for (const raw of argv) {
    if (raw === "-rel") {
      debug = false;
      continue;
    }
    if (raw === "-dbg") {
      debug = true;
      continue;
    }
    if (raw === "-nobuild") {
      nobuild = true;
      continue;
    }
    if (raw.startsWith("-")) {
      die(`Unknown flag: ${raw}`);
    }
    pages.push(raw.toLowerCase());
  }
  return { debug, nobuild, pages: pages.length ? pages : ["introduction"] };
}

function rustDir(): string {
  return rustTreeDir(root);
}

function rustExe(debug: boolean): string {
  return join(rustDir(), "target", debug ? "debug" : "release", "gpui-component-story.exe");
}

// Rust Gallery::set_active_story matches Story::title(), not the kebab slug.
// "alert-dialog" does not contain-match "AlertDialog" and blanks the page.
function rustStoryArg(slug: string): string {
  if (slug === "theme-colors") {
    return "Theme Colors";
  }
  if (slug === "introduction") {
    return "Introduction";
  }
  return slug
    .split("-")
    .map((part) => (part ? part[0].toUpperCase() + part.slice(1) : part))
    .join("");
}

function cppExe(debug: boolean): string {
  return join(root, "out", debug ? "dbg" : "rel", "story.exe");
}

function run(cmd: string[], cwd: string): number {
  const r = Bun.spawnSync(cmd, { cwd, stdout: "inherit", stderr: "inherit" });
  return r.exitCode ?? 1;
}

function placePair(rustHwnd: number, cppHwnd: number) {
  const wa = getWorkArea();
  const rust = getWindowRect(rustHwnd);
  const rustW = rust.right - rust.left;
  const rustH = rust.bottom - rust.top;
  const cpp = getWindowRect(cppHwnd);
  const cppW = cpp.right - cpp.left;
  const cppH = cpp.bottom - cpp.top;
  const y = wa.top + 8;
  moveWindow(rustHwnd, wa.left + 8, y, rustW, rustH);
  moveWindow(cppHwnd, wa.left + 8 + rustW + 8, y, cppW, cppH);
}

const { debug, nobuild, pages } = parseArgs(Bun.argv.slice(2));
setProcessDpiAware();

let rustRoot: string;
try {
  rustRoot = ensureRustTree(root);
} catch (e) {
  die(e instanceof Error ? e.message : String(e));
}

if (!nobuild) {
  if (run(["bun", "cmd/build.ts", debug ? "-dbg" : "-rel", "story"], root) !== 0) {
    process.exit(1);
  }
}

if (!existsSync(cppExe(debug))) {
  die(`Missing ${cppExe(debug)}`);
}
if (!existsSync(rustExe(debug))) {
  die(`Missing ${rustExe(debug)}. Build with: cargo build -p gpui-component-story`);
}

const outDir = join(root, "out", "compare-story");
mkdirSync(outDir, { recursive: true });

for (const slug of pages) {
  if (!(slugs as readonly string[]).includes(slug)) {
    die(`Unknown story slug: ${slug}`);
  }
  console.log(`\n=== ${slug} ===`);
  const rustProc = Bun.spawn([rustExe(debug), rustStoryArg(slug)], {
    cwd: rustDir(),
    stdout: "ignore",
    stderr: "ignore",
  });
  const cppProc = Bun.spawn([cppExe(debug), slug], {
    cwd: join(root, "out", debug ? "dbg" : "rel"),
    stdout: "ignore",
    stderr: "ignore",
  });
  const rustHwnd = await waitForPidWindow(rustProc.pid ?? 0, 30000);
  const cppHwnd = await waitForPidWindow(cppProc.pid ?? 0, 15000);
  if (!rustHwnd || !cppHwnd) {
    await killAndWait(rustProc);
    await killAndWait(cppProc);
    die(`window did not appear (rust=${!!rustHwnd} cpp=${!!cppHwnd})`);
  }
  placePair(rustHwnd, cppHwnd);
  const wa = getWorkArea();
  setCursorPos(wa.left + 4, wa.top + 4);
  setForegroundWindow(rustHwnd);
  setForegroundWindow(cppHwnd);
  await sleep(500);
  const rustPng = join(outDir, `${slug}-rust.png`);
  const cppPng = join(outDir, `${slug}-cpp.png`);
  captureWindowToPng(rustHwnd, rustPng);
  captureWindowToPng(cppHwnd, cppPng);
  console.log(`  ${rustPng}`);
  console.log(`  ${cppPng}`);
  await killAndWait(rustProc);
  await killAndWait(cppProc);
  await sleep(150);
}

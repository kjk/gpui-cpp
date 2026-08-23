// Drive the same input at the Rust story app and at ours, and photograph both
// after each step. cmd/compare-story.ts answers "does the page look the same
// when it opens"; this one answers "does it behave the same once you touch it",
// which is where the differences that a static sweep cannot see live: a menu
// that opens with different rows, a dropdown that does not close, a list that
// scrolls a different distance, a field that takes a keystroke differently.
//
//   bun cmd/compare-ui.ts menu click:120,200 shot:open
//   bun cmd/compare-ui.ts -nobuild select click:200,180 shot:open key:40 key:13 shot:picked
//
// Steps, applied left to right, to each app in turn (so a hover step can own
// the one real cursor the desktop has):
//
//   click:X,Y            left press + release at client X,Y
//   rclick:X,Y           the same with the secondary button
//   hover:X,Y            park the pointer there
//   move:X,Y             a WM_MOUSEMOVE without moving the cursor
//   drag:X1,Y1,X2,Y2     press, eight moves, release
//   wheel:N[@X,Y]        N notches (negative scrolls down) at the window
//                        centre, or at X,Y when it is given
//   key:VK[+mods]        keydown+keyup; mods are ctrl/shift/alt
//   type:TEXT            WM_CHAR per character
//   wait:MS              sit still
//   shot:NAME            capture both windows as <slug>-NN-NAME-{rust,cpp}.png
//
// A step list with no shot: gets one at the end anyway, so the common case is
// just the clicks. Shots land in out/compare-ui/.
//
// Both windows are the same size (the halves cmd/compare-story.ts uses), so a
// client coordinate means the same thing in both -- which is the whole point:
// the same number drives both apps, and a click that lands on a different
// widget on one side is itself the finding.

import { existsSync, mkdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import {
  captureWindowToPng,
  clickClient,
  clientToScreen,
  getClientRect,
  getWorkArea,
  hoverClient,
  killAndWait,
  packCoords,
  placeOnWorkAreaHalf,
  rightClickClient,
  sendMessage,
  setCursorPos,
  setForegroundWindow,
  setProcessDpiAware,
  sleep,
  waitForForeground,
  waitForPidWindow,
  workAreaHalfRect,
} from "./winapi.ts";
import { ensureRustTree, rustTreeDir } from "./versions.ts";

const root = resolve(dirname(Bun.main), "..");
process.chdir(root);

const WM_KEYDOWN = 0x0100;
const WM_KEYUP = 0x0101;
const WM_CHAR = 0x0102;
const WM_MOUSEMOVE = 0x0200;
const WM_LBUTTONDOWN = 0x0201;
const WM_LBUTTONUP = 0x0202;
const WM_MOUSEWHEEL = 0x020a;

const VK_CONTROL = 0x11;
const VK_SHIFT = 0x10;
const VK_MENU = 0x12;

type Step =
  | { kind: "click"; x: number; y: number; right?: boolean }
  | { kind: "hover"; x: number; y: number }
  | { kind: "move"; x: number; y: number }
  | { kind: "drag"; x1: number; y1: number; x2: number; y2: number }
  | { kind: "wheel"; n: number; x?: number; y?: number }
  | { kind: "key"; vk: number; ctrl: boolean; shift: boolean; alt: boolean }
  | { kind: "type"; text: string }
  | { kind: "wait"; ms: number }
  | { kind: "shot"; name: string };

function die(msg?: string): never {
  if (msg) {
    console.error(msg);
  }
  console.error("Usage: bun cmd/compare-ui.ts [-dbg] [-nobuild] <slug> [step ...]");
  process.exit(1);
}

function nums(s: string): number[] {
  return s.split(",").map((v) => Number(v.trim()));
}

function parseStep(raw: string): Step {
  const at = raw.indexOf(":");
  const kind = at < 0 ? raw : raw.slice(0, at);
  const arg = at < 0 ? "" : raw.slice(at + 1);
  switch (kind) {
    case "click":
    case "rclick": {
      const [x, y] = nums(arg);
      return { kind: "click", x: x ?? 0, y: y ?? 0, right: kind === "rclick" };
    }
    case "hover":
    case "move": {
      const [x, y] = nums(arg);
      return { kind, x: x ?? 0, y: y ?? 0 };
    }
    case "drag": {
      const [x1, y1, x2, y2] = nums(arg);
      return { kind, x1: x1 ?? 0, y1: y1 ?? 0, x2: x2 ?? 0, y2: y2 ?? 0 };
    }
    case "wheel": {
      const cut = arg.indexOf("@");
      if (cut < 0) {
        return { kind, n: Number(arg) || 0 };
      }
      const [x, y] = nums(arg.slice(cut + 1));
      return { kind, n: Number(arg.slice(0, cut)) || 0, x: x ?? 0, y: y ?? 0 };
    }
    case "key": {
      const parts = arg.split("+");
      const vk = Number(parts[0]);
      const mods = parts.slice(1).map((m) => m.toLowerCase());
      return {
        kind,
        vk: Number.isFinite(vk) ? vk : 0,
        ctrl: mods.includes("ctrl"),
        shift: mods.includes("shift"),
        alt: mods.includes("alt"),
      };
    }
    case "type":
      return { kind, text: arg };
    case "wait":
      return { kind, ms: Number(arg) || 0 };
    case "shot":
      return { kind, name: arg || "shot" };
    default:
      return die(`Unknown step: ${raw}`);
  }
}

async function applyStep(hwnd: number, s: Step, shot: (name: string) => void): Promise<void> {
  switch (s.kind) {
    case "click":
      await (s.right ? rightClickClient(hwnd, s.x, s.y, 250) : clickClient(hwnd, s.x, s.y, 250));
      return;
    case "move":
      sendMessage(hwnd, WM_MOUSEMOVE, 0, packCoords(s.x, s.y));
      await sleep(150);
      return;
    case "hover":
      await hoverClient(hwnd, s.x, s.y, 300);
      return;
    case "drag": {
      sendMessage(hwnd, WM_LBUTTONDOWN, 1, packCoords(s.x1, s.y1));
      await sleep(60);
      for (let i = 1; i <= 8; i++) {
        const x = Math.round(s.x1 + ((s.x2 - s.x1) * i) / 8);
        const y = Math.round(s.y1 + ((s.y2 - s.y1) * i) / 8);
        sendMessage(hwnd, WM_MOUSEMOVE, 1, packCoords(x, y));
        await sleep(20);
      }
      sendMessage(hwnd, WM_LBUTTONUP, 0, packCoords(s.x2, s.y2));
      await sleep(200);
      return;
    }
    case "wheel": {
      const r = getClientRect(hwnd);
      const cx = s.x ?? Math.floor(r.right / 2);
      const cy = s.y ?? Math.floor(r.bottom / 2);
      const pt = clientToScreen(hwnd, cx, cy);
      const step = s.n > 0 ? 120 : -120;
      for (let i = 0; i < Math.abs(s.n); i++) {
        sendMessage(hwnd, WM_MOUSEWHEEL, (step << 16) >>> 0, packCoords(pt.x, pt.y));
        await sleep(40);
      }
      await sleep(250);
      return;
    }
    case "key": {
      const down: number[] = [];
      if (s.ctrl) down.push(VK_CONTROL);
      if (s.shift) down.push(VK_SHIFT);
      if (s.alt) down.push(VK_MENU);
      for (const m of down) {
        sendMessage(hwnd, WM_KEYDOWN, m, 0);
      }
      sendMessage(hwnd, WM_KEYDOWN, s.vk, 0);
      await sleep(40);
      sendMessage(hwnd, WM_KEYUP, s.vk, 0xc0000001);
      for (const m of down.reverse()) {
        sendMessage(hwnd, WM_KEYUP, m, 0xc0000001);
      }
      await sleep(150);
      return;
    }
    case "type":
      for (const chr of s.text) {
        sendMessage(hwnd, WM_CHAR, chr.codePointAt(0) ?? 0, 1);
        await sleep(50);
      }
      await sleep(120);
      return;
    case "wait":
      await sleep(s.ms);
      return;
    case "shot":
      shot(s.name);
      return;
  }
}

// Rust Gallery::set_active_story matches Story::title(), not the kebab slug.
function rustStoryArg(slug: string): string {
  if (slug === "theme-colors") return "Theme Colors";
  if (slug === "introduction") return "Introduction";
  return slug
    .split("-")
    .map((p) => (p ? p[0]!.toUpperCase() + p.slice(1) : p))
    .join("");
}

const argv = Bun.argv.slice(2);
let debug = false;
let nobuild = false;
const rest: string[] = [];
for (const a of argv) {
  if (a === "-dbg") debug = true;
  else if (a === "-rel") debug = false;
  else if (a === "-nobuild") nobuild = true;
  else if (a.startsWith("-")) die(`Unknown flag: ${a}`);
  else rest.push(a);
}
const slug = rest[0];
if (!slug) die();
const steps = rest.slice(1).map(parseStep);
if (!steps.some((s) => s.kind === "shot")) {
  steps.push({ kind: "shot", name: "end" });
}

setProcessDpiAware();
let rustRoot: string;
try {
  rustRoot = ensureRustTree(root);
} catch (e) {
  die(e instanceof Error ? e.message : String(e));
}
void rustRoot;

if (!nobuild) {
  const b = Bun.spawnSync(["bun", "cmd/build.ts", debug ? "-dbg" : "-rel", "story"], {
    cwd: root,
    stdout: "inherit",
    stderr: "inherit",
  });
  if ((b.exitCode ?? 1) !== 0) process.exit(1);
}

const rustExe = join(rustTreeDir(root), "target", debug ? "debug" : "release", "gpui-component-story.exe");
const cppExe = join(root, "out", debug ? "dbg" : "rel", "story.exe");
if (!existsSync(rustExe)) die(`Missing ${rustExe}. Build with: cargo build -p gpui-component-story`);
if (!existsSync(cppExe)) die(`Missing ${cppExe}`);

const outDir = join(root, "out", "compare-ui");
mkdirSync(outDir, { recursive: true });

const half = workAreaHalfRect("right");
const rustProc = Bun.spawn([rustExe, rustStoryArg(slug!)], {
  cwd: rustTreeDir(root),
  stdout: "ignore",
  stderr: "ignore",
});
const cppProc = Bun.spawn([cppExe, `-gpui-window=${half.x},${half.y},${half.w},${half.h}`, slug!], {
  cwd: join(root, "out", debug ? "dbg" : "rel"),
  stdout: "ignore",
  stderr: "ignore",
});
const rustHwnd = await waitForPidWindow(rustProc.pid ?? 0, 30000);
const cppHwnd = await waitForPidWindow(cppProc.pid ?? 0, 15000);
if (!rustHwnd || !cppHwnd) {
  await Promise.all([killAndWait(rustProc), killAndWait(cppProc)]);
  die(`window did not appear (rust=${!!rustHwnd} cpp=${!!cppHwnd})`);
}
placeOnWorkAreaHalf(rustHwnd, "left");
placeOnWorkAreaHalf(cppHwnd, "right");
await sleep(600);

// One side at a time: a hover step needs the one cursor the desktop has, and
// an app that only repaints when it is active would otherwise be photographed
// mid-nap.
async function drive(hwnd: number, tag: "rust" | "cpp"): Promise<void> {
  setForegroundWindow(hwnd);
  await waitForForeground(hwnd, 3000);
  await sleep(400);
  let n = 0;
  const shot = (name: string) => {
    n++;
    const p = join(outDir, `${slug}-${String(n).padStart(2, "0")}-${name}-${tag}.png`);
    captureWindowToPng(hwnd, p);
    console.log(`  ${p}`);
  };
  for (const s of steps) {
    await applyStep(hwnd, s, shot);
  }
}

console.log(`
=== ${slug} ===`);
const wa = getWorkArea();
setCursorPos(wa.left + 4, wa.top + 4);
await drive(rustHwnd, "rust");
setCursorPos(wa.left + 4, wa.top + 4);
await drive(cppHwnd, "cpp");
await Promise.all([killAndWait(rustProc), killAndWait(cppProc)]);

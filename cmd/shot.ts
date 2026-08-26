// Screenshot one example window: bun cmd/shot.ts [-dbg] <example> [out.png]
//   -click=X,Y   click at client coords first
//   -rclick=X,Y  the same with the secondary button
//   -hover=X,Y   leave the pointer there, for capturing a hover state
//   -settle=MS   wait that long after the input before the shutter, for a
//                widget that answers on a timer (a hover card's open delay)
//   -clickwait=MS  how long to wait after each click before the next input
//                (default 200) — lower it to catch an animation mid-flight
//   -drag=X1,Y1,X2,Y2  press, move, release: a text selection drag
//   -draghold=X1,Y1,X2,Y2  the same without the release, so the shutter
//                catches what a drag looks like while it is in flight
//   -wheel=N     N notches of scroll at the window centre. Runs before the
//                clicks, so a click coordinate is read off the scrolled page
//   -key=VK      send a key: the down and the up, since Enter and Space
//                activate a focused element from the release
//   -type=TEXT   type the characters: WM_CHAR each, which is how a digit or a
//                letter reaches a field. -key carries the key, not the text it
//                produced, so a printable character needs this one. Typed
//                before -key, so `-type=42 -key=8` reads left to right.
//   -half=left|right  size the window the way cmd/compare-story.ts does, so a
//                     shot lines up with the Rust side-by-side pair
import { dirname, join, resolve } from "node:path";
import { mkdirSync } from "node:fs";
import {
  captureWindowToPng,
  clientToScreen,
  getClientRect,
  packCoords,
  clickClient,
  rightClickClient,
  getCursorPos,
  getForegroundWindow,
  getWindowText,
  hoverClient,
  killAndWait,
  parkCursorOutside,
  placeOnWorkAreaHalf,
  workAreaHalfRect,
  sendMessage,
  setCursorPos,
  setProcessDpiAware,
  sleep,
  waitForForeground,
  waitForPidWindow,
} from "./winapi.ts";
import type { WorkAreaHalf } from "./winapi.ts";

const root = resolve(dirname(Bun.main), "..");
process.chdir(root);

const argv = Bun.argv.slice(2);
let debug = false;
let noBuild = false;
const clicks: { x: number; y: number; right?: boolean }[] = [];
let hover: { x: number; y: number } | null = null;
let typed = "";
let settleMs = 0;
let clickWaitMs = 200;
let drag: { x1: number; y1: number; x2: number; y2: number } | null = null;
let dragHold = false;
const keys: number[] = [];
let wheel = 0;
let half: WorkAreaHalf | null = null;
const rest: string[] = [];
for (const a of argv) {
  if (a === "-dbg") {
    debug = true;
  } else if (a === "-rel") {
    debug = false;
  } else if (a.startsWith("-wheel=")) {
    wheel = Number(a.slice(7));
  } else if (a.startsWith("-half=")) {
    const side = a.slice(6);
    if (side !== "left" && side !== "right") {
      console.error("-half takes left or right");
      process.exit(1);
    }
    half = side;
  } else if (a.startsWith("-key=")) {
    keys.push(Number(a.slice(5)));
  } else if (a.startsWith("-type=")) {
    typed = a.slice(6);
  } else if (a.startsWith("-drag=")) {
    const [x1, y1, x2, y2] = a.slice(6).split(",").map(Number);
    drag = { x1: x1 ?? 0, y1: y1 ?? 0, x2: x2 ?? 0, y2: y2 ?? 0 };
  } else if (a.startsWith("-draghold=")) {
    const [x1, y1, x2, y2] = a.slice(10).split(",").map(Number);
    drag = { x1: x1 ?? 0, y1: y1 ?? 0, x2: x2 ?? 0, y2: y2 ?? 0 };
    dragHold = true;
  } else if (a.startsWith("-settle=")) {
    settleMs = Number(a.slice(8)) || 0;
  } else if (a.startsWith("-clickwait=")) {
    clickWaitMs = Number(a.slice(11)) || 0;
  } else if (a.startsWith("-hover=")) {
    const [x, y] = a.slice(7).split(",").map(Number);
    hover = { x: x ?? 0, y: y ?? 0 };
  } else if (a.startsWith("-click=")) {
    const [x, y] = a.slice(7).split(",").map(Number);
    clicks.push({ x: x ?? 0, y: y ?? 0 });
  } else if (a.startsWith("-rclick=")) {
    const [x, y] = a.slice(8).split(",").map(Number);
    clicks.push({ x: x ?? 0, y: y ?? 0, right: true });
  } else if (a === "-nobuild" || a === "--nobuild") {
    noBuild = true;
  } else {
    rest.push(a);
  }
}
const name = rest[0];
if (!name) {
  console.error("Usage: bun cmd/shot.ts [-dbg] [-nobuild] <example> [out.png] [args...]");
  process.exit(1);
}
const outDir = join(root, "out", "shots");
mkdirSync(outDir, { recursive: true });
const dst = rest[1] ?? join(outDir, `${name}.png`);
const exeDir = join(root, "out", debug ? "dbg" : "rel");
const exe = join(exeDir, `${name}.exe`);

// Build first, the way cmd/test.ts does. A screenshot is evidence about the
// code in the tree, and shooting a stale binary makes it evidence about
// something else — which is exactly how a layout regression once passed a
// whole sweep of "identical" captures. Skip with -nobuild when the caller
// has just built, or is deliberately shooting an older binary.
if (!noBuild) {
  const build = Bun.spawnSync(["bun", join("cmd", "build.ts"), debug ? "-dbg" : "-rel", name], {
    cwd: root,
    stdout: "pipe",
    stderr: "inherit",
  });
  if (build.exitCode !== 0) {
    console.error(new TextDecoder().decode(build.stdout));
    console.error(`build failed for ${name}`);
    process.exit(build.exitCode ?? 1);
  }
}

setProcessDpiAware();
// AppLog writes out\gpui.log relative to cwd.
mkdirSync(join(exeDir, "out"), { recursive: true });
const logPath = join(exeDir, "out", "gpui.log");
try {
  await Bun.file(logPath).delete();
} catch {}
// -gpui-window asks the app to open at the rect rather than be moved into it
// afterwards, which saves a second layout of the whole tree. The runtime takes
// the flag out of argv before the example parses it. placeOnWorkAreaHalf below
// still runs and is a no-op when this worked.
const geom = half ? [`-gpui-window=${((r) => `${r.x},${r.y},${r.w},${r.h}`)(workAreaHalfRect(half))}`] : [];
const proc = Bun.spawn([exe, ...geom, ...rest.slice(2)], { cwd: exeDir, stdout: "pipe", stderr: "pipe" });
const hwnd = await waitForPidWindow(proc.pid ?? 0, 15000);
if (!hwnd) {
  await killAndWait(proc);
  console.error("window did not appear");
  process.exit(1);
}
// Before anything reads client coordinates: compare-story places the two
// windows on halves of the work area, and a shot meant to line up with that
// pair has to be the same size. Same helper, so it is the same rect.
if (half) {
  placeOnWorkAreaHalf(hwnd, half);
}
// Clicks and keys below want an active window; the shot does too, but that is
// checked again just before the capture.
await waitForForeground(hwnd, 3000);
const cursorWas = getCursorPos();
await sleep(500);
// Scroll before keys/capture: notches of WM_MOUSEWHEEL at the window centre.
if (wheel !== 0) {
  const r = getClientRect(hwnd);
  const pt = clientToScreen(hwnd, Math.floor(r.right / 2), Math.floor(r.bottom / 2));
  const step = wheel > 0 ? 120 : -120;
  for (let i = 0; i < Math.abs(wheel); i++) {
    sendMessage(hwnd, 0x020a /* WM_MOUSEWHEEL */, (step << 16) >>> 0, packCoords(pt.x, pt.y));
    await sleep(40);
  }
  await sleep(250);
}
for (const c of clicks) {
  await (c.right ? rightClickClient(hwnd, c.x, c.y, clickWaitMs) : clickClient(hwnd, c.x, c.y, clickWaitMs));
}
// A selection drag: press, a few moves so the app sees the path, release.
if (drag) {
  const steps = 8;
  sendMessage(hwnd, 0x0201 /* WM_LBUTTONDOWN */, 1, packCoords(drag.x1, drag.y1));
  await sleep(60);
  for (let i = 1; i <= steps; i++) {
    const x = Math.round(drag.x1 + ((drag.x2 - drag.x1) * i) / steps);
    const y = Math.round(drag.y1 + ((drag.y2 - drag.y1) * i) / steps);
    sendMessage(hwnd, 0x0200 /* WM_MOUSEMOVE */, 1, packCoords(x, y));
    await sleep(20);
  }
  if (!dragHold) {
    sendMessage(hwnd, 0x0202 /* WM_LBUTTONUP */, 0, packCoords(drag.x2, drag.y2));
  }
  await sleep(200);
}

// The text before the keys, so `-type=42 -key=8` reads left to right: a digit
// or a letter reaches a field as a WM_CHAR, which is not what -key sends, and
// -key is then the Enter or the backspace that follows what was typed.
for (const chr of typed) {
  sendMessage(hwnd, 0x0102 /* WM_CHAR */, chr.codePointAt(0) ?? 0, 1);
  await sleep(60);
}
for (const vk of keys) {
  sendMessage(hwnd, 0x0100 /* WM_KEYDOWN */, vk, 0);
  await sleep(40);
  // Bit 31 of lParam is what a real WM_KEYUP carries.
  sendMessage(hwnd, 0x0101 /* WM_KEYUP */, vk, 0xc0000001);
  await sleep(120);
}
// Last, so the pointer is still on the element when the frame is captured.
if (hover) {
  if (!(await hoverClient(hwnd, hover.x, hover.y))) {
    // Saying so beats a shot that quietly shows no hover at all: the window
    // asks Windows to tell it when the pointer leaves, and a pointer that was
    // never there has already left.
    console.error(
      "warning: the pointer could not be placed (the session is probably locked); hover states will not show",
    );
  }
} else if (dragHold) {
  // The button is still down: moving the real cursor would send the window a
  // move of its own and drag the held thing away from where it was left.
} else {
  // Nothing asked for a pointer, so make sure there isn't one over the window:
  // where it happens to rest is not something a screenshot should depend on.
  if (parkCursorOutside(hwnd)) {
    // It was over the window: give the app a moment to repaint without a
    // hover state before the shutter.
    await sleep(150);
  }
}
// A widget whose answer is on a timer -- a hover card counting down its open
// delay -- has not answered yet when the pointer stops moving.
if (settleMs > 0) {
  await sleep(settleMs);
}
// The foreground can move away while the clicks and keys above play out, and
// DWM composites a window that lost it with the inactive caption shade -- a
// diff in every comparison that the picture itself does not explain. Take the
// shot with the window active, or say why it isn't.
if (!(await waitForForeground(hwnd, 2000))) {
  const fg = getForegroundWindow();
  const who = fg ? `"${getWindowText(fg)}" holds it` : "nothing holds it, the session is probably locked";
  console.error(`warning: window never reached the foreground (${who}); the caption renders inactive`);
}
captureWindowToPng(hwnd, dst);
setCursorPos(cursorWas.x, cursorWas.y);
await killAndWait(proc);
const logFile = Bun.file(logPath);
if (await logFile.exists()) {
  const text = (await logFile.text()).trim();
  if (text) {
    console.log(text);
  }
}
console.log(dst);

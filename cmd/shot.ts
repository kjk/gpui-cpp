// Screenshot one example window: bun cmd/shot.ts [-dbg] <example> [out.png]
//   -click=X,Y   click at client coords first
//   -hover=X,Y   leave the pointer there, for capturing a hover state
//   -drag=X1,Y1,X2,Y2  press, move, release: a text selection drag
//   -wheel=N     N notches of scroll at the window centre
//   -key=VK      send a key down
import { dirname, join, resolve } from "node:path";
import { mkdirSync } from "node:fs";
import {
  captureWindowToPng,
  clientToScreen,
  getClientRect,
  packCoords,
  clickClient,
  hoverClient,
  killAndWait,
  setForegroundWindow,
  sendMessage,
  setProcessDpiAware,
  sleep,
  waitForPidWindow,
} from "./winapi.ts";

const root = resolve(dirname(Bun.main), "..");
process.chdir(root);

const argv = Bun.argv.slice(2);
let debug = false;
const clicks: { x: number; y: number }[] = [];
let hover: { x: number; y: number } | null = null;
let drag: { x1: number; y1: number; x2: number; y2: number } | null = null;
const keys: number[] = [];
let wheel = 0;
const rest: string[] = [];
for (const a of argv) {
  if (a === "-dbg") {
    debug = true;
  } else if (a === "-rel") {
    debug = false;
  } else if (a.startsWith("-wheel=")) {
    wheel = Number(a.slice(7));
  } else if (a.startsWith("-key=")) {
    keys.push(Number(a.slice(5)));
  } else if (a.startsWith("-drag=")) {
    const [x1, y1, x2, y2] = a.slice(6).split(",").map(Number);
    drag = { x1: x1 ?? 0, y1: y1 ?? 0, x2: x2 ?? 0, y2: y2 ?? 0 };
  } else if (a.startsWith("-hover=")) {
    const [x, y] = a.slice(7).split(",").map(Number);
    hover = { x: x ?? 0, y: y ?? 0 };
  } else if (a.startsWith("-click=")) {
    const [x, y] = a.slice(7).split(",").map(Number);
    clicks.push({ x: x ?? 0, y: y ?? 0 });
  } else {
    rest.push(a);
  }
}
const name = rest[0];
if (!name) {
  console.error("Usage: bun cmd/shot.ts [-dbg] <example> [out.png] [args...]");
  process.exit(1);
}
const outDir = join(root, "out", "shots");
mkdirSync(outDir, { recursive: true });
const dst = rest[1] ?? join(outDir, `${name}.png`);
const exeDir = join(root, "out", debug ? "dbg" : "rel");
const exe = join(exeDir, `${name}.exe`);

setProcessDpiAware();
// AppLog writes out\gpui2.log relative to cwd.
mkdirSync(join(exeDir, "out"), { recursive: true });
const logPath = join(exeDir, "out", "gpui2.log");
try {
  await Bun.file(logPath).delete();
} catch {}
const proc = Bun.spawn([exe, ...rest.slice(2)], { cwd: exeDir, stdout: "pipe", stderr: "pipe" });
const hwnd = await waitForPidWindow(proc.pid ?? 0, 15000);
if (!hwnd) {
  await killAndWait(proc);
  console.error("window did not appear");
  process.exit(1);
}
setForegroundWindow(hwnd);
await sleep(500);
for (const c of clicks) {
  await clickClient(hwnd, c.x, c.y);
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
  sendMessage(hwnd, 0x0202 /* WM_LBUTTONUP */, 0, packCoords(drag.x2, drag.y2));
  await sleep(200);
}

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
for (const vk of keys) {
  sendMessage(hwnd, 0x0100 /* WM_KEYDOWN */, vk, 0);
  await sleep(120);
}
// Last, so the pointer is still on the element when the frame is captured.
if (hover) {
  await hoverClient(hwnd, hover.x, hover.y);
}
captureWindowToPng(hwnd, dst);
await killAndWait(proc);
const logFile = Bun.file(logPath);
if (await logFile.exists()) {
  const text = (await logFile.text()).trim();
  if (text) {
    console.log(text);
  }
}
console.log(dst);

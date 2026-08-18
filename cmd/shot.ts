// Screenshot one example window: bun cmd/shot.ts [-dbg] <example> [out.png]
//   -click=X,Y   click at client coords first
//   -hover=X,Y   leave the pointer there, for capturing a hover state
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

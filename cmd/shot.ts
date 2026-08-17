// Screenshot one example window: bun cmd/shot.ts [-dbg] <example> [out.png]
import { dirname, join, resolve } from "node:path";
import { mkdirSync } from "node:fs";
import {
  captureWindowToPng,
  clickClient,
  killAndWait,
  setForegroundWindow,
  setProcessDpiAware,
  sleep,
  waitForPidWindow,
} from "./winapi.ts";

const root = resolve(dirname(Bun.main), "..");
process.chdir(root);

const argv = Bun.argv.slice(2);
let debug = false;
const clicks: { x: number; y: number }[] = [];
const rest: string[] = [];
for (const a of argv) {
  if (a === "-dbg") {
    debug = true;
  } else if (a === "-rel") {
    debug = false;
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

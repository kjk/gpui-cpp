// Drive editor.exe under DeleakerConsole, scroll the document, then close so
// snapshots and an XML leak report are written. Windows only.
//
//   bun cmd/deleaker-scroll.ts
//   bun cmd/deleaker-scroll.ts -dbg
//   bun cmd/deleaker-scroll.ts -- __layout_reuse=off
import { existsSync, mkdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import {
  clickClient,
  clientToScreen,
  findVisibleClassWindows,
  getClientRect,
  killAndWait,
  packCoords,
  sendMessage,
  setForegroundWindow,
  setProcessDpiAware,
  sleep,
  waitForNewClassWindow,
} from "./winapi.ts";

const root = resolve(dirname(Bun.main), "..");
process.chdir(root);

const kClass = "GpuiSystemMonitor";
const kDeleaker = "C:\\Program Files (x86)\\Deleaker\\DeleakerConsole.exe";
const kOutDir = join(root, "out", "deleaker");

function die(msg: string): never {
  console.error(msg);
  process.exit(1);
}

const argv = Bun.argv.slice(2);
let debug = false;
const appArgs: string[] = [];
for (let i = 0; i < argv.length; i++) {
  const a = argv[i]!;
  if (a === "-dbg") {
    debug = true;
  } else if (a === "-rel") {
    debug = false;
  } else if (a === "--") {
    appArgs.push(...argv.slice(i + 1));
    break;
  } else {
    appArgs.push(a);
  }
}

const exe = join(root, "out", debug ? "dbg" : "rel", "editor.exe");
if (!existsSync(exe)) {
  die(`missing ${exe}; bun cmd/build.ts ${debug ? "-dbg" : "-rel"} editor`);
}
if (!existsSync(kDeleaker)) {
  die(`missing ${kDeleaker}`);
}

mkdirSync(kOutDir, { recursive: true });
const xmlPath = join(kOutDir, "editor-scroll.xml");
const snapPath = join(kOutDir, "editor-scroll.dsnapshot");

setProcessDpiAware();
const ignore = new Set(findVisibleClassWindows(kClass));

console.log(`DeleakerConsole --run ${exe} ${appArgs.join(" ")}`);
const proc = Bun.spawn(
  [
    kDeleaker,
    "--export-xml-report-on-exit",
    xmlPath,
    "--snapshot-database",
    snapPath,
    "--save-snapshot-period",
    "2",
    "--save-snapshot-on-exit",
    "--snapshot-name",
    "exit",
    "--process-working-directory",
    root,
    "--run",
    exe,
    ...appArgs,
  ],
  { cwd: root, stdout: "inherit", stderr: "inherit" },
);

const hwnd = await waitForNewClassWindow(kClass, ignore, 60000);
if (!hwnd) {
  await killAndWait(proc);
  die("editor window did not appear");
}
console.log(`window hwnd=${hwnd}`);
setForegroundWindow(hwnd);
await sleep(2500);

const r = getClientRect(hwnd);
const cx = Math.floor((r.right * 2) / 3);
const cy = Math.floor(r.bottom / 2);
await clickClient(hwnd, cx, cy, 200);
const pt = clientToScreen(hwnd, cx, cy);

async function wheel(notches: number): Promise<void> {
  const step = notches > 0 ? 120 : -120;
  for (let i = 0; i < Math.abs(notches); i++) {
    sendMessage(hwnd, 0x020a, (step << 16) >>> 0, packCoords(pt.x, pt.y));
    await sleep(30);
  }
}

console.log("scroll down");
await wheel(-50);
await sleep(2500);
console.log("scroll up");
await wheel(60);
await sleep(2500);
console.log("scroll down again");
await wheel(-40);
await sleep(2000);

console.log("closing");
sendMessage(hwnd, 0x0010, 0, 0);
const closed = await Promise.race([proc.exited.then(() => true), sleep(120000).then(() => false)]);
if (!closed) {
  console.log("Deleaker still running; killing editor");
  try {
    proc.kill();
  } catch {
    /* already gone */
  }
  await proc.exited;
}
console.log(`xml ${xmlPath}`);
console.log(`snapshots ${snapPath}`);

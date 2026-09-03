// Measure scene modes under actual window input and timer invalidations.
// Windows only, because only paint_win.cpp currently records a scene and the
// driver uses the same Win32 input seam as the screenshot tools.
//
//   bun cmd/bench-scene.ts
//   bun cmd/bench-scene.ts -n=20 -nobuild

import { existsSync, mkdirSync, rmSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import {
  captureWindowSurfaceToPng,
  clickClient,
  clientToScreen,
  getClientRect,
  getCursorPos,
  hoverClient,
  killAndWait,
  packCoords,
  sendMessage,
  setCursorPos,
  setForegroundWindow,
  setProcessDpiAware,
  sleep,
  waitForPidWindow,
  WM_CLOSE,
} from "./winapi.ts";

const root = resolve(dirname(Bun.main), "..");
process.chdir(root);

if (process.platform !== "win32") {
  console.error("cmd/bench-scene.ts currently requires Windows; only the Windows painter records a scene.");
  process.exit(1);
}

let cycles = 12;
let build = true;
for (const arg of Bun.argv.slice(2)) {
  if (arg === "-nobuild") {
    build = false;
  } else if (arg.startsWith("-n=")) {
    cycles = Math.max(4, Number.parseInt(arg.slice(3), 10) || cycles);
  } else {
    console.error(`unknown argument: ${arg}`);
    process.exit(1);
  }
}

type Scenario = {
  name: string;
  exe: string;
  args: string[];
  drive: (hwnd: number) => Promise<void>;
  deterministic: boolean;
  expectVisibleChange: boolean;
};

const scenarios: Scenario[] = [
  {
    name: "scroll",
    exe: "table_in_scrollable",
    args: [],
    deterministic: true,
    expectVisibleChange: true,
    drive: async (hwnd) => {
      const r = getClientRect(hwnd);
      const x = Math.floor(r.right / 2);
      const y = Math.floor(r.bottom / 2);
      const screen = clientToScreen(hwnd, x, y);
      for (let i = 0; i < cycles; i++) {
        const delta = i < Math.floor((cycles * 3) / 4) ? -120 : 120;
        sendMessage(hwnd, 0x020a /* WM_MOUSEWHEEL */, (delta << 16) >>> 0, packCoords(screen.x, screen.y));
        await sleep(45);
      }
    },
  },
  {
    name: "hover",
    exe: "showcase",
    args: ["button"],
    deterministic: true,
    expectVisibleChange: false,
    drive: async (hwnd) => {
      // The centered row starts left of the window midpoint. Use the primary
      // button so each move crosses an element with a visible hover style.
      const inside = { x: 305, y: 320 };
      const outside = { x: 12, y: 12 };
      for (let i = 0; i < cycles; i++) {
        const p = i + 1 === cycles || (i & 1) === 0 ? inside : outside;
        if (!(await hoverClient(hwnd, p.x, p.y, 45))) {
          throw new Error("the desktop refused to place the pointer; hover measurements would not be real");
        }
      }
    },
  },
  {
    name: "caret",
    exe: "input",
    args: [],
    deterministic: false,
    expectVisibleChange: false,
    drive: async (hwnd) => {
      const r = getClientRect(hwnd);
      // The 32-DIP input is centered; its bottom edge is exactly the client
      // midpoint in this example, and hit-testing excludes that edge.
      await clickClient(hwnd, Math.floor(r.right / 2), Math.floor(r.bottom / 2) - 8, 100);
      await sleep(cycles * 520);
    },
  },
  {
    name: "popup",
    exe: "showcase",
    args: ["popup"],
    deterministic: true,
    expectVisibleChange: true,
    drive: async (hwnd) => {
      const r = getClientRect(hwnd);
      const x = Math.floor(r.right / 2);
      const y = Math.floor(r.bottom / 2);
      // Finish open in every run so the final visible-surface capture checks
      // the deferred layer rather than the less interesting closed state.
      const count = cycles | 1;
      for (let i = 0; i < count; i++) {
        await clickClient(hwnd, x, y, 70);
      }
    },
  },
  {
    name: "chart-tick",
    exe: "system_monitor",
    args: [],
    deterministic: false,
    expectVisibleChange: false,
    drive: async () => {
      await sleep(cycles * 520);
    },
  },
];

const modes = ["off", "skip", "damage"] as const;
type Mode = (typeof modes)[number];
type Frame = {
  draw: number;
  build: number;
  layout: number;
  paint: number;
  presented: number;
  damage: number;
};
type Result = {
  scenario: string;
  mode: Mode;
  frames: Frame[];
  image: string;
  imageHash: string;
  visiblyChanged: boolean;
};

function runBuild(target: string): void {
  const result = Bun.spawnSync(["bun", "cmd/build.ts", "-rel", target], {
    cwd: root,
    stdout: "inherit",
    stderr: "inherit",
  });
  if ((result.exitCode ?? 1) !== 0) {
    process.exit(result.exitCode ?? 1);
  }
}

if (build) {
  for (const target of [...new Set(scenarios.map((scenario) => scenario.exe))]) {
    runBuild(target);
  }
}

const outDir = join(root, "out", "scene-bench");
mkdirSync(outDir, { recursive: true });
const logPath = join(root, "out", "gpui.log");

function parseFrames(text: string): Frame[] {
  const frames: Frame[] = [];
  const pattern =
    /interaction-bench frame=\d+ draw=([\d.]+) build=([\d.]+) layout=([\d.]+) paint=([\d.]+) presented=(\d+) invalidations=\d+ prims=-?\d+ changed=-?\d+ damage=(-?[\d.]+)/g;
  for (const match of text.matchAll(pattern)) {
    frames.push({
      draw: Number(match[1]),
      build: Number(match[2]),
      layout: Number(match[3]),
      paint: Number(match[4]),
      presented: Number(match[5]),
      damage: Number(match[6]),
    });
  }
  return frames;
}

async function hashFile(path: string): Promise<string> {
  const hasher = new Bun.CryptoHasher("sha256");
  hasher.update(await Bun.file(path).arrayBuffer());
  return hasher.digest("hex");
}

async function closeProcess(proc: Bun.Subprocess, hwnd: number): Promise<void> {
  sendMessage(hwnd, WM_CLOSE, 0, 0);
  const closed = await Promise.race([proc.exited.then(() => true), sleep(3000).then(() => false)]);
  if (!closed) {
    await killAndWait(proc);
  }
}

async function runOne(scenario: Scenario, mode: Mode): Promise<Result> {
  const exe = join(root, "out", "rel", `${scenario.exe}.exe`);
  if (!existsSync(exe)) {
    throw new Error(`missing ${exe}`);
  }
  rmSync(logPath, { force: true });
  const proc = Bun.spawn([exe, ...scenario.args, `__scene=${mode}`], {
    cwd: root,
    env: { ...process.env, GPUI_HOVER_HOLD: "1", GPUI_INTERACTION_BENCH: "1" },
    stdout: "ignore",
    stderr: "ignore",
  });
  const hwnd = await waitForPidWindow(proc.pid ?? 0, 15000);
  if (!hwnd) {
    await killAndWait(proc);
    throw new Error(`${scenario.name}/${mode}: window did not appear`);
  }
  setForegroundWindow(hwnd);
  await sleep(700);
  // Establish the same unhovered baseline even when a prior run left the
  // desktop pointer where this run's centered control will appear.
  await hoverClient(hwnd, 12, 12, 100);
  const startupFrames = parseFrames(await Bun.file(logPath).text()).length;
  const before = join(outDir, `${scenario.name}-${mode}-before.png`);
  if (!captureWindowSurfaceToPng(hwnd, before)) {
    await closeProcess(proc, hwnd);
    throw new Error(`${scenario.name}/${mode}: initial visible-surface capture failed`);
  }
  await scenario.drive(hwnd);
  await sleep(150);
  const image = join(outDir, `${scenario.name}-${mode}.png`);
  if (!captureWindowSurfaceToPng(hwnd, image)) {
    await closeProcess(proc, hwnd);
    throw new Error(`${scenario.name}/${mode}: visible-surface capture failed`);
  }
  await closeProcess(proc, hwnd);
  const log = await Bun.file(logPath).text();
  const frames = parseFrames(log).slice(startupFrames);
  if (frames.length < 2) {
    throw new Error(`${scenario.name}/${mode}: only ${frames.length} measured frames`);
  }
  const imageHash = await hashFile(image);
  const visiblyChanged = (await hashFile(before)) !== imageHash;
  if (scenario.expectVisibleChange && !visiblyChanged) {
    throw new Error(`${scenario.name}/${mode}: interaction did not visibly change the client surface`);
  }
  return { scenario: scenario.name, mode, frames, image, imageHash, visiblyChanged };
}

function percentile(frames: Frame[], field: keyof Pick<Frame, "draw" | "paint">, p: number): number {
  const values = frames.map((frame) => frame[field]).sort((a, b) => a - b);
  return values[Math.min(values.length - 1, Math.floor(values.length * p))]!;
}

function mean(frames: Frame[], field: keyof Pick<Frame, "build" | "layout" | "damage">): number {
  return frames.reduce((sum, frame) => sum + frame[field], 0) / frames.length;
}

setProcessDpiAware();
const cursor = getCursorPos();
const results: Result[] = [];
try {
  for (const scenario of scenarios) {
    for (const mode of modes) {
      console.log(`${scenario.name}/${mode}`);
      results.push(await runOne(scenario, mode));
    }
  }
} finally {
  setCursorPos(cursor.x, cursor.y);
}

const lines = [
  "# Scene interaction benchmark",
  "",
  `Generated ${new Date().toISOString()} with \`bun cmd/bench-scene.ts -n=${cycles}\`.`,
  "Times are milliseconds. Startup frames are excluded before each interaction begins.",
  "",
  "| scenario | scene | frames | draw median | draw p95 | paint median | paint p95 | build mean | layout mean | mean damage | presents |",
  "| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
];
for (const result of results) {
  const f = result.frames;
  lines.push(
    `| ${result.scenario} | ${result.mode} | ${f.length} | ${percentile(f, "draw", 0.5).toFixed(3)} | ${percentile(f, "draw", 0.95).toFixed(3)} | ${percentile(f, "paint", 0.5).toFixed(3)} | ${percentile(f, "paint", 0.95).toFixed(3)} | ${mean(f, "build").toFixed(3)} | ${mean(f, "layout").toFixed(3)} | ${mean(f, "damage").toFixed(3)} | ${f.reduce((n, frame) => n + frame.presented, 0)} |`,
  );
}

lines.push("", "## Visible-surface checks", "");
for (const result of results) {
  lines.push(
    `- ${result.scenario}/${result.mode}: interaction ${result.visiblyChanged ? "changed the surface" : "left the surface unchanged"}.`,
  );
}
lines.push("");
for (const scenario of scenarios.filter((item) => item.deterministic)) {
  const skip = results.find((result) => result.scenario === scenario.name && result.mode === "skip")!;
  const damage = results.find((result) => result.scenario === scenario.name && result.mode === "damage")!;
  lines.push(
    `- ${scenario.name}: ${skip.imageHash === damage.imageHash ? "identical" : "DIFFERENT"} skip/damage captures (${skip.imageHash.slice(0, 12)} / ${damage.imageHash.slice(0, 12)}).`,
  );
}
lines.push("");

const report = join(outDir, "results.md");
await Bun.write(report, `${lines.join("\n")}\n`);
console.log(`\n${lines.join("\n")}`);
console.log(`report ${report}`);

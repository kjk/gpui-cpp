// Automated visual and interaction parity checks against the pinned Rust
// story application.
//
//   bun cmd/parity.ts                     # build and run every case
//   bun cmd/parity.ts -nobuild input-edit select-open
//   bun cmd/parity.ts -report             # report budget excess without failing
//   bun cmd/parity.ts -list
//
// The Rust and C++ windows receive the same native input through compare-ui.
// Every checkpoint is compared in both directions: Rust versus C++ checks
// fidelity, and interactive checkpoints versus their initial frame prove the
// scripted input actually changed both applications. Results and captures go
// under out/parity/.

import { existsSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { comparePngFiles, type ImageDiff } from "./imgdiff.ts";
import { ensureRustTree, gpuiComponent, rustTreeDir } from "./run.ts";

const root = resolve(dirname(Bun.main), "..");
process.chdir(root);
const skipRows = 34;
const bigPixelTolerance = 90;

type Budget = {
  shot: string;
  maxAnyPct: number;
  maxBigPct: number;
  changesFrom?: string;
  minChangePct?: number;
};

type ParityCase = {
  name: string;
  slug: string;
  steps: string[];
  budgets: Budget[];
};

// These cases cover static composition plus state, text input, overlays,
// scrolling, pointer capture and retained tree state. Budgets are deliberately
// percentages rather than pixel counts so the suite survives a different work
// area size. They are calibrated against the current pinned Rust build with
// room for font antialiasing variance, not intended as claims of pixel identity.
const cases: ParityCase[] = [
  {
    name: "introduction-static",
    slug: "introduction",
    steps: ["shot:initial"],
    budgets: [{ shot: "initial", maxAnyPct: 13, maxBigPct: 10 }],
  },
  {
    name: "button-static",
    slug: "button",
    steps: ["shot:initial"],
    budgets: [{ shot: "initial", maxAnyPct: 10, maxBigPct: 5 }],
  },
  {
    name: "checkbox-toggle",
    slug: "checkbox",
    steps: ["shot:initial", "click:420,258", "shot:checked"],
    budgets: [
      { shot: "initial", maxAnyPct: 8, maxBigPct: 4.5 },
      { shot: "checked", maxAnyPct: 8, maxBigPct: 4.5, changesFrom: "initial", minChangePct: 0.01 },
    ],
  },
  {
    name: "input-edit",
    slug: "input",
    steps: ["shot:initial", "click:500,314", "type:Parity42", "shot:typed"],
    budgets: [
      { shot: "initial", maxAnyPct: 11, maxBigPct: 4.25 },
      { shot: "typed", maxAnyPct: 11, maxBigPct: 4.25, changesFrom: "initial", minChangePct: 0.2 },
    ],
  },
  {
    name: "select-open",
    slug: "select",
    steps: ["shot:initial", "click:580,269", "shot:open"],
    budgets: [
      { shot: "initial", maxAnyPct: 9.5, maxBigPct: 4.75 },
      { shot: "open", maxAnyPct: 9.5, maxBigPct: 4.75, changesFrom: "initial", minChangePct: 1 },
    ],
  },
  {
    name: "dialog-open",
    slug: "dialog",
    steps: ["shot:initial", "click:580,278", "shot:open", "click:566,377", "shot:closed"],
    budgets: [
      { shot: "initial", maxAnyPct: 8, maxBigPct: 4 },
      { shot: "open", maxAnyPct: 18, maxBigPct: 4, changesFrom: "initial", minChangePct: 50 },
      { shot: "closed", maxAnyPct: 8, maxBigPct: 4, changesFrom: "open", minChangePct: 50 },
    ],
  },
  {
    name: "table-scroll",
    slug: "table",
    steps: ["shot:initial", "wheel:-4@700,500", "shot:scrolled"],
    budgets: [
      { shot: "initial", maxAnyPct: 14, maxBigPct: 6.5 },
      { shot: "scrolled", maxAnyPct: 14, maxBigPct: 6.5, changesFrom: "initial", minChangePct: 10 },
    ],
  },
  {
    name: "slider-drag",
    slug: "slider",
    steps: ["shot:initial", "drag:630,319,500,319", "shot:moved"],
    budgets: [
      { shot: "initial", maxAnyPct: 10.5, maxBigPct: 5 },
      { shot: "moved", maxAnyPct: 10.5, maxBigPct: 5, changesFrom: "initial", minChangePct: 0.04 },
    ],
  },
  {
    name: "tabs-static",
    slug: "tabs",
    steps: ["shot:initial"],
    budgets: [{ shot: "initial", maxAnyPct: 12.5, maxBigPct: 5 }],
  },
  {
    name: "tree-expand",
    slug: "tree",
    steps: ["shot:initial", "click:365,273", "shot:expanded"],
    budgets: [
      { shot: "initial", maxAnyPct: 7.5, maxBigPct: 4 },
      { shot: "expanded", maxAnyPct: 10, maxBigPct: 4.5, changesFrom: "initial", minChangePct: 1 },
    ],
  },
];

type Args = {
  debug: boolean;
  nobuild: boolean;
  reportOnly: boolean;
  list: boolean;
  selected: string[];
};

function usage(): never {
  console.error("Usage: bun cmd/parity.ts [-rel|-dbg] [-nobuild] [-report] [-list] [case ...]");
  process.exit(2);
}

function parseArgs(argv: string[]): Args {
  let debug = false;
  let nobuild = false;
  let reportOnly = false;
  let list = false;
  const selected: string[] = [];
  for (const arg of argv) {
    if (arg === "-rel") debug = false;
    else if (arg === "-dbg") debug = true;
    else if (arg === "-nobuild") nobuild = true;
    else if (arg === "-report") reportOnly = true;
    else if (arg === "-list") list = true;
    else if (arg.startsWith("-")) usage();
    else selected.push(arg);
  }
  return { debug, nobuild, reportOnly, list, selected };
}

function run(command: string[], cwd: string): number {
  console.log(`> ${command.join(" ")}`);
  const result = Bun.spawnSync(command, { cwd, stdout: "inherit", stderr: "inherit" });
  return result.exitCode ?? 1;
}

function cargoExe(): string {
  if (process.platform !== "win32") return "cargo";
  const local = join(process.env["USERPROFILE"] ?? "", ".cargo", "bin", "cargo.exe");
  return existsSync(local) ? local : "cargo";
}

function shotPath(dir: string, c: ParityCase, budget: Budget, side: "rust" | "cpp"): string {
  const index = c.budgets.indexOf(budget) + 1;
  return join(dir, `${c.slug}-${String(index).padStart(2, "0")}-${budget.shot}-${side}.png`);
}

function pct(n: number, total: number): number {
  return total ? (n * 100) / total : 0;
}

type ShotResult = {
  shot: string;
  status: "pass" | "fail";
  withinBudget?: boolean;
  interactionChanged?: boolean;
  anyPct?: number;
  bigPct?: number;
  maxAnyPct: number;
  maxBigPct: number;
  rustChangePct?: number;
  cppChangePct?: number;
  minChangePct?: number;
  box?: number[] | null;
  error?: string;
};

type CaseResult = {
  name: string;
  slug: string;
  status: "pass" | "fail";
  shots: ShotResult[];
  error?: string;
};

function changePct(from: string, to: string): number {
  const diff = comparePngFiles(from, to, skipRows, bigPixelTolerance);
  return pct(diff.any, diff.total);
}

function compareShot(dir: string, c: ParityCase, budget: Budget): ShotResult {
  const rust = shotPath(dir, c, budget, "rust");
  const cpp = shotPath(dir, c, budget, "cpp");
  if (!existsSync(rust) || !existsSync(cpp)) {
    return {
      shot: budget.shot,
      status: "fail",
      maxAnyPct: budget.maxAnyPct,
      maxBigPct: budget.maxBigPct,
      error: `missing capture: ${!existsSync(rust) ? rust : cpp}`,
    };
  }
  try {
    const diff: ImageDiff = comparePngFiles(rust, cpp, skipRows, bigPixelTolerance);
    const anyPct = pct(diff.any, diff.total);
    const bigPct = pct(diff.big, diff.total);
    let rustChangePct: number | undefined;
    let cppChangePct: number | undefined;
    let changed: boolean | undefined;
    if (budget.changesFrom) {
      const from = c.budgets.find((candidate) => candidate.shot === budget.changesFrom);
      if (!from) throw new Error(`unknown changesFrom checkpoint ${budget.changesFrom}`);
      rustChangePct = changePct(shotPath(dir, c, from, "rust"), rust);
      cppChangePct = changePct(shotPath(dir, c, from, "cpp"), cpp);
      const minimum = budget.minChangePct ?? 0;
      changed = rustChangePct >= minimum && cppChangePct >= minimum;
    }
    return {
      shot: budget.shot,
      status: anyPct <= budget.maxAnyPct && bigPct <= budget.maxBigPct && changed !== false ? "pass" : "fail",
      withinBudget: anyPct <= budget.maxAnyPct && bigPct <= budget.maxBigPct,
      interactionChanged: changed,
      anyPct,
      bigPct,
      maxAnyPct: budget.maxAnyPct,
      maxBigPct: budget.maxBigPct,
      rustChangePct,
      cppChangePct,
      minChangePct: budget.minChangePct,
      box: diff.box,
    };
  } catch (error) {
    return {
      shot: budget.shot,
      status: "fail",
      maxAnyPct: budget.maxAnyPct,
      maxBigPct: budget.maxBigPct,
      error: error instanceof Error ? error.message : String(error),
    };
  }
}

function fixed(n: number | undefined): string {
  return n === undefined ? "-" : n.toFixed(3);
}

function fixedPct(n: number | undefined): string {
  return n === undefined ? "-" : `${n.toFixed(3)}%`;
}

function markdown(results: CaseResult[], args: Args): string {
  const lines = [
    "# Rust/C++ parity suite",
    "",
    `Generated ${new Date().toISOString()} with \`bun cmd/parity.ts${args.nobuild ? " -nobuild" : ""}\`.`,
    `Rust specification: gpui-kit ${gpuiComponent.sha.slice(0, 12)} (${gpuiComponent.date}).`,
    `The top ${skipRows} window rows are ignored; a channel-sum above ${bigPixelTolerance} counts as a big pixel difference.`,
    "",
    "| case | shot | result | any / budget | big / budget | Rust changed | C++ changed |",
    "| --- | --- | --- | ---: | ---: | ---: | ---: |",
  ];
  for (const result of results) {
    if (result.error) {
      lines.push(`| ${result.name} | harness | fail | - | - | - | - |`);
    }
    for (const shot of result.shots) {
      lines.push(
        `| ${result.name} | ${shot.shot} | ${shot.status} | ${fixedPct(shot.anyPct)} / ${shot.maxAnyPct.toFixed(3)}% | ${fixedPct(shot.bigPct)} / ${shot.maxBigPct.toFixed(3)}% | ${fixedPct(shot.rustChangePct)} | ${fixedPct(shot.cppChangePct)} |`,
      );
    }
  }
  const failed = results.filter((result) => result.status === "fail").length;
  lines.push("", `${results.length - failed} passed, ${failed} failed.`, "");
  return lines.join("\n");
}

const args = parseArgs(Bun.argv.slice(2));
if (args.list) {
  for (const c of cases) console.log(`${c.name.padEnd(24)} ${c.slug}`);
  process.exit(0);
}
if (process.platform !== "win32") {
  console.error("The parity suite currently needs the Windows native-input and capture driver.");
  process.exit(2);
}

const selected = args.selected.length ? cases.filter((c) => args.selected.includes(c.name)) : cases;
for (const name of args.selected) {
  if (!cases.some((c) => c.name === name)) {
    console.error(`Unknown parity case: ${name}`);
    usage();
  }
}

let rustRoot: string;
try {
  rustRoot = ensureRustTree(root);
} catch (error) {
  console.error(error instanceof Error ? error.message : String(error));
  process.exit(1);
}

const profile = args.debug ? "-dbg" : "-rel";
if (!args.nobuild) {
  if (run(["bun", "cmd/build.ts", profile, "story"], root) !== 0) process.exit(1);
  const cargo = cargoExe();
  const cargoArgs = ["build", ...(args.debug ? [] : ["--release"]), "-p", "gpui-component-story"];
  if (run([cargo, ...cargoArgs], rustRoot) !== 0) process.exit(1);
}

const rustExe = join(rustTreeDir(root), "target", args.debug ? "debug" : "release", "gpui-component-story.exe");
const cppExe = join(root, "out", args.debug ? "dbg" : "rel", "story.exe");
if (!existsSync(rustExe) || !existsSync(cppExe)) {
  console.error(`Missing ${!existsSync(rustExe) ? rustExe : cppExe}; rerun without -nobuild.`);
  process.exit(1);
}

const outDir = join(root, "out", "parity");
mkdirSync(outDir, { recursive: true });
const results: CaseResult[] = [];

for (const c of selected) {
  console.log(`\n=== parity: ${c.name} ===`);
  const caseDir = join(outDir, c.name);
  rmSync(caseDir, { recursive: true, force: true });
  mkdirSync(caseDir, { recursive: true });
  const command = ["bun", "cmd/compare-ui.ts", profile, "-nobuild", `-out=${caseDir}`, c.slug, ...c.steps];
  const rc = run(command, root);
  if (rc !== 0) {
    results.push({ name: c.name, slug: c.slug, status: "fail", shots: [], error: `compare-ui exited ${rc}` });
    continue;
  }
  const shots = c.budgets.map((budget) => compareShot(caseDir, c, budget));
  const result: CaseResult = {
    name: c.name,
    slug: c.slug,
    status: shots.every((shot) => shot.status === "pass") ? "pass" : "fail",
    shots,
  };
  results.push(result);
  for (const shot of shots) {
    const changes =
      shot.minChangePct === undefined
        ? ""
        : ` changed rust=${fixed(shot.rustChangePct)}% cpp=${fixed(shot.cppChangePct)}%`;
    console.log(
      `  ${shot.status.toUpperCase()} ${shot.shot}: any=${fixed(shot.anyPct)}%/${shot.maxAnyPct.toFixed(3)}% ` +
        `big=${fixed(shot.bigPct)}%/${shot.maxBigPct.toFixed(3)}%${changes}${shot.error ? ` ${shot.error}` : ""}`,
    );
  }
}

const report = markdown(results, args);
writeFileSync(
  join(outDir, "results.json"),
  `${JSON.stringify(
    {
      generatedAt: new Date().toISOString(),
      rust: { sha: gpuiComponent.sha, date: gpuiComponent.date },
      profile: args.debug ? "debug" : "release",
      skipRows,
      bigPixelTolerance,
      results,
    },
    null,
    2,
  )}\n`,
);
writeFileSync(join(outDir, "results.md"), report);
console.log(`\n${report}`);
console.log(`Artifacts: ${outDir}`);

const harnessFailed = results.some(
  (result) => result.error || result.shots.some((shot) => shot.error || shot.interactionChanged === false),
);
const budgetFailed = results.some((result) => result.shots.some((shot) => shot.withinBudget === false));
process.exit(harnessFailed || (budgetFailed && !args.reportOnly) ? 1 : 0);

// Measure how Vec and ArenaVec grow, and replay the run against other growth
// policies: bun cmd/vec-log.ts [-dbg] [-out=FILE] [-top=N] <target> [args...]
//
//   bun cmd/vec-log.ts tests
//   bun cmd/vec-log.ts bench markdown
//   bun cmd/vec-log.ts shot showcase           # drive an example through shot.ts
//   bun cmd/vec-log.ts -read=out/dbg/vec-log-tests.txt   # a log already taken
//
// The instrumentation is in src/base.h and src/base.cpp behind `#if
// defined(DEBUG)`, and writes nothing unless GPUI_VEC_LOG names a file — which
// is what this script sets. The line formats are documented there. Everything
// below is analysis: a policy is replayed rather than rebuilt, because the log
// records what each caller *asked* for (`needed`, `want`) and not only what
// the current policy handed out.
//
// The replay is exact for a vec that only ever appends one element at a time,
// which is nearly all of them, and it is optimistic for a candidate policy
// that is smaller than the current one at some capacity: a reserve that fit
// under the current policy was never logged, so a tighter policy's extra
// realloc for it cannot be seen. Read the 1.5x rows with that in mind; the
// rows that only ever raise a capacity are exact.

import { join, resolve } from "node:path";
import { mkdirSync } from "node:fs";
import { build, defaultBuildFlags, outDir as buildOutDir, outFileName, platformFor, root } from "./build.ts";

const argv = Bun.argv.slice(2);
let debug = true;
let readOnly = "";
let outPath = "";
let top = 20;
const rest: string[] = [];
for (const a of argv) {
  if (a === "-rel" || a === "--rel") {
    debug = false;
  } else if (a === "-dbg" || a === "--dbg") {
    debug = true;
  } else if (a.startsWith("-read=")) {
    readOnly = a.slice(6);
  } else if (a.startsWith("-out=")) {
    outPath = a.slice(5);
  } else if (a.startsWith("-top=")) {
    top = parseInt(a.slice(5), 10);
  } else {
    rest.push(a);
  }
}

function die(msg: string): never {
  console.error(msg);
  process.exit(2);
}

// build.ts owns the out/ layout and the toolchain; ask it rather than
// spelling the rule out a second time.
const flags = defaultBuildFlags();
flags.debug = debug;
const plat = platformFor(flags, die);
const outDir = buildOutDir(plat, flags);

async function run(): Promise<string> {
  const target = rest[0];
  if (!target) {
    die("usage: bun cmd/vec-log.ts [-dbg] <tests|bench|shot|EXAMPLE> [args...]");
  }
  mkdirSync(outDir, { recursive: true });
  const log = resolve(root, outPath || join(outDir, `vec-log-${target}.txt`));
  const env = { ...process.env, GPUI_VEC_LOG: log };
  const flag = debug ? "-dbg" : "-rel";

  let cmd: string[];
  if (target === "tests" || target === "bench") {
    await build({ names: [target], plat, flags, fail: die, quiet: true });
    cmd = [join(root, outDir, outFileName(plat, target)), ...rest.slice(1)];
  } else if (target === "shot") {
    // shot.ts builds, opens the window, takes the picture and kills it, which
    // is the only way to get an example's frame-building work into a log
    // without a person clicking the close box.
    cmd = ["bun", join(root, "cmd", "shot.ts"), flag, ...rest.slice(1)];
  } else {
    cmd = ["bun", join(root, "cmd", "shot.ts"), flag, target];
  }
  const r = Bun.spawnSync(cmd, { cwd: join(root, outDir), env, stdout: "inherit", stderr: "inherit" });
  if (r.exitCode !== 0) process.exit(r.exitCode ?? 1);
  return log;
}

// ─── the log ─────────────────────────────────────────────────────────────

type Ev = { needed: number; len: number };
type VecRec = {
  id: number;
  kind: string;
  elSize: number;
  site: string;
  events: Ev[];
  finalLen: number;
  peakLen: number;
};

async function parse(path: string): Promise<VecRec[]> {
  const t = await Bun.file(path).text();
  const byId = new Map<number, VecRec>();
  for (const line of t.split("\n")) {
    if (line.length < 3) continue;
    const f = line.split(" ");
    const id = +f[1];
    switch (f[0]) {
      case "B": {
        // B <id> <V|A> <elSize> <func> <file>:<line>. A struct's members all
        // land on its closing brace, so the element size is part of the key
        // that tells them apart and the function name says who owns them.
        byId.set(id, {
          id,
          kind: f[2],
          elSize: +f[3],
          site: `${f.slice(5).join(" ")} ${f[4]} [${f[3]}b]`,
          events: [],
          finalLen: 0,
          peakLen: 0,
        });
        break;
      }
      case "G": {
        // G <id> <len> <oldCap> <needed> <newCap>
        const r = byId.get(id);
        if (!r) break;
        r.events.push({ len: +f[2], needed: +f[4] });
        r.peakLen = Math.max(r.peakLen, +f[4]);
        break;
      }
      case "S": {
        // S <id> <len> <want> <lastSegCap> <newSegCap> <totalCap> <reused>
        const r = byId.get(id);
        if (!r) break;
        const len = +f[2];
        const want = +f[3];
        r.events.push({ len, needed: len + want });
        r.peakLen = Math.max(r.peakLen, len + want);
        break;
      }
      case "D":
      case "E": {
        const r = byId.get(id);
        if (!r) break;
        r.finalLen = Math.max(r.finalLen, +f[2]);
        r.peakLen = Math.max(r.peakLen, +f[2]);
        break;
      }
    }
  }
  return [...byId.values()];
}

// ─── policy replay ───────────────────────────────────────────────────────

// `copies` says whether a grow moves the elements: a Vec reallocates and
// memcpys, an ArenaVec links a new segment and copies nothing. `headerBytes`
// is what each grow costs in bookkeeping — ArenaVecSegment's header, which is
// why a policy that takes many small segments is not free even though it
// never copies. `padEls` is Vec's trailing zero element.
//
// "alloc bytes" is the sum of every vec's final capacity over the whole run —
// cumulative allocation, not peak resident memory, since most of these vecs
// are short-lived.
type Policy = {
  name: string;
  next: (cap: number, needed: number, elSize: number) => number;
  copies?: boolean;
  headerBytes?: number;
  padEls?: number;
};

function cost(recs: VecRec[], p: Policy) {
  const copies = p.copies !== false;
  const header = p.headerBytes ?? 0;
  const pad = p.padEls ?? 0;
  let reallocs = 0;
  let elsCopied = 0;
  let bytesCopied = 0;
  let bytesLive = 0;
  let wasteBytes = 0;
  let multi = 0;
  for (const r of recs) {
    let cap = 0;
    let grows = 0;
    const grow = (needed: number, lenNow: number) => {
      while (cap < needed) {
        reallocs++;
        grows++;
        if (copies) {
          const n = lenNow > cap ? cap : lenNow;
          elsCopied += n;
          bytesCopied += n * r.elSize;
        }
        cap = p.next(cap, Math.max(needed, cap + 1), r.elSize);
      }
    };
    // The appends that were silent under the recorded policy still happened;
    // replaying them is what makes a candidate's realloc count comparable.
    let seen = 0;
    for (const e of r.events) {
      for (let n = seen + 1; n <= e.len; n++) grow(n, n - 1);
      grow(e.needed, e.len);
      seen = Math.max(seen, e.needed);
    }
    for (let n = seen + 1; n <= r.finalLen; n++) grow(n, n - 1);
    if (grows > 1) multi++;
    bytesLive += cap > 0 ? (cap + pad) * r.elSize + grows * header : 0;
    wasteBytes += (cap - r.peakLen) * r.elSize + grows * header;
  }
  return { reallocs, elsCopied, bytesCopied, bytesLive, wasteBytes, multi };
}

const vecPolicies: Policy[] = [
  {
    // What src/base.h does today: VecNextCap. Every row below it is a
    // candidate, and "was" is what the tree did before it.
    name: "CURRENT  8/4/1  2x",
    next: (c, n, e) => Math.max(c === 0 ? (e === 1 ? 8 : e <= 1024 ? 4 : 1) : c * 2, n),
    padEls: 1,
  },
  { name: "was:     max(2c, n)", next: (c, n) => Math.max(c * 2, n), padEls: 1 },
  { name: "min2     max(2c, n)", next: (c, n) => Math.max(c === 0 ? 2 : c * 2, n), padEls: 1 },
  { name: "min4     max(2c, n)", next: (c, n) => Math.max(c === 0 ? 4 : c * 2, n), padEls: 1 },
  { name: "min8     max(2c, n)", next: (c, n) => Math.max(c === 0 ? 8 : c * 2, n), padEls: 1 },
  { name: "min16    max(2c, n)", next: (c, n) => Math.max(c === 0 ? 16 : c * 2, n), padEls: 1 },
  { name: "min4     max(1.5c,n)", next: (c, n) => Math.max(c === 0 ? 4 : Math.ceil(c * 1.5), n), padEls: 1 },
  { name: "min8     max(1.5c,n)", next: (c, n) => Math.max(c === 0 ? 8 : Math.ceil(c * 1.5), n), padEls: 1 },
  {
    name: "min8 2c<=1k then1.5",
    next: (c, n) => Math.max(c === 0 ? 8 : c < 1024 ? c * 2 : Math.ceil(c * 1.5), n),
    padEls: 1,
  },
  // Sizing the first block purely in bytes, for comparison: it under-serves
  // a 192-byte element, which is why the shipped rule is a count with a
  // byte-aware floor rather than this.
  {
    name: "first ~64B      2x",
    next: (c, n, e) => Math.max(c === 0 ? Math.min(8, Math.max(1, Math.floor(64 / e))) : c * 2, n),
    padEls: 1,
  },
  {
    name: "first ~128B     2x",
    next: (c, n, e) => Math.max(c === 0 ? Math.min(16, Math.max(1, Math.floor(128 / e))) : c * 2, n),
    padEls: 1,
  },
];

// sizeof(ArenaVecSegment<T>) on a 64-bit host: an ArenaPtr to the next segment
// and three ints, which is 16, then rounded up to alignof(T) — 16 for
// everything this tree puts in one, since nothing it holds is aligned wider
// than 8. It was 24 while `next` was a pointer.
const kSegHeader = 16;

// The shipped rule: `count` elements, or as many as fit in `bytes`, whichever
// is fewer. Shared with the per-site recommendation below.
function segStep(cap: number, elSize: number) {
  const step = (count: number, bytes: number) => Math.max(1, Math.min(count, Math.floor(bytes / elSize)));
  const a = step(4, 64);
  const b = step(16, 256);
  const c = step(64, 1024);
  return cap < a ? a : cap < b ? b : cap < c ? c : cap;
}

function arenaPolicies(): Policy[] {
  const out: Policy[] = [
    {
      name: "CURRENT 4/16/64 capB",
      copies: false,
      headerBytes: kSegHeader,
      next: (cap, needed, elSize) => {
        let seg = segStep(cap, elSize);
        const want = needed - cap;
        if (seg < want) seg = want;
        return cap + seg;
      },
    },
  ];
  // Plain element counts, the shape this used to have, plus a few neighbours.
  const triples = [
    [4, 16, 64],
    [1, 8, 64],
    [2, 8, 32],
    [2, 16, 128],
    [3, 12, 48],
    [4, 32, 256],
    [8, 32, 128],
    [8, 64, 256],
    [16, 64, 256],
  ];
  for (const [a, b, c0] of triples) {
    out.push({
      name: `seg ${a}/${b}/${c0} counts`,
      copies: false,
      headerBytes: kSegHeader,
      next: (cap: number, needed: number) => {
        // ArenaVec never copies: `cap` here stands for the total capacity and
        // the next segment is sized off the last one. Replaying it as a
        // running total is close but not exact once a vec is past its third
        // segment, which about one vec in a hundred ever is.
        let seg = cap < a ? a : cap < b ? b : cap < c0 ? c0 : cap * 2;
        const want = needed - cap;
        if (seg < want) seg = want;
        return cap + seg;
      },
    });
  }
  // The whole progression sized in bytes rather than only capped by them.
  out.push({
    name: "seg ~64B/256B/1KB",
    copies: false,
    headerBytes: kSegHeader,
    next: (cap: number, needed: number, elSize: number) => {
      const els = (bytes: number) => Math.max(1, Math.floor(bytes / elSize));
      const a = els(64);
      const b = els(256);
      const c0 = els(1024);
      let seg = cap < a ? a : cap < b ? b : cap < c0 ? c0 : cap;
      const want = needed - cap;
      if (seg < want) seg = want;
      return cap + seg;
    },
  });
  return out;
}

// ─── the report ──────────────────────────────────────────────────────────

function fmtN(n: number) {
  return n.toLocaleString("en-US");
}

function pct(a: number, b: number) {
  return b === 0 ? "-" : ((a / b) * 100).toFixed(1) + "%";
}

function histogram(vals: number[]) {
  const buckets = [0, 1, 2, 4, 8, 16, 32, 64, 128, 512, 4096, Infinity];
  const counts = new Array(buckets.length).fill(0);
  for (const v of vals) {
    for (let i = buckets.length - 1; i >= 0; i--) {
      if (v >= buckets[i]) {
        counts[i]++;
        break;
      }
    }
  }
  const out: string[] = [];
  for (let i = 0; i < buckets.length; i++) {
    if (!counts[i]) continue;
    const lo = buckets[i];
    const hi = buckets[i + 1] === undefined ? Infinity : buckets[i + 1];
    const label = hi === Infinity ? `>=${lo}` : hi === lo + 1 ? `${lo}` : `${lo}..${hi - 1}`;
    out.push(`${label.padStart(9)} ${String(counts[i]).padStart(7)}  ${pct(counts[i], vals.length).padStart(6)}`);
  }
  return out.join("\n");
}

function quantile(sorted: number[], q: number) {
  if (!sorted.length) return 0;
  return sorted[Math.min(sorted.length - 1, Math.floor(q * sorted.length))];
}

function report(recs: VecRec[], kind: string, policies: Policy[], label: string) {
  const mine = recs.filter((r) => r.kind === kind);
  if (!mine.length) return;
  const seg = kind === "A";
  const grew = mine.filter((r) => r.events.length > 0);
  console.log(`\n${"=".repeat(72)}\n${label}\n${"=".repeat(72)}`);
  console.log(`vecs created        ${fmtN(mine.length)}`);
  console.log(`ever grew           ${fmtN(grew.length)}  (${pct(grew.length, mine.length)})`);
  console.log(`growth events       ${fmtN(mine.reduce((a, r) => a + r.events.length, 0))}`);
  console.log(`\npeak length, all vecs:`);
  console.log(histogram(mine.map((r) => r.peakLen)));

  console.log(`\npolicy replay (whole run):`);
  console.log(
    `${"policy".padEnd(21)} ${(seg ? "segments" : "grows").padStart(9)} ` +
      `${(seg ? ">1 seg" : "els copied").padStart(12)} ${(seg ? "" : "bytes copied").padStart(13)} ` +
      `${(seg ? "arena bytes" : "alloc bytes").padStart(12)} ${"waste".padStart(12)}`,
  );
  for (const p of policies) {
    const c = cost(mine, p);
    const b = seg ? fmtN(c.multi) : fmtN(c.elsCopied);
    const cc = seg ? "" : fmtN(c.bytesCopied);
    console.log(
      `${p.name.padEnd(21)} ${fmtN(c.reallocs).padStart(9)} ${b.padStart(12)} ${cc.padStart(13)} ` +
        `${fmtN(c.bytesLive).padStart(12)} ${fmtN(c.wasteBytes).padStart(12)}`,
    );
  }

  // The per-site recommendation is the smallest capacity that covers nine
  // vecs in ten at the site — the 90th percentile of peak length, rounded up
  // to a power of two — and the two columns after it are what the site costs
  // once it starts there instead of at the generic first capacity.
  const bySite = new Map<string, VecRec[]>();
  for (const r of mine) {
    const a = bySite.get(r.site);
    if (a) a.push(r);
    else bySite.set(r.site, [r]);
  }
  const rows = [...bySite.entries()].map(([site, rs]) => {
    const base = cost(rs, policies[0]);
    const peaks = rs.map((r) => r.peakLen).sort((a, b) => a - b);
    const p90 = quantile(peaks, 0.9);
    let rec = 1;
    while (rec < p90) rec *= 2;
    const withRec = cost(rs, {
      ...policies[0],
      name: "rec",
      next: (c, n, e) => Math.max(c === 0 ? rec : seg ? c + segStep(c, e) : c * 2, n),
    });
    return {
      site,
      n: rs.length,
      elSize: rs[0].elSize,
      grows: base.reallocs,
      bytesCopied: base.bytesCopied,
      p50: quantile(peaks, 0.5),
      p90,
      max: peaks[peaks.length - 1],
      rec,
      recGrows: withRec.reallocs,
      recLive: withRec.bytesLive,
    };
  });
  rows.sort((a, b) => b.grows - a.grows || b.bytesCopied - a.bytesCopied);
  console.log(`\ntop ${top} sites by growth events:`);
  console.log(
    `${"grows".padStart(8)} ${"vecs".padStart(7)} ${"elSz".padStart(5)} ${"copiedB".padStart(10)} ` +
      `${"p50".padStart(6)} ${"p90".padStart(6)} ${"max".padStart(7)} ${"rec".padStart(5)} ` +
      `${"->grows".padStart(8)} ${"->allocB".padStart(11)}  site`,
  );
  for (const r of rows.slice(0, top)) {
    console.log(
      `${fmtN(r.grows).padStart(8)} ${fmtN(r.n).padStart(7)} ${String(r.elSize).padStart(5)} ` +
        `${fmtN(r.bytesCopied).padStart(10)} ${String(r.p50).padStart(6)} ${String(r.p90).padStart(6)} ` +
        `${String(r.max).padStart(7)} ${String(r.rec).padStart(5)} ${fmtN(r.recGrows).padStart(8)} ` +
        `${fmtN(r.recLive).padStart(11)}  ${r.site}`,
    );
  }
}

const logPath = readOnly ? resolve(root, readOnly) : await run();
const recs = await parse(logPath);
console.log(`log: ${logPath}  (${fmtN(recs.length)} vecs)`);
report(recs, "V", vecPolicies, "Vec  — heap, reallocating");
report(recs, "A", arenaPolicies(), "ArenaVec — arena, segmented");

// Exe size reporting, shared by cmd/build.ts (what it just built) and
// cmd/run.ts (the rust binary next to ours, under -compare).

import { existsSync, statSync } from "node:fs";

export function formatExactBytes(n: number): string {
  return `${n.toLocaleString("en-US")} b`;
}

export function formatHumanBytes(n: number): string {
  if (n >= 1_000_000_000) {
    return `${(n / 1_000_000_000).toFixed(1)} GB`;
  }
  if (n >= 1_000_000) {
    return `${(n / 1_000_000).toFixed(1)} MB`;
  }
  if (n >= 1_000) {
    return `${(n / 1_000).toFixed(1)} kB`;
  }
  return `${n} b`;
}

export type SizeEntry = {
  /** What to show in the first column. */
  label: string;
  /** Absolute path to stat. */
  path: string;
};

/** One row per entry: label, exact bytes, human readable. */
export function printSizeTable(entries: SizeEntry[]): void {
  if (entries.length === 0) {
    return;
  }
  const rows = entries.map((e) => {
    if (!existsSync(e.path)) {
      return { label: e.label, bytes: "(not generated)", human: "" };
    }
    const size = statSync(e.path).size;
    return { label: e.label, bytes: formatExactBytes(size), human: formatHumanBytes(size) };
  });
  const labelW = Math.max(...rows.map((r) => r.label.length));
  const bytesW = Math.max(...rows.map((r) => r.bytes.length));
  const humanW = Math.max(...rows.map((r) => r.human.length));
  for (const r of rows) {
    console.log(`${r.label.padEnd(labelW)}  ${r.bytes.padStart(bytesW)}  ${r.human.padStart(humanW)}`);
  }
}

// Exact upstream checkins this C++ port is matching.
// Source of truth — AGENTS.md / port-*.md point here. Bump the SHAs (then
// re-run bun cmd/build.ts or bun cmd/versions.ts) when ingesting a newer
// gpui-component. Do not read HEAD of a random clone.

import { existsSync, mkdirSync, rmSync } from "node:fs";
import { dirname, join, resolve } from "node:path";

/** Spec we port: crates/base, crates/ui, crates/story, examples. */
export const gpuiComponent = {
  repo: "https://github.com/longbridge/gpui-component",
  sha: "b77f352589503a114cdf709507c0738a88e28364",
  date: "2026-08-19",
  subject:
    "title_bar: Skip drawn window controls under server-side decorations (#2773)",
  crates: {
    "gpui-base": "0.5.2",
    "gpui-component": "0.5.2",
    "gpui-component-story": "0.5.1",
  },
  dir: ".work/gpui-component",
} as const;

/**
 * Zed GPUI snapshot from that gpui-component Cargo.lock.
 * Reference for runtime behavior only — not a crate we port.
 */
export const zedGpui = {
  repo: "https://github.com/zed-industries/zed",
  sha: "e0931d5a9dbf4f781b336fdf448739e74a2ac0b5",
  date: "2026-08-17",
  subject: "Reuse char-scan invisibles detection in `highlight_invisibles` (#62715)",
  crates: {
    gpui: "0.2.2",
    gpui_platform: "0.2.2",
    gpui_macros: "0.2.2",
  },
  lock: "git+https://github.com/zed-industries/zed#e0931d5a9dbf4f781b336fdf448739e74a2ac0b5",
} as const;

/**
 * The layout crate that gpui-component's Cargo.lock resolves for `gpui`,
 * which asks for `taffy = "=0.12.2"`. We port it: `src/taffy/` is a C++ port
 * of exactly this version, and `src/gpui` lays out through it. See
 * `src/taffy/readme.md`.
 */
export const taffy = {
  repo: "https://github.com/DioxusLabs/taffy",
  version: "0.12.2",
  crateSha256: "340a09581f29809fc0df82a3955501dc7f2a21f887e5d1c13dbe288fe1c0bef4",
  dir: "src/taffy",
} as const;

/**
 * The CommonMark + GFM parser gpui-component's `crates/ui/Cargo.toml` asks
 * for (`markdown = { version = "1.0.0", features = ["serde"] }`). We port it:
 * `src/markdown/` is a C++ port of exactly this version, and
 * `component::TextView` parses through it. See `src/markdown/readme.md`.
 */
export const markdown = {
  repo: "https://github.com/wooorm/markdown-rs",
  version: "1.0.0",
  crateSha256: "a5cab8f2cadc416a82d2e783a1946388b31654d391d1c7d92cc1f03e295b1deb",
  dir: "src/markdown",
} as const;

/**
 * The webview crate `crates/webview` (the `gpui-wry` crate) is built on:
 * `wry = { version = "0.53.3", package = "lb-wry" }`, longbridge's fork. We
 * port it: `src/wry/` is a C++ port of exactly this version, and
 * `src/webview/` is the gpui-side view `crates/webview` is. See
 * `src/wry/readme.md` for what is ported and what is not.
 */
export const wry = {
  repo: "https://github.com/tauri-apps/wry",
  crate: "lb-wry",
  version: "0.53.3",
  crateSha256: "d9cfe72bff8acf9af0d6d276569be5b9cb3f313f9882761ada5a50d3044214d4",
  dir: "src/wry",
} as const;

export function rustTreeDir(repoRoot: string): string {
  return join(repoRoot, gpuiComponent.dir);
}

function decode(buf: Uint8Array | undefined): string {
  return buf ? new TextDecoder().decode(buf) : "";
}

function gitOut(args: string[], cwd: string): { ok: boolean; out: string; err: string } {
  const r = Bun.spawnSync(["git", ...args], { cwd, stdout: "pipe", stderr: "pipe" });
  return { ok: (r.exitCode ?? 1) === 0, out: decode(r.stdout).trim(), err: decode(r.stderr).trim() };
}

function gitRun(args: string[], cwd: string): number {
  const r = Bun.spawnSync(["git", ...args], { cwd, stdout: "inherit", stderr: "inherit" });
  return r.exitCode ?? 1;
}

function headSha(dir: string): string | null {
  const r = gitOut(["rev-parse", "HEAD"], dir);
  return r.ok && r.out ? r.out : null;
}

function hasGit(dir: string): boolean {
  return existsSync(join(dir, ".git"));
}

function checkoutPin(dir: string, sha: string): void {
  if (headSha(dir) === sha) {
    return;
  }
  const have = gitOut(["cat-file", "-t", sha], dir);
  if (!have.ok) {
    console.log(`Fetching gpui-component ${sha.slice(0, 12)}`);
    if (gitRun(["fetch", "--depth", "1", "origin", sha], dir) !== 0) {
      if (gitRun(["fetch", "origin", sha], dir) !== 0) {
        throw new Error(`git fetch ${sha} failed in ${dir}`);
      }
    }
  }
  if (gitRun(["checkout", "--detach", "--force", sha], dir) !== 0) {
    throw new Error(`git checkout ${sha} failed in ${dir}`);
  }
  if (headSha(dir) !== sha) {
    throw new Error(`HEAD in ${dir} is ${headSha(dir)}, wanted ${sha}`);
  }
}

/**
 * Clone or reset `.work/gpui-component` to `gpuiComponent.sha`.
 *
 * The tree is a reading reference, not a build input, so a compile-only run
 * (CI) can skip the clone with GPUI_NO_RUST_TREE=1.
 */
export function ensureRustTree(repoRoot: string): string {
  const dir = rustTreeDir(repoRoot);
  const sha = gpuiComponent.sha;
  if (process.env["GPUI_NO_RUST_TREE"]) {
    return dir;
  }
  if (hasGit(dir)) {
    if (headSha(dir) === sha) {
      return dir;
    }
    console.log(`Updating ${gpuiComponent.dir} to ${sha.slice(0, 12)}`);
    checkoutPin(dir, sha);
    return dir;
  }
  if (existsSync(dir)) {
    rmSync(dir, { recursive: true, force: true });
  }
  mkdirSync(dir, { recursive: true });
  console.log(`Cloning ${gpuiComponent.repo} @ ${sha.slice(0, 12)} -> ${gpuiComponent.dir}`);
  if (gitRun(["init"], dir) !== 0) {
    throw new Error(`git init failed in ${dir}`);
  }
  if (gitRun(["remote", "add", "origin", gpuiComponent.repo], dir) !== 0) {
    throw new Error(`git remote add failed in ${dir}`);
  }
  if (gitRun(["fetch", "--depth", "1", "origin", sha], dir) !== 0) {
    throw new Error(`git fetch ${sha} failed (is git on PATH and the network up?)`);
  }
  if (gitRun(["checkout", "--detach", "FETCH_HEAD"], dir) !== 0) {
    throw new Error(`git checkout FETCH_HEAD failed in ${dir}`);
  }
  if (headSha(dir) !== sha) {
    throw new Error(`HEAD in ${dir} is ${headSha(dir)}, wanted ${sha}`);
  }
  return dir;
}

if (import.meta.main) {
  const root = resolve(dirname(Bun.main), "..");
  console.log("gpui-component", gpuiComponent.sha, gpuiComponent.date);
  console.log("  ", gpuiComponent.subject);
  console.log("  crates", gpuiComponent.crates);
  console.log("zed gpui     ", zedGpui.sha, zedGpui.date, "(reference only)");
  console.log("crates ported", `taffy ${taffy.version} -> ${taffy.dir}`);
  console.log("             ", `markdown ${markdown.version} -> ${markdown.dir}`);
  console.log("             ", `${wry.crate} ${wry.version} -> ${wry.dir}`);
  try {
    const dir = ensureRustTree(root);
    console.log("tree", dir, headSha(dir));
  } catch (e) {
    console.error(e instanceof Error ? e.message : e);
    process.exit(1);
  }
}

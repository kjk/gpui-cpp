// Exact upstream checkins this C++ port is matching.
// Source of truth — AGENTS.md / port-*.md point here. Bump the SHAs (then
// re-run bun cmd/build.ts or bun cmd/versions.ts) when ingesting a newer
// gpui-component. Do not read HEAD of a random clone.

import { existsSync, mkdirSync, rmSync } from "node:fs";
import { dirname, join, resolve } from "node:path";

/** Spec we port: crates/base, crates/ui, crates/story, examples. */
export const gpuiComponent = {
    repo: "https://github.com/longbridge/gpui-component",
    sha: "da4f93696dc2b2b4d91bcc42412b9053a3d24de8",
    date: "2026-08-16",
    subject: "docs: Fix EditorState::new() API usage in editor documentation (#2739)",
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
    sha: "cc053a4a6fa2fd0e8793201ed9099466af1be0b1",
    date: "2026-08-07",
    subject: "gpui: Expose accessibility identifiers to platform clients (#61926)",
    crates: {
        gpui: "0.2.2",
        gpui_platform: "0.2.2",
        gpui_macros: "0.2.2",
    },
    lock: "git+https://github.com/zed-industries/zed#cc053a4a6fa2fd0e8793201ed9099466af1be0b1",
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

/** Clone or reset `.work/gpui-component` to `gpuiComponent.sha`. */
export function ensureRustTree(repoRoot: string): string {
    const dir = rustTreeDir(repoRoot);
    const sha = gpuiComponent.sha;
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
    try {
        const dir = ensureRustTree(root);
        console.log("tree", dir, headSha(dir));
    } catch (e) {
        console.error(e instanceof Error ? e.message : e);
        process.exit(1);
    }
}

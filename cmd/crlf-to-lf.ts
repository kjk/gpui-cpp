// Convert CRLF (and lone CR) to LF in text files.
//   bun cmd/crlf-to-lf.ts            # whole repo
//   bun cmd/crlf-to-lf.ts src/ui     # one path or more
//
// Skips .git, out, node_modules, .work, binaries (NUL byte or known extension).

import { existsSync, readdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { dirname, extname, join, resolve } from "node:path";

const root = resolve(dirname(Bun.main), "..");

const skipDir = new Set([".git", "out", "node_modules", ".work", ".grok"]);
const skipExt = new Set([
    ".exe",
    ".dll",
    ".obj",
    ".lib",
    ".pdb",
    ".ilk",
    ".bmp",
    ".png",
    ".jpg",
    ".jpeg",
    ".gif",
    ".webp",
    ".ico",
    ".woff",
    ".woff2",
    ".ttf",
    ".otf",
    ".zip",
    ".7z",
    ".pdf",
]);

function walk(dir: string, out: string[]) {
    for (const name of readdirSync(dir)) {
        if (skipDir.has(name)) {
            continue;
        }
        const p = join(dir, name);
        const st = statSync(p);
        if (st.isDirectory()) {
            walk(p, out);
        } else if (st.isFile()) {
            out.push(p);
        }
    }
}

function toLf(buf: Buffer): Buffer | null {
    let crs = 0;
    for (let i = 0; i < buf.length; i++) {
        if (buf[i] === 13) {
            crs++;
        }
    }
    if (crs === 0) {
        return null;
    }
    const out = Buffer.allocUnsafe(buf.length);
    let j = 0;
    for (let i = 0; i < buf.length; i++) {
        if (buf[i] === 13) {
            if (i + 1 < buf.length && buf[i + 1] === 10) {
                continue;
            }
            out[j++] = 10;
            continue;
        }
        out[j++] = buf[i];
    }
    return out.subarray(0, j);
}

function convertFile(path: string): boolean {
    if (skipExt.has(extname(path).toLowerCase())) {
        return false;
    }
    const buf = readFileSync(path);
    if (buf.includes(0)) {
        return false;
    }
    const next = toLf(buf);
    if (!next) {
        return false;
    }
    writeFileSync(path, next);
    return true;
}

const args = Bun.argv.slice(2);
const targets = args.length > 0 ? args.map((a) => resolve(root, a)) : [root];
const files: string[] = [];
for (const t of targets) {
    if (!existsSync(t)) {
        console.error(`missing: ${t}`);
        process.exit(1);
    }
    const st = statSync(t);
    if (st.isDirectory()) {
        walk(t, files);
    } else {
        files.push(t);
    }
}

let n = 0;
for (const f of files) {
    if (convertFile(f)) {
        n++;
        console.log(f.startsWith(root) ? f.slice(root.length + 1) : f);
    }
}
console.log(n === 0 ? "No CRLF files." : `Converted ${n} file(s) to LF.`);

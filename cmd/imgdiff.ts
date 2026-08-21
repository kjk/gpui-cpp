// Compare two screenshots, or two directories of them:
//
//   bun cmd/imgdiff.ts <a.png> <b.png>
//   bun cmd/imgdiff.ts <dirA> <dirB> [-skip=32] [-tol=90] [-bbox]
//
//   -skip=N   ignore the top N rows. The window caption paints active or
//             inactive depending on whether the shot won the foreground, so a
//             batch run and a single run disagree there for reasons that have
//             nothing to do with the code. 32 covers the title bar.
//   -tol=N    the channel-sum a pixel has to differ by to count as a "big"
//             difference rather than antialiasing noise. Default 90.
//   -bbox     also print the bounding box of what differs, which is usually
//             enough to say which widget moved.
//
// Exits 1 if anything differs, so a sweep can be a check rather than a report.
//
// Why this exists: the same job in PowerShell took 4.1 s per image pair --
// 6.4 minutes to compare one sweep of 93 -- because a per-pixel loop there is
// interpreted. Two thirds of that was spent proving that identical files are
// identical, which a byte compare answers in a millisecond. The rest is a
// typed-array loop, which bun JITs. The PNG decoder is 40 lines because the
// only PNGs it ever sees are the ones cmd/winapi.ts writes: 8-bit, RGB or
// RGBA, not interlaced.
import { readdirSync, readFileSync, statSync } from "node:fs";
import { inflateSync } from "node:zlib";
import { basename, join } from "node:path";

type Image = { w: number; h: number; ch: number; px: Uint8Array };

function decodePng(buf: Buffer): Image {
  if (buf.readUInt32BE(0) !== 0x89504e47) {
    throw new Error("not a PNG");
  }
  let pos = 8;
  let w = 0;
  let h = 0;
  let ch = 0;
  const idat: Buffer[] = [];
  while (pos < buf.length) {
    const len = buf.readUInt32BE(pos);
    const type = buf.toString("ascii", pos + 4, pos + 8);
    const body = buf.subarray(pos + 8, pos + 8 + len);
    if (type === "IHDR") {
      w = body.readUInt32BE(0);
      h = body.readUInt32BE(4);
      const depth = body[8];
      const color = body[9];
      const interlace = body[12];
      if (depth !== 8 || interlace !== 0 || (color !== 2 && color !== 6)) {
        throw new Error(`unsupported PNG: depth ${depth} color ${color} interlace ${interlace}`);
      }
      ch = color === 6 ? 4 : 3;
    } else if (type === "IDAT") {
      idat.push(body);
    } else if (type === "IEND") {
      break;
    }
    pos += 12 + len;
  }
  const raw = inflateSync(Buffer.concat(idat));
  const stride = w * ch;
  const px = new Uint8Array(h * stride);
  // Undo the per-scanline filter: PNG filters 0..4, each relative to the
  // pixel to the left (a), the one above (b) and the one above-left (c).
  for (let y = 0; y < h; y++) {
    const filter = raw[y * (stride + 1)];
    const src = y * (stride + 1) + 1;
    const dst = y * stride;
    for (let i = 0; i < stride; i++) {
      const x = raw[src + i]!;
      const a = i >= ch ? px[dst + i - ch]! : 0;
      const b = y > 0 ? px[dst - stride + i]! : 0;
      const c = y > 0 && i >= ch ? px[dst - stride + i - ch]! : 0;
      let v: number;
      switch (filter) {
        case 0:
          v = x;
          break;
        case 1:
          v = x + a;
          break;
        case 2:
          v = x + b;
          break;
        case 3:
          v = x + ((a + b) >> 1);
          break;
        case 4: {
          const p = a + b - c;
          const pa = Math.abs(p - a);
          const pb = Math.abs(p - b);
          const pc = Math.abs(p - c);
          v = x + (pa <= pb && pa <= pc ? a : pb <= pc ? b : c);
          break;
        }
        default:
          throw new Error(`bad PNG filter ${filter} on row ${y}`);
      }
      px[dst + i] = v & 0xff;
    }
  }
  return { w, h, ch, px };
}

type Result = { any: number; big: number; total: number; box: number[] | null };

function compare(a: Image, b: Image, skip: number, tol: number): Result {
  let anyN = 0;
  let bigN = 0;
  let x0 = a.w;
  let x1 = -1;
  let y0 = a.h;
  let y1 = -1;
  for (let y = skip; y < a.h; y++) {
    const ra = y * a.w * a.ch;
    const rb = y * b.w * b.ch;
    for (let x = 0; x < a.w; x++) {
      const ia = ra + x * a.ch;
      const ib = rb + x * b.ch;
      const d =
        Math.abs(a.px[ia]! - b.px[ib]!) +
        Math.abs(a.px[ia + 1]! - b.px[ib + 1]!) +
        Math.abs(a.px[ia + 2]! - b.px[ib + 2]!);
      if (d === 0) {
        continue;
      }
      anyN++;
      if (d > tol) {
        bigN++;
      }
      if (x < x0) x0 = x;
      if (x > x1) x1 = x;
      if (y < y0) y0 = y;
      if (y > y1) y1 = y;
    }
  }
  const total = a.w * (a.h - skip);
  return { any: anyN, big: bigN, total, box: x1 < 0 ? null : [x0, x1, y0, y1] };
}

const argv = Bun.argv.slice(2);
let skip = 0;
let tol = 90;
let wantBox = false;
const rest: string[] = [];
for (const arg of argv) {
  if (arg.startsWith("-skip=")) {
    skip = Number(arg.slice(6));
  } else if (arg.startsWith("-tol=")) {
    tol = Number(arg.slice(5));
  } else if (arg === "-bbox") {
    wantBox = true;
  } else {
    rest.push(arg);
  }
}
if (rest.length !== 2) {
  console.error("usage: bun cmd/imgdiff.ts <a> <b> [-skip=N] [-tol=N] [-bbox]");
  process.exit(2);
}
const [aPath, bPath] = rest as [string, string];
const isDir = statSync(aPath).isDirectory();
const names = isDir
  ? readdirSync(aPath)
      .filter((f) => f.endsWith(".png"))
      .sort()
  : [""];

let differing = 0;
let identical = 0;
let missing = 0;
for (const name of names) {
  const fa = isDir ? join(aPath, name) : aPath;
  const fb = isDir ? join(bPath, name) : bPath;
  const label = isDir ? name : `${basename(fa)} vs ${basename(fb)}`;
  let bufB: Buffer;
  try {
    bufB = readFileSync(fb);
  } catch {
    console.log(`${label.padEnd(26)} MISSING on the other side`);
    missing++;
    continue;
  }
  const bufA = readFileSync(fa);
  // The encoder is deterministic, so identical renders are identical files.
  // This is the case that matters: it is most of them, and it costs nothing.
  if (bufA.equals(bufB)) {
    identical++;
    continue;
  }
  const ia = decodePng(bufA);
  const ib = decodePng(bufB);
  if (ia.w !== ib.w || ia.h !== ib.h) {
    console.log(`${label.padEnd(26)} SIZE ${ia.w}x${ia.h} vs ${ib.w}x${ib.h}`);
    differing++;
    continue;
  }
  const r = compare(ia, ib, skip, tol);
  if (r.any === 0) {
    // Byte-different, pixel-identical: only the skipped rows moved.
    identical++;
    continue;
  }
  differing++;
  const pct = (n: number) => ((n / r.total) * 100).toFixed(2) + "%";
  let line = `${label.padEnd(26)} any=${String(r.any).padEnd(8)} (${pct(r.any)})  big=${String(r.big).padEnd(8)} (${pct(r.big)})`;
  if (wantBox && r.box) {
    line += `  bbox x ${r.box[0]}..${r.box[1]} y ${r.box[2]}..${r.box[3]}`;
  }
  console.log(line);
}
console.log(`-- ${identical} identical, ${differing} differing${missing ? `, ${missing} missing` : ""}`);
process.exit(differing > 0 ? 1 : 0);

// Build a gpui2 example for the web and serve it, the way cmd/run.ts builds
// and launches a native one.
//
//   bun cmd/run-wasm.ts hello_world
//   bun cmd/run-wasm.ts -dbg showcase
//   bun cmd/run-wasm.ts -no-build -port 8080 story
//   bun cmd/run-wasm.ts tests            # runs under node, prints, exits
//
// A wasm module has to come off a server: a browser refuses to instantiate
// one fetched from a file:// URL, so there is no double-clicking the .html.
// This is that server and nothing more — no caching, no compression, no
// directory listing.

import { existsSync, statSync } from "node:fs";
import { dirname, join, resolve, extname } from "node:path";
import { emsdkNode, findEmcc } from "./emsdk.ts";

const root = resolve(dirname(Bun.main), "..");
process.chdir(root);

const usage = `Usage: bun cmd/run-wasm.ts [-rel|-dbg] [-no-build] [-no-open] [-port N] <example>

  -rel        release (default)
  -dbg        debug
  -no-build   serve what is already in out/wasm/, without compiling
  -no-open    do not launch a browser
  -port N     listen on N (default 8000; the next free port if it is taken)

tests and bench have no window: they are run under emsdk's node and their
output is printed here.`;

function die(msg: string): never {
  console.error(msg);
  console.error("");
  console.error(usage);
  process.exit(1);
}

let debug = false;
let build = true;
let open = true;
let port = 8000;
let target = "";
const argv = Bun.argv.slice(2);
for (let i = 0; i < argv.length; i++) {
  const a = argv[i]!;
  if (a === "-dbg") {
    debug = true;
  } else if (a === "-rel") {
    debug = false;
  } else if (a === "-no-build") {
    build = false;
  } else if (a === "-no-open") {
    open = false;
  } else if (a === "-port") {
    port = Number(argv[++i]);
    if (!Number.isFinite(port) || port <= 0) {
      die("-port wants a number");
    }
  } else if (a.startsWith("-")) {
    die(`Unknown flag: ${a}`);
  } else {
    target = a.toLowerCase();
  }
}
if (!target) {
  die("Pass an example name");
}

function run(cmd: string[]): number {
  const r = Bun.spawnSync(cmd, { cwd: root, stdout: "inherit", stderr: "inherit" });
  return r.exitCode ?? 1;
}

if (build) {
  const rc = run(["bun", join(root, "cmd/build-wasm.ts"), debug ? "-dbg" : "-rel", target]);
  if (rc !== 0) {
    process.exit(rc);
  }
}

const outDir = join(root, "out", "wasm", debug ? "dbg" : "rel");
const isConsole = target === "tests" || target === "bench";

// tests and bench print and exit; there is nothing to serve. emsdk ships the
// node they were built against, so use that one when it is there.
if (isConsole) {
  const js = join(outDir, `${target}.js`);
  if (!existsSync(js)) {
    die(`${js} not built`);
  }
  process.exit(run([emsdkNode(findEmcc()), js]));
}

const page = join(outDir, `${target}.html`);
if (!existsSync(page)) {
  die(`${page} not built`);
}

const mime: Record<string, string> = {
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".mjs": "text/javascript; charset=utf-8",
  ".wasm": "application/wasm",
  ".data": "application/octet-stream",
  ".json": "application/json",
  ".css": "text/css; charset=utf-8",
  ".svg": "image/svg+xml",
  ".png": "image/png",
  ".map": "application/json",
};

function serve(p: number) {
  return Bun.serve({
    port: p,
    fetch(req) {
      let path = new URL(req.url).pathname;
      if (path === "/" || path === "") {
        path = `/${target}.html`;
      }
      // Everything is served out of the one output directory: no traversal
      // out of it, and nothing else on the disk is reachable.
      const abs = join(outDir, path.replace(/^\/+/, ""));
      if (!abs.startsWith(outDir) || !existsSync(abs) || !statSync(abs).isFile()) {
        return new Response("not found", { status: 404 });
      }
      return new Response(Bun.file(abs), {
        headers: {
          "content-type": mime[extname(abs).toLowerCase()] ?? "application/octet-stream",
          // A rebuild while the tab is open should be one reload away.
          "cache-control": "no-store",
        },
      });
    },
  });
}

let server: ReturnType<typeof serve> | null = null;
for (let p = port; p < port + 20 && !server; p++) {
  try {
    server = serve(p);
  } catch {
    // In use; try the next one.
  }
}
if (!server) {
  die(`No free port in ${port}..${port + 19}`);
}

const url = `http://localhost:${server.port}/`;
console.log(`serving ${outDir} at ${url}`);
console.log("ctrl-c to stop");

if (open) {
  const cmd =
    process.platform === "win32"
      ? ["cmd", "/c", "start", "", url]
      : process.platform === "darwin"
        ? ["open", url]
        : ["xdg-open", url];
  Bun.spawn(cmd, { stdout: "ignore", stderr: "ignore" });
}

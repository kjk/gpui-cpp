// MSVC build for gpui2 examples. Requires cl.exe on PATH.
//   bun cmd/build.ts                         # print example list
//   bun cmd/build.ts app_assets
//   bun cmd/build.ts -dbg all
//   bun cmd/build.ts -rel -asan system_monitor
//   bun cmd/build.ts -rel -clean showcase

import { existsSync, mkdirSync, cpSync, rmSync, readdirSync, statSync } from "node:fs";
import { basename, dirname, join, resolve } from "node:path";
import { ensureRustTree } from "./versions.ts";

const root = resolve(dirname(Bun.main), "..");
process.chdir(root);

const simpleExamples = [
    "hello_world",
    "window_title",
    "root_borderless",
    "dialog_overlay",
    "focus_trap",
    "fps_monitor",
    "input",
    "sidebar",
    "tooltip_top_edge",
    "table_in_scrollable",
    "text_selection",
    "markdown_table",
];

const knownTargets = ["system_monitor", "app_assets", "showcase", "story", "all", ...simpleExamples] as const;
type Target = (typeof knownTargets)[number];

const usage = `Usage: bun cmd/build.ts [-rel|-dbg] [-asan] [-clean] <example>

  -rel    release (default)
  -dbg    debug
  -asan   AddressSanitizer; combines with -rel or -dbg
  -clean  delete out/<dir>/ before building

Outputs:
  out/rel/          release
  out/dbg/          debug
  out/rel_asan/     release + asan
  out/dbg_asan/     debug + asan`;

function printExamples(msg?: string): never {
    if (msg) {
        console.error(msg);
        console.error("");
    }
    console.error(usage);
    console.error("");
    console.error("Examples:");
    for (const n of knownTargets) {
        console.error(`  ${n}`);
    }
    process.exit(1);
}

function die(msg?: string): never {
    if (msg) {
        console.error(msg);
    }
    console.error(usage);
    process.exit(1);
}

function parseArgs(argv: string[]): { target: Target; debug: boolean; asan: boolean; clean: boolean } {
    if (argv.length === 0 || argv[argv.length - 1].startsWith("-")) {
        printExamples();
    }
    const last = argv[argv.length - 1];
    const name = last.toLowerCase();
    if (!(knownTargets as readonly string[]).includes(name)) {
        printExamples(`Unknown example: ${last}`);
    }
    const target = name as Target;

    let sawRel = false;
    let sawDbg = false;
    let asan = false;
    let clean = false;
    for (const raw of argv.slice(0, -1)) {
        if (raw === "-rel") {
            sawRel = true;
            continue;
        }
        if (raw === "-dbg") {
            sawDbg = true;
            continue;
        }
        if (raw === "-asan") {
            asan = true;
            continue;
        }
        if (raw === "-clean") {
            clean = true;
            continue;
        }
        if (raw.startsWith("-")) {
            die(`Unknown flag: ${raw}`);
        }
        die(`Unknown argument: ${raw}`);
    }
    if (sawRel && sawDbg) {
        die("Cannot combine -rel and -dbg");
    }
    return { target, debug: sawDbg, asan, clean };
}

function outDirName(debug: boolean, asan: boolean): string {
    const base = debug ? "dbg" : "rel";
    return asan ? `${base}_asan` : base;
}

const baseSrc = [
    "src/base/Arena.cpp",
    "src/base/Arena_win.cpp",
    "src/base/Base.cpp",
    "src/base/Base_win.cpp",
    "src/base/Str.cpp",
    "src/base/Strconv.cpp",
    "src/base/StrFormatParse.cpp",
    "src/base/StrUtf8.cpp",
];

const gpuiSrc = [
    "src/gpui/Gpui.cpp",
    "src/gpui/Window.cpp",
    "src/gpui/Assets.cpp",
    "src/gpui/Svg.cpp",
];

function cppDir(rel: string): string[] {
    const dir = join(root, rel);
    if (!existsSync(dir)) {
        return [];
    }
    return readdirSync(dir)
        .filter((f) => f.endsWith(".cpp"))
        .map((f) => `${rel}/${f}`)
        .sort();
}

function uiSrc(): string[] {
    return cppDir("src/ui");
}

function componentSrc(): string[] {
    return cppDir("src/component");
}

const libs = [
    "d2d1.lib",
    "dwrite.lib",
    "dwmapi.lib",
    "psapi.lib",
    "ole32.lib",
    "windowscodecs.lib",
    "user32.lib",
    "gdi32.lib",
    "gdiplus.lib",
    "shlwapi.lib",
    "uxtheme.lib",
    "comctl32.lib",
    "oleaut32.lib",
];

function sourcesFor(name: string): string[] | null {
    if (name === "system_monitor") {
        return [...gpuiSrc, ...uiSrc(), ...componentSrc(), "src/sys/SysInfo.cpp", "src/examples/system_monitor.cpp"];
    }
    if (name === "app_assets") {
        return [...gpuiSrc, ...uiSrc(), ...componentSrc(), "src/examples/app_assets.cpp"];
    }
    if (name === "showcase") {
        const dir = join(root, "src/examples/showcase");
        const files = readdirSync(dir)
            .filter((f) => f.endsWith(".cpp"))
            .map((f) => `src/examples/showcase/${f}`)
            .sort();
        return [...gpuiSrc, ...uiSrc(), ...componentSrc(), ...files];
    }
    if (name === "story") {
        const dir = join(root, "src/examples/story");
        const files = readdirSync(dir)
            .filter((f) => f.endsWith(".cpp"))
            .map((f) => `src/examples/story/${f}`)
            .sort();
        return [...gpuiSrc, ...uiSrc(), ...componentSrc(), ...files];
    }
    if (simpleExamples.includes(name)) {
        return [...gpuiSrc, ...uiSrc(), ...componentSrc(), `src/examples/${name}.cpp`];
    }
    return null;
}

function copyAssets(outDir: string) {
    const src = join(root, "assets");
    if (!existsSync(src)) {
        return;
    }
    const dst = join(root, outDir, "assets");
    mkdirSync(dst, { recursive: true });
    cpSync(src, dst, { recursive: true });
}

function clDir(): string | null {
    const r = Bun.spawnSync(["where", "cl"], { stdout: "pipe", stderr: "pipe" });
    if (r.exitCode !== 0) {
        return null;
    }
    const first = new TextDecoder().decode(r.stdout).split(/\r?\n/).find((l) => l.trim().length > 0);
    if (!first) {
        return null;
    }
    return dirname(first.trim());
}

// ASan's own runtime (not the VC++ redistributable). Needed next to the exe.
function copyAsanDll(outDir: string) {
    const dir = clDir();
    if (!dir) {
        return;
    }
    const name = "clang_rt.asan_dynamic-x86_64.dll";
    const src = join(dir, name);
    if (!existsSync(src)) {
        return;
    }
    cpSync(src, join(root, outDir, name));
}

function groupSources(files: string[]): { key: string; files: string[] }[] {
    const buckets: Record<string, string[]> = { base: [], gpui: [], ui: [], component: [], sys: [], ex: [] };
    for (const f of files) {
        if (f.startsWith("src/base/")) {
            buckets.base.push(f);
        } else if (f.startsWith("src/gpui/")) {
            buckets.gpui.push(f);
        } else if (f.startsWith("src/ui/")) {
            buckets.ui.push(f);
        } else if (f.startsWith("src/component/")) {
            buckets.component.push(f);
        } else if (f.startsWith("src/sys/")) {
            buckets.sys.push(f);
        } else {
            buckets.ex.push(f);
        }
    }
    return Object.entries(buckets)
        .filter(([, list]) => list.length > 0)
        .map(([key, list]) => ({ key, files: list }));
}

function runCl(args: string[]) {
    const r = Bun.spawnSync(["cl", ...args], {
        cwd: root,
        stdout: "inherit",
        stderr: "inherit",
    });
    if (r.exitCode !== 0) {
        process.exit(r.exitCode ?? 1);
    }
}

function buildOne(name: string, debug: boolean, asan: boolean) {
    const src = sourcesFor(name);
    if (!src) {
        die(`Unknown target: ${name}`);
    }

    const outName = outDirName(debug, asan);
    const outDir = join("out", outName);
    mkdirSync(join(root, outDir), { recursive: true });

    const cflags = [
        "/nologo",
        "/std:c++20",
        "/EHsc",
        "/utf-8",
        "/I",
        "src",
        "/DUNICODE",
        "/D_UNICODE",
        "/W3",
        "/wd4996",
        "/wd4244",
        "/wd4267",
        "/MP",
        "/FS",
        "/Zi",
        // /MT /MTd: static CRT. Do not use /MD — that pulls vcruntime140.dll.
        ...(debug ? ["/Od", "/MTd", "/DDEBUG"] : ["/O2", "/MT", "/DNDEBUG"]),
    ];
    if (asan) {
        cflags.push("/fsanitize=address");
    }

    const cfg = `${debug ? "dbg" : "rel"}${asan ? "+asan" : ""}`;
    console.log(`Building ${name} (${cfg}) -> ${outDir}\\${name}.exe`);

    // Separate /Fo dirs so src/ui/Button.cpp and examples/showcase/button.cpp
    // do not both write button.obj.
    const objs: string[] = [];
    // AppLog.cpp implements Base.h log() / loga() / _uploadDebugReport for every example.
    for (const g of groupSources(["src/examples/AppLog.cpp", ...src, ...baseSrc])) {
        const objDir = join(outDir, "obj", g.key);
        mkdirSync(join(root, objDir), { recursive: true });
        runCl([...cflags, "/c", `/Fo${objDir}\\`, `/Fd${objDir}\\`, ...g.files]);
        // Only this compile's objs — leftover example .obj from another target
        // (e.g. system_monitor.obj after a showcase build) must not be linked.
        for (const srcFile of g.files) {
            objs.push(join(objDir, basename(srcFile).replace(/\.cpp$/i, ".obj")));
        }
    }

    const link = [
        "/link",
        "/SUBSYSTEM:WINDOWS",
        "/ENTRY:wWinMainCRTStartup",
        "/NODEFAULTLIB:msvcrt.lib",
        "/NODEFAULTLIB:msvcrtd.lib",
        "/NODEFAULTLIB:ucrt.lib",
        "/NODEFAULTLIB:ucrtd.lib",
        "/NODEFAULTLIB:vcruntime.lib",
        "/NODEFAULTLIB:vcruntimed.lib",
        ...libs,
    ];
    if (asan) {
        link.push("/INCREMENTAL:NO");
    }
    link.push("/DEBUG", `/PDB:${outDir}\\${name}.pdb`);
    runCl(["/nologo", `/Fe${outDir}\\${name}.exe`, `/Fd${outDir}\\`, ...objs, ...link]);

    if (asan) {
        copyAsanDll(outDir);
    }
    copyAssets(outDir);
    console.log(`Built ${outDir}\\${name}.exe`);
}

function formatElapsed(ms: number): string {
    const total = Math.max(0, Math.round(ms / 1000));
    const h = Math.floor(total / 3600);
    const m = Math.floor((total % 3600) / 60);
    const s = total % 60;
    if (h > 0) {
        return `${h}h ${m}m ${s}s`;
    }
    if (m > 0) {
        return `${m}m ${s}s`;
    }
    return `${s}s`;
}

function formatExactBytes(n: number): string {
    return `${n.toLocaleString("en-US")} b`;
}

function formatHumanBytes(n: number): string {
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

function printFileSize(relPath: string) {
    const abs = join(root, relPath);
    if (!existsSync(abs)) {
        console.log(`${relPath}  (not generated)`);
        return;
    }
    const n = statSync(abs).size;
    console.log(`${relPath}  ${formatExactBytes(n)}  ${formatHumanBytes(n)}`);
}

const started = performance.now();
const { target, debug, asan, clean } = parseArgs(Bun.argv.slice(2));
try {
    ensureRustTree(root);
} catch (e) {
    console.error(e instanceof Error ? e.message : e);
    process.exit(1);
}
const outDir = join("out", outDirName(debug, asan));
if (clean) {
    const abs = join(root, outDir);
    if (existsSync(abs)) {
        console.log(`Cleaning ${outDir}\\`);
        rmSync(abs, { recursive: true, force: true });
    }
}
const built: string[] = [];
if (target === "all") {
    built.push("system_monitor", "app_assets", "showcase", ...simpleExamples);
} else {
    built.push(target);
}
for (const n of built) {
    buildOne(n, debug, asan);
}

console.log("");
for (const n of built) {
    printFileSize(join(outDir, `${n}.exe`));
    printFileSize(join(outDir, `${n}.pdb`));
}
console.log(`elapsed ${formatElapsed(performance.now() - started)}`);

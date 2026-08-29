// Run clang-tidy over the source translation units used by this host build.
// The arguments before `--` are passed to clang-tidy; arguments after it are
// appended to the compiler command line (for example, a project-specific
// -D). The source list follows build.ts's platform suffix rules.
//
//   bun cmd/clang-tidy.ts -checks=bugprone-*,performance-*
//   bun cmd/clang-tidy.ts -fix -- -DGPUI_MARKDOWN_MINI=1

import { dirname, join } from "path";
import { existsSync, readdirSync } from "fs";

type Platform = "win" | "linux" | "mac";

const root = process.cwd();

function die(message: string): never {
  console.error(message);
  process.exit(1);
}

function hostPlatform(): Platform {
  switch (process.platform) {
    case "win32": return "win";
    case "linux": return "linux";
    case "darwin": return "mac";
    default: die(`Unsupported platform: ${process.platform}`);
  }
}

function sourcePlatform(rel: string, plat: Platform): boolean {
  if (/_win\.cpp$/.test(rel)) return plat === "win";
  if (/_linux\.cpp$/.test(rel)) return plat === "linux";
  if (/_mac\.cpp$/.test(rel)) return plat === "mac";
  if (/_wasm\.cpp$/.test(rel)) return false;
  if (/_mem_posix\.cpp$/.test(rel)) return plat === "linux" || plat === "mac";
  if (/_posix\.cpp$/.test(rel)) return plat === "linux" || plat === "mac";
  return true;
}

function sourceFiles(rel: string, plat: Platform): string[] {
  const dir = join(root, rel);
  if (!existsSync(dir)) return [];
  const result: string[] = [];
  for (const ent of readdirSync(dir, { withFileTypes: true }).sort((a, b) => a.name.localeCompare(b.name))) {
    const child = `${rel}/${ent.name}`;
    if (ent.isDirectory()) {
      result.push(...sourceFiles(child, plat));
    } else if ((ent.name.endsWith(".cpp") || ent.name.endsWith(".c")) && sourcePlatform(child, plat)) {
      result.push(child);
    }
  }
  return result;
}

function findExecutable(name: string): string | null {
  const finder = process.platform === "win32" ? "where.exe" : "which";
  const r = Bun.spawnSync([finder, name], { stdout: "pipe", stderr: "pipe" });
  if ((r.exitCode ?? 1) !== 0 || !r.stdout) return null;
  const text = new TextDecoder().decode(r.stdout);
  return text.split(/\r?\n/).map((s) => s.trim()).find((s) => s.length > 0) ?? null;
}

function findClangTidy(): string {
  const fromPath = findExecutable(process.platform === "win32" ? "clang-tidy.exe" : "clang-tidy");
  if (fromPath) return fromPath;

  // Visual Studio installs clang-tidy beside clang-cl, even when neither is
  // added to PATH. This also works from a regular PowerShell prompt.
  if (process.platform === "win32") {
    const clangCl = findExecutable("clang-cl.exe");
    if (clangCl) {
      const sibling = join(dirname(clangCl), "clang-tidy.exe");
      if (existsSync(sibling)) return sibling;
    }
  }
  die("clang-tidy was not found. Install LLVM (or the Visual Studio C++ Clang tools) and put it on PATH.");
}

function main(): void {
  const tidyArgs: string[] = [];
  const extraArgs: string[] = [];
  let afterSeparator = false;
  for (const arg of Bun.argv.slice(2)) {
    if (arg === "--") {
      afterSeparator = true;
    } else if (afterSeparator) {
      extraArgs.push(arg);
    } else {
      tidyArgs.push(arg);
    }
  }

  const plat = hostPlatform();
  const files = sourceFiles("src", plat);
  if (files.length === 0) die("No source files found under src/.");
  const exe = findClangTidy();
  const cxxCompileArgs = [
    "-std=c++20",
    "-I", "src",
    "-I", "src/gpui",
    "-include", "markdown/markdown.h",
    "-include", "base/lib.h",
    "-include", "ui/lib.h",
    "-include", "gpui/paint.h",
    "-include", "gpui/assets.h",
    "-include", "gpui/svg.h",
    "-include", "gpui/accessibility_win.h",
    "-include", "sys/executor.h",
    ...(plat === "win" ? ["-DWIN_BACKEND_ALL=1", "-DUNICODE", "-D_UNICODE"] : []),
    ...extraArgs,
  ];
  const cCompileArgs = ["-std=c11", ...extraArgs];

  console.log(`Running ${exe} on ${files.length} source files (${plat})`);
  for (const file of files) {
    const compileArgs = file.endsWith(".c") ? cCompileArgs : cxxCompileArgs;
    const result = Bun.spawnSync([exe, ...tidyArgs, file, "--", ...compileArgs], {
      cwd: root,
      stdout: "inherit",
      stderr: "inherit",
    });
    if ((result.exitCode ?? 1) !== 0) {
      die(`clang-tidy failed for ${file} (exit ${result.exitCode ?? 1})`);
    }
  }
}

main();

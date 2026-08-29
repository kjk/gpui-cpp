# gpui-cpp-dist

The amalgamated build of [gpui-cpp](https://github.com/kjk/gpui-cpp): GPUI is
`gpui.h` and `gpui.cpp`, and its QuickJS-NG engine is the separate generated
`quickjs/quickjs.h` and `quickjs/quickjs.c`. Everything beside those four files
is here so you can run them before you commit to them — every example, the
assets they load, and the build and run scripts used by the source repo.

Nothing here is written by hand, so issues and pull requests belong in the
source repo.

## Try it

First install [bun](https://bun.sh/), then a compiler:

- **Windows** — Visual Studio 2026; the free Community edition is fine, and
  2022 works too. `build.ts` finds it with vswhere, so no developer prompt.
- **Linux** — `g++` or `clang++`, plus `pkg-config`, X11, cairo and pangocairo.
- **macOS** — the Xcode command line tools (`xcode-select --install`).

Then:

```
bun run.ts story
```

That builds and launches a comprehensive showcase of the available
functionality ([examples/story](./examples/story)) — a sidebar gallery with a
page per widget. `bun run.ts showcase` is the shorter tour, and `bun run.ts` with no
arguments lists every example and every option.

```
bun build.ts -all              # build every example, do not run one
bun run.ts -dbg input          # debug build
bun run.ts -wasm story         # build for the browser, serve it, open a tab
bun run.ts gpui_shell -- examples/js_todolist --dev
```

`gpui_shell` is the desktop JavaScript host. Its `check <directory>` command
loads and renders once without opening a window, and `types <directory>` writes
the matching `gpui.d.ts` declarations.

Windows builds Direct2D by default. Pass
`--win-backend=d2d|d3d11|d3d12|all` to `build.ts` or `run.ts` to choose what
the executable contains. An `all` build retains the process-start
`__paint=d2d|d3d11|d3d12` selector; `__msaa=1|2|4|8` controls the custom
renderers' sample count, and `__scene=off|replay|cache|skip|damage` selects the
scene optimization level (`skip` by default). Pass those after `run.ts`'s
`--`; Windows removes them from `argv` before calling `GpuiMain`.

## What is here

```
gpui.h, gpui.cpp     the C++20 library amalgam
quickjs/             pinned QuickJS-NG as one C11 header and one C source
gpui_shell/           command-line JavaScript application host
examples/            every example, including story/ and showcase/
assets/              icons, images and documents the examples load at runtime
web/shell.html       the page a -wasm build is served in
build.ts, run.ts     the source repo's own build and run scripts
```

`out/` is where builds land. Nothing else is generated in place.

## Use it

Drop the GPUI pair and `quickjs/` into your tree, `#include "gpui.h"` where you
need the API, compile `gpui.cpp` as C++20, and compile `quickjs/quickjs.c` as
C11. The platform halves are already inside `gpui.cpp` behind `GPUI_OS_*`
guards, so the same source set builds on all four:

- **Windows** — `cl /std:c++20 /EHsc /utf-8 /DUNICODE /D_UNICODE`, static CRT;
  define one of `WIN_BACKEND_DIRECT2D`, `WIN_BACKEND_D3D11` or
  `WIN_BACKEND_D3D12` and link its import libraries. With no definition the
  compatibility default is Direct2D. `WIN_BACKEND_ALL` compiles and links all
  three.
- **Linux** — `g++ -std=c++20` with `pkg-config --cflags --libs x11 cairo pangocairo`.
- **macOS** — `clang++ -std=c++20 -x objective-c++` with the Cocoa, CoreText and
  IOKit frameworks. The file is Objective-C++ because the mac half is.
- **wasm** — `em++ -std=c++20` with `-sALLOW_MEMORY_GROWTH`; the browser half
  draws through Canvas2D and needs no library at all. em++ rather than emcc:
  the link needs the C++ runtime and emcc leaves it out.

Compile QuickJS with `/TC /std:c11 /experimental:c11atomics` under MSVC
(`/experimental:c11atomics` is not needed by clang-cl), or with
`-x c -std=gnu11` under clang, GCC and emscripten. `build.ts` supplies the full
warning and platform flags.

No other dependencies, no nested build system, no STL containers.

## This copy

Amalgamated from gpui-cpp [`<checkin-sha1>`](https://github.com/kjk/gpui-cpp/commit/<checkin-sha1>).

[What has changed in gpui-cpp since](https://github.com/kjk/gpui-cpp/compare/<checkin-sha1>...main)
shows every commit this copy is behind by; if that page is empty, it is current.

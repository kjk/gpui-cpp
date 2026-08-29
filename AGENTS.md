# gpui — C++ port of gpui-component

This repository is a C++ port of [longbridge/gpui-component](https://github.com/longbridge/gpui-component) targeting **Windows, Linux, macOS and the browser** (wasm, through emscripten). The goal is to port **as much of the Rust as this tree can hold**: every module of `crates/base` and `crates/ui`, the story gallery, the showcase and the examples. Assume a thing is in scope until a hard rule below says otherwise; the answer to "should we port X?" is yes unless X needs a dependency or a runtime we have ruled out.

The Rust sources live under `.work/gpui-component/` (gitignored clone). Do not treat that tree as something to compile into this binary. Read it as the specification. `bun cmd/build.ts` and `bun cmd/run.ts` clone that tree at the pinned SHA if it is missing.

**Upstream pins** — source of truth: the pin block at the top of [`cmd/run.ts`](cmd/run.ts) (`gpuiComponent`, `zedGpui`, and the three crates we port: `taffy`, `markdown` and `wry`); `bun cmd/run.ts -versions` prints them. How to ingest a later checkin: `port-upstream.md`.

## Goal

Port gpui-component to C++, module by module, and ship its examples as native
desktop apps. `system_monitor` was the first milestone and is done; the story
gallery and the showcase came after it, and the work now is depth — the parts
of a ported module that were left behind. `port-progress.md` is the running
log of what has been done and what a session found; read its tail before
picking up something new.

Fidelity is the bar. When a widget's look or numbers are in question, read the
Rust file under `.work/gpui-component/` at the SHA in `cmd/run.ts` and
copy the constants. Where this tree has to differ — because the runtime does
not have the seam Rust does, or a dependency is out — say so in the comment
where it differs and in the progress log, rather than quietly doing something
else.

```
bun cmd/build.ts system_monitor
bun cmd/build.ts -rel hello_world
bun cmd/build.ts -rel showcase
bun cmd/build.ts -dbg -all
bun cmd/build.ts -rel -asan system_monitor
bun cmd/build.ts -wasm system_monitor
bun cmd/build.ts -clang -rel showcase   # Windows: clang-cl, not cl.exe
bun cmd/run.ts -wasm showcase

bun cmd/build.ts -rel --win-backend=d3d11 story    # fixed custom D3D11 build
bun cmd/build.ts -rel --win-backend=d3d12 story    # fixed native D3D12 build
bun cmd/run.ts -rel --win-backend=all story -- __paint=d3d12 __msaa=4 __scene=damage
out/rel/story.exe __scene=off               # draw straight, without the scene
GPUI_FRAME_BENCH=600 out/rel/story.exe      # frame time, by phase
GPUI_LAYOUT_DUMP=lay.txt out/rel/story.exe  # every frame's laid-out tree
GPUI_LAYOUT_REUSE=off out/rel/story.exe     # rebuild the taffy tree each frame
```

Rust references (in `.work/gpui-component`):

```
cargo run -p system_monitor
cargo run -p hello_world
```

Matching means:

- same window size (680×600, centered), title, dark theme
- same layout: custom title bar + segmented System/Processes tabs + content + status bar
- same widgets: area charts, progress bars, icons, striped sortable table
- same data: CPU %, memory %, process list (top 200), disk used %, battery %
- same refresh cadence (500 ms) and history length (120 samples)
- same interaction: tab switch, column sort, window drag, min/max/close, Alt+F4, vertical scroll

It does **not** mean a line-for-line clone of Zed's GPUI renderer or Blade.
Those are the layer *under* gpui-component, and we reimplement a subset of them
rather than port them. **Three crates are the exception**, and each is a full
C++ port at the version gpui-component pins: `src/taffy/` lays out every box in
this tree, `src/markdown/` parses every `TextView`, and `src/wry/` is the
webview `crates/webview` puts in a window. See `src/taffy/readme.md`,
`src/markdown/readme.md` and `src/wry/readme.md`.

`crates/shell` is in scope too. Its JavaScript engine is the explicit vendoring
exception: the exact QuickJS-NG revision recorded in `cmd/run.ts` is checked out
only under `.work/quickjs-ng`, and `bun cmd/update-quickjs.ts` regenerates the one
tracked C header and one tracked C source under `src/quickjs/`. Ordinary builds
never fetch it. `quickjs-libc.c` is not included; Shell owns its sandboxed
module loader, scheduler and capability-gated system APIs.

## Non-goals

These are the standing exclusions. Everything else in gpui-component is in
scope, and a module being large or unglamorous is not a reason to skip it.

- Zed's scene graph as a whole, and general-purpose C++ futures/coroutines.
  Shell's JavaScript promises are the narrow exception: they are implemented
  over callback jobs, `ExecSpawn`, `WindowPost` and window timers, with no C++
  async runtime. Two halves of the scene graph are
  here and neither is the whole. `src/gpui/paintgpu.h` is the renderer half —
  one instance buffer a frame, SDF rounded rects and borders, a glyph atlas,
  stencil-and-cover paths, which is what Blade and `directx_renderer.rs` do —
  and it is compiled only with `WIN_BACKEND_D3D11`, `WIN_BACKEND_D3D12` or
  `WIN_BACKEND_ALL`.
  `src/gpui/scene.h` is the collection
  half, and it *is* on: a frame's drawing gathered as a flat array of
  primitives, each carrying its own content mask and its layer, hashed against
  the last frame's. What that bought was not batching, which the GPU backend
  already did, but culling — three quarters of the story gallery's primitives
  are masked down to nothing before they are drawn — a path-geometry cache,
  and not drawing a frame that is identical to the one before it. What it is
  still not: no stacking context per element, so layers are a field rather
  than a tree; no offscreen mask cache; no batching across windows; and only
  `paint_win.cpp` dispatches into it, so the other three backends draw the way
  they always did. Both headers say what they are worth and what they are
  short of, and `__scene=off` is how to take the scene back out. We do have
  `App`/`Window`/`Entity`/`Ctx`, actions and a keymap, `EventEmitter`, window
  subscriptions and an executor — see below — but not refcounted entities,
  observers, futures, or a `Task<T>` that cancels by being dropped.
  `src/sys/executor.h` is GPUI's foreground/background pair written as
  callbacks: a queue the main thread drains and a pool of threads that fills
  it, with integer handles where Rust has a handle whose destructor is the
  cancel
- STL containers (`std::string`, `std::vector`, `std::map`, iostreams, `std::function` as the default callback style)
- Reusing `../gpui/` — that experiment uses STL heavily and is not the base for this port
- A general network beyond one GET. `src/sys/http.h` fetches the bytes at an http(s)
  URL with the OS's own client — WinHTTP, NSURLSession, libcurl — because a
  remote image needs it. There is no POST, no session, no socket and no TLS
  of ours, and a bigger client wants a bigger reason than "it would be tidy".
  What it is for: `gpui/image.h`. Shell's default-denied capabilities are the
  exception: its private platform seams implement the bounded HTTP, TCP and
  WebSocket surface upstream exposes, without turning `src/sys/http.h` into a
  general client.
- Anything that needs a third-party C++ library, by hard rule 3: tree-sitter
  and syntect (so `highlighter` stays the small hand-written lexer it is), an
  LSP client, resvg, ropey, html5ever. Where Rust reaches for one of these and
  the feature is still worth having, write the small version this tree needs,
  or port the crate the way `src/taffy`, `src/markdown` and `src/wry` are
  ported; `port-upstream.md` lists which is which. The webview is the worked
  example of the second route: `src/wry/` is a port of the wry crate, and the
  two things Rust gets from crates on Windows — the WebView2 COM bindings and
  Microsoft's loader — are declared and written out in `wry_win.cpp` rather
  than vendored. Windows and macOS have backends — WebView2 and WKWebView —
  and `src/wry/readme.md` says what the other two would take

A thing that is *not* ported for a reason other than these belongs in
`port-progress.md` with the reason, so the next session does not have to
rediscover it.

## Hard rules

1. **No STL data structures.** C headers and the C++ headers SumatraPDF already uses (`cstdint`, `cstring`, `new`, `algorithm` for `std::min`/`std::max`, `utility`) are allowed. Do not introduce `std::string`, `std::vector`, `std::unique_ptr`, `std::optional`, `std::function`, `std::unordered_map`.
2. **Use SumatraPDF base types.** `Str`, `Vec<T>`, `Arena`, `StrBuilder`, `fmt()`, `uint8_t`/`int32_t`/`uint32_t`/`int64_t`/`uint64_t`, `Func0`/`Func1`. Source of truth: `C:\Users\kjk\src\sumatrapdf\src\base`. A curated copy lives in `src/base.h` / `src/base.cpp` so this tree builds without that checkout, and it is `namespace base`. Everything else in `src/` lives in `namespace gpui` (themed widgets in `gpui::component`), which takes the base in with a using-directive, so gpui code writes `Str` unqualified and `gpui::Str` still names it from outside. Examples `#include "gpui.h"` and `using namespace gpui;`.

   The two ported crates are the reason for the split. `src/taffy` and `src/markdown` are ports of crates that have never heard of gpui, so they are written against `base.h` and nothing else: they include no gpui header and name no gpui symbol, and `cmd/update-dist.ts` fails the build if that stops being true. Keep it that way when adding to either — anything one of them needs from the tree belongs in `base`, or it does not belong to them.
3. **Three platforms, no third-party C++ libraries.** Windows: MSVC `cl.exe` on PATH, static CRT (`/MT` / `/MTd`) — no VC++ redistributable DLLs — plus WinHTTP for `src/sys/http_win.cpp`. Linux: g++ or clang++ with the system X11, cairo and Pango, found through `pkg-config`, and libcurl the same way when it is installed (the one soft dependency: without it the tree still builds and only loses remote images). macOS: clang++ with Cocoa, Core Graphics, Core Text, IOKit and Foundation's NSURLSession from the system SDK. `bun cmd/build.ts` picks the toolchain by host. Do not add CMake, vcpkg, or a C++ package manager. There is no `ext/`: what Rust gets from a crate this tree either writes itself or ports (`src/taffy`, `src/markdown`). QuickJS-NG is the sole vendored-source exception, pinned and reduced by `cmd/update-quickjs.ts` to `src/quickjs/quickjs.h` plus `quickjs.c`; it is compiled directly as C11 and brings no build system or transitive library.
4. **POD-friendly C++.** Prefer structs with explicit ownership. `Vec<T>` is memcpy/POD only. Heap strings are `Str` owned by `StrDup` / `StrFree` or an `Arena`. Frame UI trees allocate from a per-frame `Arena` and are discarded, not destructed as a graph of C++ objects.
5. **No exceptions, no RTTI needed.** COM (`Direct2D` / `DirectWrite`) uses HRESULT checks, not C++ exceptions.
6. **When unsure about a widget's look or numbers, read the Rust file** under `.work/gpui-component/` (the SHA in `cmd/run.ts`) and copy constants (heights, gaps, colors, column widths). Do not invent a different design system.
7. **Portable by default.** `GPUI_OS_WINDOWS` / `GPUI_OS_LINUX` / `GPUI_OS_MAC` are for the handful of places where a single expression differs. Anything larger gets a portable signature in a shared header and an implementation in `<name>_win.cpp`, `<name>_linux.cpp` and `<name>_mac.cpp`. Never call an OS API from a shared file.

## What we are actually porting

Rust stack for `system_monitor`:

```
examples/system_monitor
        │
        ▼
gpui-component  (Theme, Root, TitleBar, Tab/TabBar, AreaChart,
                 Progress, Icon, DataTable)
        │
        ▼
gpui-base       (Tabs, Progress parts, table behavior, h_flex/v_flex)
        │
        ▼
gpui + gpui_platform   (Zed: window, layout via Taffy, GPU scene,
                        Entity/Context, timers, input)
        │
        ▼
sysinfo + battery      (process/CPU/mem/disk + battery)
```

C++ stack we implement:

```
examples/system_monitor.cpp   MonitorApp: a view entity with Render(self, cx)
        │
        ▼
src/ui/     Theme, TitleBar, TabBar, AreaChart, Progress, Icon, Table, Root;
            component::TextView parses through src/markdown
        │
        ▼
src/base/   unstyled primitives the themed layer is built from
        │
        ▼
src/gpui/   App + Window + entity store, hit-test, timer, frame arena;
            layout through src/taffy, paint through paint.h, the OS window
            through platform.h
        │
        ▼
src/taffy/  the taffy crate, ported; every El box is laid out by it
src/markdown/  the markdown crate, ported; every TextView is parsed by it
        │
        ▼
src/gpui/paint_win.cpp     Direct2D + DirectWrite
src/gpui/paint_linux.cpp   cairo + Pango
src/gpui/paint_mac.cpp     Core Graphics + Core Text
src/gpui/window_win.cpp    Win32 message loop
src/gpui/window_linux.cpp  X11 event loop
src/gpui/window_mac.cpp    Cocoa event loop
        │
        ▼
src/sys/    process/CPU/memory/disk/battery, per OS; the executor and the
            fetch table, portable
        │
        ▼
src/base.h  Str, Vec, Arena, Geom, Color helpers
```

## Portability

`src/base.h` defines `GPUI_OS_WINDOWS`, `GPUI_OS_LINUX`, `GPUI_OS_MAC` and
`GPUI_OS_WASM` from the compiler's own predefines; exactly one is 1. They exist
for one-expression differences (the path separator, `SRWLOCK` vs
`pthread_mutex_t`). Everything bigger is a portable signature plus one
implementation per platform:

| Seam                                       | Shared header          | Windows                   | Linux                       | macOS                     | wasm                       |
| ------------------------------------------ | ---------------------- | ------------------------- | --------------------------- | ------------------------- | -------------------------- |
| virtual memory, paths, strings, self usage | `src/base.h` (`Plat*`) | `src/base_win.cpp`        | `src/base_linux.cpp`        | `src/base_mac.cpp`        | `src/base_wasm.cpp`        |
| 2D drawing and shaped text                 | `src/gpui/paint.h`     | `src/gpui/paint_win.cpp`  | `src/gpui/paint_linux.cpp`  | `src/gpui/paint_mac.cpp`  | `src/gpui/paint_wasm.cpp`  |
| the OS window and its event loop           | `src/gpui/platform.h`  | `src/gpui/window_win.cpp` | `src/gpui/window_linux.cpp` | `src/gpui/window_mac.cpp` | `src/gpui/window_wasm.cpp` |
| system metrics                             | `src/sys/sysinfo.h`    | `src/sys/sysinfo_win.cpp` | `src/sys/sysinfo_linux.cpp` | `src/sys/sysinfo_mac.cpp` | `src/sys/sysinfo_wasm.cpp` |
| one HTTP GET                               | `src/sys/http.h`       | `src/sys/http_win.cpp`    | `src/sys/http_linux.cpp`    | `src/sys/http_mac.cpp`    | `src/sys/http_wasm.cpp`    |
| a webview in the window                    | `src/wry/wry.h`        | `src/wry/wry_win.cpp`     | `src/wry/wry_linux.cpp`     | `src/wry/wry_mac.cpp`     | `src/wry/wry_wasm.cpp`     |

`_posix.cpp` is the fourth suffix: Linux, macOS **and** wasm compile it, since
emscripten's libc answers for strings, directories, threads and the clock the
way the other two do. What it cannot answer is mmap with a reserve/commit
split, so that half is `_mem_posix.cpp` and only the two hosted targets take
it — `src/base_wasm.cpp` writes its own against a linear heap that grows.

`src/gpui/window_common.cpp` holds everything a window does that is not the OS
window — frame drawing, input dispatch, the app lifecycle — and all platform
files call into it.

### Three Windows backends

`paint.h` has three implementations on Windows and makes the choice at compile
time. Define exactly one of `WIN_BACKEND_DIRECT2D`, `WIN_BACKEND_D3D11` or
`WIN_BACKEND_D3D12`; if none is defined, Direct2D is selected because its
mature driver path, WARP fallback and DirectWrite ClearType output make it the
widest compatibility choice. `WIN_BACKEND_ALL` compiles all three and retains
the process-start `__paint=d2d|d3d11|d3d12` selector. Fixed builds ignore an
unavailable backend choice. The repository build script's equivalent is
`--win-backend=d2d|d3d11|d3d12|all`.

`paint_win.cpp` implements Direct2D on a D3D11 device over a flip-model swap
chain — already on the GPU, and what the default build and screenshots use.
`paintgpu_win.cpp` is GPUI's own shape of renderer: a frame is one instance buffer of rounded
rects, borders, glyphs, images and gradients, the shape and the content mask
are evaluated in the pixel shader, and path fills go through stencil-and-cover
rather than a tessellator. The CPU batching, shaders, atlas and path machinery
are shared; only native resource and command submission differ. The D3D12 half
owns a command queue, triple-buffered allocators, persistent upload heaps and
descriptor tables; it does not use D3D11On12. `__msaa=1|2|4|8` sets the
custom renderers' sample count. `WinPaintBackend`, `WinPaintMsaa`,
`WinSceneMode` and `WinPaintOptions` hold all three selections; Windows fills
them while stripping the reserved arguments before `GpuiMain`, and
`WinPaintOptionsGet()` is the paint backends' typed accessor.

The four Shader Model 5 entry points live in `src/gpui/paintgpu_win.hlsl` and
are not compiled at application startup. `bun cmd/update-win-shaders.ts` runs
FXC with `/O3 /WX` and rewrites the checked-in
`src/gpui/paintgpu_shaders_win.cpp` only when its output changes. That file
stores DXBC as basE95 over every printable ASCII character in raw strings;
the renderer decodes it once into BSS. `cmd/build.ts` checks the HLSL SHA-256
recorded in the generated file and rejects stale bytecode. Ordinary builds do
not need FXC, do not link D3DCompiler, and do not load D3DCompiler_47.dll.

All three share everything device-independent rather than writing it three
times: the DirectWrite factory and its formats, the `IDWriteTextLayout` a
`TextLayout*` is on Windows — so shaping, measurement, hit-testing and range rects are the
same code on all three — and WIC decode. Direct2D and the D3D11 custom half
also share the D3D11 device. Only the target and drawing differ, and
`paint_win.cpp` hands over in one line per entry
point. Read `src/gpui/paintgpu.h` before touching either: it has the measured
numbers, the reason it is not the default, and the two gaps that would have to
close first (subpixel glyph positioning, dashes on a rounded rect).

### The scene

`src/gpui/scene.h` sits between the element tree and `paint.h` on Windows: the
tree's drawing is collected as a flat array of primitives rather than issued to
a backend as it walks, and the array is then replayed through the same
`paint.h` entry points — so all three Windows backends draw it and none can tell.
It is on at the `skip` level; `__scene=off|replay|cache|skip|damage` turns it
down, and `off` is the first thing to try if a frame ever comes out stale, since
this is the only thing in the tree that can decide not to draw. On the story
gallery it takes the paint phase from 1.41 ms to 0.24 on Direct2D and from 0.59
to 0.11 on the GPU backend. It costs 11% on a scene where every primitive
changes every frame, and 0.03% of the story's pixels, on curve edges, because a
cached path is filled from a geometry realization. Read the header before
touching it.

`GPUI_FRAME_BENCH=<n>` draws n frames back to back and prints what each cost,
split into building the element tree, laying it out and painting it. It is how
the two were compared and is the right tool for any question about frame time;
without the variable it is inert.

### The browser

`bun cmd/build.ts -wasm <example>` builds a page instead of a binary, and
`bun cmd/run.ts -wasm <example>` builds it, serves it and opens it — a wasm
module has to come off a server, so there is no double-clicking the html.
Emscripten is found through `$EMCC`, `$EMSDK`, `PATH` or a sibling `.emsdk`
checkout, and only ever when the target is wasm; nothing else is needed, because the browser half draws through
Canvas2D and takes no library at all. `web/shell.html` is the page: it puts a
canvas called `gpui-canvas` at the top left of the viewport and does nothing
else, which is what lets `window_wasm.cpp` read a `clientX` as a window
coordinate. Assets are preloaded into MEMFS at `/assets`, so
`gpui/assets.cpp` walks them with the same `fopen` it uses everywhere.

The whole platform layer is `EM_JS`, and its state lives on one
`globalThis.__gpui` object handed back to C++ as integer ids. Note for anyone
adding to it: an `EM_JS` body is stringified by the preprocessor, so it can
hold no empty `''`, no regex literal, and no backslash outside a string
literal. Use `""`, and build a `RegExp` from a string if you ever need one.

Where a page is not a desktop, and why:

- **One window.** A tab has one canvas; a second `WindowOpen` answers null.
- **`AppRun` does not return.** `emscripten_set_main_loop` unwinds the stack
  and hands the tab back to the browser, which is the only way a C main loop
  and an event loop share a thread. Nothing after `AppRun` runs.
- **No threads.** `PlatThreadRun` fails, so `ExecSpawn` queues the job on the
  main thread's own queue rather than dropping it — `ExecHasThreads()` is how
  anything that cares can tell. Rule 1 still holds: the job is written as if
  it were elsewhere.
- **No `HttpGet`.** Everything a page can fetch with is asynchronous and
  `HttpGet` blocks, so a remote image renders as its alt text, exactly as a
  Linux build without libcurl does. `src/sys/http_wasm.cpp` says what a real
  one would need.
- **`ImageDecode` answers before the picture is decoded**, for the same
  reason. The `Image` comes back sized zero and fills itself in, and the load
  repaints. SVG never goes through it — `src/gpui/svg.h` turns SVG into draw
  ops, which is most of what this tree draws.
- **The clipboard is a mirror** kept by the DOM `paste` event, and the paste
  chord is driven by that event rather than by its keydown.
- **`sysinfo` reports the tab**, not the machine: the wasm heap as memory,
  `navigator.hardwareConcurrency`, the Battery Status API. No process table,
  no host CPU, no disk.
- **An arena reserves 4 MB, not 64.** wasm has no reserve/commit split, so a
  reservation is spent memory; `PlatArenaReserveSize()` is the seam.

An example never names an OS API. It implements `int GpuiMain(int argc,
char** argv)`; the runtime provides `wWinMain` / `main`. Key codes are the
`Key*` constants in `Gpui.h` (the Win32 `VK_*` values, which the X11 window
maps keysyms onto), and the clipboard is `ClipboardSetText`.

`cmd/update-dist.ts` amalgamates `src/` into `gpui.h` and `gpui.cpp`, then copies
the separately compiled `quickjs/quickjs.h` and `quickjs/quickjs.c`. All four
are the same on every platform. `.work/` is gitignored and is what
every build compiles — `bun cmd/build.ts`, `cmd/test.ts` and CI all go through
it. The published copy is a repo of its own,
[gpui-cpp-dist](https://github.com/kjk/gpui-cpp-dist), cloned to
`.work/gpui-cpp-dist` and refreshed only by running `bun cmd/update-dist.ts` by
hand: that syncs the clone, writes the GPUI and QuickJS pairs into it, builds every example
against it (`GPUI_AMALGAM_DIR` points the platform build at that copy, and its
objects go to their own `out/*_dist` tree), rewrites its readme with the
gpui-cpp commit it came from and a compare link showing what it is behind by,
then commits and pushes it. Never regenerate a published copy as part of a
build, a test run or a commit; `buildDist()` takes a required `outDir` so an
automatic caller has to say `.work` out loud. A checkout that is dirty or not
on `main` is removed and cloned again rather than repaired: the script writes
the whole of it and commits whatever `git status` reports, so a stray file
would be published.

The snapshot is a checkout, not four source files. Beside both pairs go every
example, `gpui_shell/`, `assets/`, `web/shell.html`, `build.ts` and `run.ts` at
the top level, and
`winapi.ts` + `mac-window-place.m` because `run.ts -compare` reaches for them
by name — so `bun run.ts story` works in a fresh clone of it. Its `.gitignore`
is written from here too, since `-compare` clones the Rust tree *into* the
snapshot. `readme-dist.md` in this repo is the published readme, copied over
with `<checkin-sha1>` filled in; edit it here, never there.

`build.ts` and `run.ts` are copied verbatim rather than forked, and discover
which tree they are in at startup: `isDist` is `existsSync(scriptDir +
"/gpui.h")`, and the repo root, `amalgamDir()`, the `out/` layout, the target
list and the usage text all hang off it. Two rules follow. `cmd/update-dist.ts`
must be imported *dynamically* — it does not exist over there, and a static
import fails at load, which is why `ensureAmalgam` and `build()` are async.
And anything either script reaches for by path must be resolved against
`scriptDir`, not `<root>/cmd/`. Keep both runnable in both trees; `bun
cmd/update-dist.ts` builds every example against the snapshot, which is the
check that catches it. The two differ in what they do
with the text: the published copy is read as a document, so the comments come
out, runs of blank lines collapse, and the `#include` lines are lifted to the
top of `gpui.cpp` and de-duplicated — the portable ones first, then one guarded block
per platform, which has to stay below the portable code because `<X11/Xlib.h>`
defines `None` and `Window`. `.work/` is the sources concatenated and nothing
else, comments and all, so a line in it is the line its `#line 1 "src/..."`
marker says and a diagnostic lands where you expect. Each `_win.cpp` /
`_linux.cpp` / `_mac.cpp` / `_posix.cpp` sits in `gpui.cpp` inside its own `#if
GPUI_OS_*`, so `<windows.h>`, `<X11/Xlib.h>` and `<Cocoa/Cocoa.h>` still never
reach one translation unit — the preprocessor drops the other two halves before
anything parses them. macOS compiles the whole file as Objective-C++, because
the mac half is.

## Source of truth for visuals

Dark theme from `crates/ui/src/theme/default-theme.json` ("Default Dark") resolved against `default-colors.json`:

| Token                            | Hex                     |
| -------------------------------- | ----------------------- |
| background                       | `#0a0a0a` (neutral-950) |
| foreground                       | `#fafafa` (neutral-50)  |
| border                           | `#262626` (neutral-800) |
| muted.foreground                 | `#a3a3a3` (neutral-400) |
| title_bar / tab_bar / status_bar | `#171717`               |
| title_bar.border / window.border | `#262626`               |
| tab.active.background            | `#0a0a0a`               |
| tab.active.foreground            | `#fafafa`               |
| tab.foreground                   | `#d4d4d4`               |
| table.background                 | `#0a0a0a`               |
| table.head.foreground            | `#525252`               |
| table.row.border                 | `#262626` @ ~70%        |
| table even row                   | `#171717` @ 40%         |
| progress_bar                     | `#f5f5f5`               |
| base.red                         | `#f87171` (red-400)     |
| base.green                       | `#4ade80` (green-400)   |
| base.blue                        | `#60a5fa` (blue-400)    |
| base.yellow                      | `#facc15` (yellow-400)  |
| danger (close hover)             | `#f87171`               |
| secondary.hover                  | `#292929`               |
| secondary.active                 | `#212121`               |

Layout constants from the Rust example / components:

- Window: `680 × 600`, centered
- Title bar height: `34`
- Title bar left pad (Windows): `12`
- Segmented tab inner height: `24`
- Status bar height: `28` (`h_7`)
- Chart min height: `160`
- Chart axis gutter: `18`
- Chart tick margin: every 15 points
- History: 120 samples, 500 ms interval
- Process columns: PID 70, Name 380, CPU 80, Memory 100
- Keep top 200 processes after sort
- Default sort: CPU descending
- Status chips: width 135, progress `w_12` × `h_2` (48×8)

Typography: Segoe UI, 16 px base. `text_sm` = 14, `text_xs` = 12. Spacing uses a 4 px grid (`gap_2` = 8, `p_3` = 12, `gap_4` = 16).

## App, Window, Entity, Ctx

The runtime mirrors GPUI's shape. Read this before touching `src/gpui`, adding an example, or writing a widget that owns state.

| GPUI (Rust)                      | Here                                                                                |
| -------------------------------- | ----------------------------------------------------------------------------------- |
| `App`                            | `App` — D2D/DirectWrite factories, shared fonts, window list, entity store          |
| `Window`                         | `Window` — hwnd, render target, frame arena, hover/focus, its root view             |
| `Entity<T>`                      | `Entity<T>` — a POD generational handle; `App` owns the state                       |
| `Context<T>`                     | `Ctx` — `{app, win, a, self}`; one type, GPUI only splits it for the borrow checker |
| `impl Render for T`              | `static El* T::Render(T* self, Ctx* cx)`                                            |
| `cx.new(...)`                    | `EntityNew<T>(app)`                                                                 |
| `cx.listener(...)`               | `Listen(cx, &T::OnThing)` / `ListenTo(entity, &T::OnThing)`                         |
| `cx.listener(move \|…\| … ix …)` | `Listen(cx, &T::OnThing, ix)` — the captured value                                  |
| `cx.notify()`                    | `Notify(cx)` — the entity, its observers, the windows it is on                       |
| `cx.observe(&e, ..)`             | `Observe(cx, e, &T::OnChanged)`; `ObserveTo` where `SubscribeTo` would be           |
| `Drop for T`                     | `~T()`, run when the entity is dropped                                              |
| `window.use_keyed_state`         | `KeyedState<T>(cx, key)`                                                            |
| `cx.spawn(...)` with no await    | `WindowPost(win, Listener)` — runs against its entity on the next pass             |
| `cx.background_spawn(work)`      | `ExecSpawn(work, done)` — `sys/executor.h`; `done` is posted to the main thread     |
| `Task<T>` dropped                | `ExecCancel(id)`, or the entity going stale, which drops a post the way it does a timer |
| `Timer::after(d).await`          | `WindowSetTimeout(win, ms, Listener)`                                               |

A view is a plain struct with state, a static `Render`, and static handlers:

```cpp
struct Example {
    int clicks = 0;

    static void OnGo(Example* self, Ctx* cx, const ClickEvent*) {
        self->clicks++;
        Notify(cx);
    }

    static El* Render(Example* self, Ctx* cx) {
        return Div(cx->a)->Child(
            component::Button::New(cx, StrL("go"))
                ->Label(StrL("Let's Go!"))
                ->OnClick(Listen(cx, &Example::OnGo))
                ->IntoEl());
    }
};

int GpuiMain(int argc, char** argv) {
    App* app = AppNew();
    ThemeSet(app, ThemeMode::Light);
    return AppRunView(StrL("Example"), 800, 600,
                      EntityNew<Example>(app).id, app, WinOpts{});
}
```

Rules:

1. **State lives in an entity, never in a `static` or a `void*` payload.** One view type per screen or page; if two things have independent state they are two entities.
2. **`Ctx*` is the first parameter** of anything that builds elements — components, page helpers, everything. The frame arena is `cx->a`; do not pass `Arena*` around.
3. **Elements carry their own listener.** `->OnClick(Listen(cx, &T::Handler))`. Dispatch resolves the entity and drops the event if the handle went stale, so a listener can outlive its view safely. There is no click-id switch anywhere; do not add one.
4. **Bind the value instead of decoding an id.** What a Rust closure captures, `Listen` takes as an `intptr_t`: `Listen(cx, &T::OnTab, ix)`. A component that produces the value itself — which day, which row — fills it with `ListenerArg` and its caller writes `Listen(cx, &T::OnDay)`.
5. `El::Click(id)` is identity only: hit-testing, hover, focus, Tab traps — GPUI's `ElementId`. It does not dispatch. `WindowOnUnhandledClick` fires for a click no element handled, which is the outside click that dismisses an overlay. A widget that wants the press itself rather than the click takes `El::OnMouseDown` / `OnMouseUp` / `OnDragMove` — `div().on_mouse_down(..)` and `on_drag_move` — where the element that took the press keeps the moves until the button comes back up, which is what GPUI's drag entity does for a drag.
6. Window-level input is a subscription bound to a view, one per event type the way `window.on_mouse_event::<T>` is: `WindowOnKey`, `WindowOnMouseDown` / `Up` / `Move` / `Exit`, `WindowOnScrollWheel`, `WindowSetInterval` / `WindowSetTimeout` (GPUI spells the last two `cx.spawn` + `Timer::after`). The handler takes the matching GPUI event — `MouseDownEvent`, `ScrollWheelEvent` — and the platform window builds those into a `PlatformInput` for `WindowDispatchInput`, which is Rust's `Window::dispatch_event`. Any number of timers can be armed; each returns a handle for `WindowCancelTimer`, and one whose view goes stale is dropped the way Rust drops a `Task` with its entity. Work that is not on a clock but on another thread goes through `src/sys/executor.h`: `ExecSpawn` runs it on the pool and `WindowPost` brings the answer back to a listener on the main thread, which is where everything the UI owns is touched. A worker never reaches for an entity, a window or the frame arena.
7. A blinking caret is state, not a function of the clock: `BlinkStart` / `Stop` / `Pause` / `Visible`, a port of `crates/base/src/input/base/blink_cursor.rs`. Sampling `TimeNow()` at paint time instead makes the caret invisible whenever nothing happens to repaint during the lit half. One cursor per field, as an entity — `InputState::blink` is Rust's `InputState::blink_cursor`. `InputFocus` / `InputBlur` start and stop it, the way Rust's `on_focus` / `on_blur` do, and the runtime does the same handoff for whichever `InputState` a view points `win->input` at, so only a custom text widget has to do it itself. `AppRequestAnim` is for real animation (the FPS HUD), never for a caret.
8. `Notify(cx)` marks the entity in hand and schedules a repaint of the windows that entity is on. It is GPUI's `App::notify`: the observers of that entity run (`Observe(cx, entity, &T::OnChanged)` is `cx.observe`), and a window is invalidated only when the entity that notified is one of the views it rendered last frame — GPUI's `Window::dirty_views`, kept here as `Window::rendered`. An entity nothing has rendered yet — a state entity that is not a view, a view on its first frame — falls back to the window the notify came from, and to every window when it came from none. What is still coarser than GPUI: a repaint rebuilds the whole window's element tree, because an `El` is arena-allocated per frame and hover, focus and animation are resolved while it is built. Layout is not rebuilt with it — see the layout cache below.
9. Entity handles are generational, not refcounted. `Entity<T>::Get` returns null once the slot is recycled; check it.
10. **Geometry is `Point` / `Size` / `Bounds` / `Edges`, all DIP floats, named the way Rust names them.** Rust's `Point<T>` and friends are generic over a *unit* — `Pixels`, `ScaledPixels`, `DevicePixels` — not over an element type, and everything above `Paint.h` here is DIPs, so the parameter is gone and the arithmetic is written out once. Use them for what is produced, returned or passed whole: a measured `Size`, a hit box, the positioner's arguments. Code that writes one component at a time — the layout pass over `El`, a mouse event's position — keeps flat fields. A unit that is not DIPs gets a named struct of its own (`WinSize` carries both the DIP and the device-pixel size of a window), never a template.

`AppNew` → `WindowOpenView` → `AppRun` → `AppFree` is the whole lifecycle; `AppRunView` is the one-window shorthand. There is no hook table.

## Layout

`src/taffy/` is a C++ port of [taffy](https://github.com/DioxusLabs/taffy)
0.13.0 — the crate Zed's GPUI lays out with, and therefore the one that defines
what gpui-component's layout *means*. Flexbox, CSS Grid, block layout and
floats are all there, and so are ports of the crate's own unit tests
(`tests/TaffyTests.cpp`). It lives in `namespace taffy`, not `gpui`: `Style`,
`Overflow`, `Position` and `Display` exist in both and mean different things.

`LayoutEl` in `src/gpui/gpui.cpp` is the seam. Each frame it walks the `El`
tree, translates every `gpui::Style` into a `taffy::Style`, runs
`TaffyTree::ComputeLayoutWithMeasure`, and writes the boxes back onto the
elements. Text, icons, images and progress bars are measured leaves, which is
Rust's `request_measured_layout`. What taffy does not model — `fixed`,
`anchorBelow` / `anchorAbove` / `anchorCenterX`, the `relative(f)` half of an
inset, and scroll offsets — is applied around it, and the comment above
`LayoutEl` says how.

**The taffy tree is kept between frames, not rebuilt.** A window owns a
`LayoutCache`; each frame the element tree is reconciled against it by
position — the nth child of the nth child is the same node it was — and a node
is told something only when it has something to hear: its `taffy::Style` is
not the one it carries (`operator==` on `taffy::Style` is the `PartialEq`
derive Rust has, ported for this), or it is a measured leaf whose text, font,
weight, line height, wrap or image has moved. Everything else keeps the cache
taffy filled last frame, and a `PerformLayout` that hits that cache does not
walk the subtree at all. A hover that only recolours a box changes neither a
style taffy knows about nor a measurement, so it costs no layout.

That is what took a story frame's layout from 1.85 ms to 0.24 — the frame
from 2.34 to 0.69 — and what is left of the 0.24 is this tree's own walk, not
taffy's. `GPUI_LAYOUT_REUSE=off` rebuilds every frame the way it used to, and
is the first thing to try if a frame ever comes out laid out stale;
`GPUI_FRAME_BENCH` prints what each frame had to tell taffy about
(`nodes=1466 made=0 dropped=0 restyled=0 remeasured=0` is a page that did not
change).

When a layout question comes up, the answer is in `src/taffy/`, and behind that
in the Rust crate. Do not add a special case to `LayoutEl` for something CSS
already has a rule for; make the style translation say the right thing instead.

`gpui::Style` is meant to say everything gpui's `Styled` trait says, because a
port that cannot write what the Rust wrote has to approximate it. The one that
bit: `crates/ui` and `crates/story` never call `.flex_grow()`, so every grow in
the Rust is `.flex_1()` — grow 1, **shrink 1, basis 0** — and `El::Grow()` left
the basis at `auto`, which sizes an item by its content instead of by the line.
`El::Flex1()` is the faithful one; `Grow(f)` is for a factor that is not 1.

**The port is kept current.** When the `gpuiComponent` pin moves to a checkin
whose `Cargo.lock` resolves a different taffy, the port moves with it — bump
`taffy.version` there and diff the crate. `src/taffy/readme.md` has the
file-for-file map and `port-upstream.md` has the procedure.

## Markdown

`src/markdown/` is a C++ port of
[markdown-rs](https://github.com/wooorm/markdown-rs) 1.0.0 — the `markdown`
crate `crates/ui/Cargo.toml` asks for, and therefore the one that defines what
a `TextView`'s source *means*. CommonMark and GFM both, tables, footnotes and
task lists included. It lives in `namespace markdown`, not `gpui`: `CharKind`,
`Name`, `Link`, `Point` and `Node` exist in both and mean different things.

`MdParse` in `src/ui/text.cpp` is the seam. It calls
`markdown::ToMdast(a, source, ParseOptions::Gfm())` and folds the mdast into
the `MdNode` tree the renderer walks, which is what
`crates/ui/src/text/format/markdown.rs` does with `ast_to_node` and
`parse_paragraph`. Raw HTML — a block, an inline tag, or a whole document —
goes through `src/ui/html.cpp` into the same tree, since there is no
html5ever here.

When a parsing question comes up, the answer is in `src/markdown/`, and behind
that in the Rust crate. Do not add a special case to `MdParse` for something
CommonMark already has a rule for.

**The port is kept current**, on the same terms as taffy: bump
`markdown.version` in `cmd/run.ts` and diff the crate.
`src/markdown/readme.md` has the file-for-file map — including what is
deliberately not ported, MDX and `to_html` — and `port-upstream.md` has the
procedure.

## Code style (match SumatraPDF `src/base`)

```cpp
#include "Base.h"

struct MetricPoint {
    float cpu = 0;
    float memory = 0;
};

void FormatBytes(uint64_t bytes, StrBuilder& out);
```

- Include `"Base.h"` first. It pulls Windows headers, `Str`, `Vec`, `Arena`, `Geom`.
- `Str s = fmt("%.1f%%", cpu);` for formatting (temp-arena string; do not `free` it).
- Own a heap `Str` only if it must survive a frame: `StrDup` / `StrFree`.
- `Vec<T>` for arrays of POD. Not for `Str` graphs — use `Vec<ProcessInfo>` where `ProcessInfo` holds a `char name[kMax]` or an arena `Str`.
- `logf("...")` for debug prints.
- Prefer `int32_t` indexes. `int` is fine when matching existing base APIs (`Vec::len` is `int`).
- Strings are UTF-8 `Str` everywhere, including our own API (`WindowOpen`, `AppSetTitle`). Convert with `ToCWstrTemp(s)` only where an OS call needs UTF-16, at the call itself. Do not widen a signature to `wchar_t*` to save a conversion.
- COM interfaces: pair every successful `Create` with `Release`. No `CComPtr`.

## Build

```
bun cmd/build.ts
bun cmd/build.ts -rel system_monitor
bun cmd/run.ts
bun cmd/run.ts -dbg hello_world
bun cmd/run.ts -debugger showcase
bun cmd/run.ts -wasm story
```

`cmd/build.ts` is the whole build, for every platform: MSVC on Windows,
g++/clang++ on Linux, clang++ on macOS, emscripten with `-wasm` from any of
the three. `cmd/run.ts` imports it rather than spawning it, so a flag means
the same thing and lands in the same `out/` directory on both sides.

On Windows `cl.exe` is used off PATH when the MSVC environment is already
set; otherwise Visual Studio is found with `vswhere` (or by scanning the
default install roots, 2026 first) and its `vcvars64.bat` is run once for the
`INCLUDE`/`LIB`/`PATH` it exports, so a plain shell builds. `-clang` builds
with `clang-cl` from the same toolset instead, into `out/rel_clang/`.

Neither script downloads what the build does not need: the Rust spec tree
under `.work/` is cloned only by `cmd/run.ts -compare`, and emscripten is
only looked for with `-wasm`.

Every compile and link is echoed with a `> ` in front, the tool spelled as
its bare name — the command as written runs in a shell where that tool is on
PATH, which on Windows means a Developer Command Prompt. What the line leaves
out is printed once above it: the directory it runs from, and `Using <full
path>` for the compiler.

No example name (or a flag last) prints the valid example list. The example is the last argument.

Debug: `bun cmd/build.ts -dbg system_monitor` (writes `out/dbg/` on Windows). Release+ASan: `bun cmd/build.ts -rel -asan system_monitor` (`out/rel_asan/` on Windows). Clean rebuild of that dir: add `-clean`. Linux and macOS write under `out/linux/` and `out/mac/`, so building the same checkout for multiple platforms never clobbers another platform's output.

`bun cmd/run.ts` takes the same flags as `build.ts`, plus:

- `-debugger` runs the binary under whichever debugger this machine has —
  WinDbg then cdb on Windows, gdb then lldb on Linux, lldb on macOS. Force one
  with `-windbg`, `-cdb`, `-gdb` or `-lldb`; a named debugger that is not
  installed is an error carrying the command that installs it, never a quiet
  fallback to a different one. `-dbg` is still the debug *build*, not this.
- `-compare` cargo-builds and launches the Rust twin beside ours.
- `-versions` prints the upstream pins, syncs `.work/gpui-component` to the
  pinned SHA, and exits. The pins live at the top of `cmd/run.ts`.
- `-wasm` serves the page and opens a tab; `-no-open` and `-port N` steer that.
- `-no-build` launches what is already in `out/`.

It does not accept `-all` — pick one binary.

Linux prerequisites (g++/clang++, pkg-config, X11 + cairo + Pango headers,
gdb, bun, rust) install with `bash cmd/ubuntu-install-deps.sh`; add
`--build-only` for just what a compile needs. From a Windows checkout,
`bun cmd/wsl-run.ts -rel system_monitor` builds and runs the Linux binary
inside WSL; the window comes up through WSLg.

macOS needs the Xcode command line tools and nothing else. `bun cmd/mac-build.ts
-rel -all` compiles on a remote Mac over ssh from any host — it snapshots the
working tree into a commit, force-pushes it to a scratch branch, and has the
Mac fetch and build it. It compiles only; a Cocoa window needs a login session.

## Tests

```
bun cmd/test.ts          # build tests/ and run it
bun cmd/test.ts -dbg
```

`tests/` holds ports of the pure-logic tests in `.work/gpui-component`, one
file per Rust module, each naming the module it came from. The framework is
`utassert(cond)` and a counter — `tests/Test.h` is all of it. A test is a plain
function that `tests.cpp` calls; nothing registers itself.

Only tests that pin code we actually ported belong here. Most of upstream's are
`#[gpui::test]` and need GPUI's `TestAppContext`, which has no counterpart, and
most of the rest cover Rust we deliberately did not port. When a test needs a
seam to reach the logic — `FrameSamplerIngest` is the drain half of
`FrameSamplerTick`, split out so the rolling window can be driven without a
window — add the seam rather than the harness.

## Benchmarks

```
bun cmd/bench.ts                  # every benchmark, release, 10 samples
bun cmd/bench.ts -small -large    # the node counts the crate gates by feature
bun cmd/bench.ts -n=3 grid/deep   # fewer samples, one group
```

`bench/` holds ports of taffy's own benchmarks — large flexbox trees, wide and
deep grids, and tree construction on its own. They come from the crate's
`benches/` directory, which is a package of its own and is not published with
the crate, so it comes from the git checkout `port-upstream.md` clones. Read a
number only from a release build; a debug one measures the assertions.

`bench/MarkdownBench.cpp` is not a port — markdown-rs carries no benchmarks —
and measures the other thing in this tree with a size to it: parsing.
`bun cmd/bench.ts markdown` runs four document shapes (prose, nested
containers, GFM tables, character references) and then splits one of them into
its tokenize and to_mdast halves, so a change to `src/markdown/` can be
attributed. Run it if you touch that tree.

The harness is `bench/Bench.h` and `bench/bench.cpp`: criterion's
`iter_batched`, without criterion. Setup builds a fresh tree and is not timed,
the run is, and the row reports the median and the minimum over the samples.

These exist because layout is the one thing here with an asymptotic
complexity, and a wrong one hides: a grid item list sorted with an insertion
sort was fine on every hand-written grid in the story gallery and took 94
seconds on a 316x316 one. If you touch `src/taffy/`, run them.

CI (`.github/workflows/build.yml`) runs `bun cmd/build.ts -rel -all` and then
`bun cmd/test.ts -rel` on windows-latest, ubuntu-latest and macos-latest.
Compiling every example on all three is most of the check, and the suite is the
rest. It sets `GPUI_NO_RUST_TREE=1` so the Rust spec clone is skipped.

**Warnings are errors.** `/W4 /WX` on MSVC, `-Wall -Wextra -Werror` on
gcc/clang. Fix the warning; do not add a suppression. The only suppressions left are the same
amalgam artifact under two names — `/wd4505` and `-Wno-unused-function`, a
static helper that only one of the concatenated files uses — plus `/wd4996`
for the deprecated CRT names. Each carries a comment saying why.

After changing `.cpp` / `.h` / `.ts` files, run `bun cmd/format.ts` on those paths (or with no args for the whole tree) before finishing. It runs clang-format on C++ in `src/` and `examples/` (`/.clang-format`, Chromium-based, 80 columns) and Prettier on TypeScript (`.prettierrc.json`: `printWidth` 120, `endOfLine` lf). Use `-ts` or `-cpp` to run only Prettier or only clang-format. Do not format `.work/` or `out/`. `.gitattributes` forces `eol=lf`.

The Rust reference (optional, slow first build because it pulls Zed). `bun cmd/build.ts` and `bun cmd/run.ts -versions` both install `.work/gpui-component` at the pinned SHA:

```
bun cmd/run.ts -versions
cd .work\gpui-component
cargo run -p system_monitor
```

## Layout of this tree

```
AGENTS.md              this file
port.md                phased porting plan
port-progress.md       what is done / what is next
port-upstream.md       how to ingest later checkins (pins live in cmd/run.ts)
cmd/format.ts          clang-format src/**/*.{cpp,h} + examples/ and prettier cmd/*.ts (`-ts` / `-cpp` to run one)
cmd/build.ts           the whole build, every platform: MSVC (or clang-cl, -clang)
                       on Windows, g++/clang++ on Linux, clang++ on macOS,
                       emscripten with -wasm from any host. Finds cl.exe through
                       vswhere + vcvars when it is not already on PATH. Exports
                       its pieces so cmd/run.ts builds through it in-process
cmd/run.ts             build then run; build.ts's flags plus -debugger /
                       -windbg / -cdb / -gdb / -lldb, -compare, -no-build, and
                       -wasm (serve the page and open a tab). Also holds the
                       upstream pins — the exact gpui-component and zed gpui
                       SHAs and the taffy / markdown / wry crate versions we
                       are porting — which -versions prints and syncs
cmd/wsl-run.ts         run cmd/run.ts inside WSL from a Windows checkout
cmd/ubuntu-install-deps.sh  non-interactive apt + bun + rustup setup for Linux
cmd/shot.ts            screenshot one example; -click=X,Y clicks first (client coords).
                       Waits for the window to reach the foreground before the shutter
                       (an inactive window is composited with the inactive caption) and
                       parks the pointer off the window unless -hover asked for one.
                       Set GPUI_TODAY=YYYY-MM-DD to pin what DateToday() returns, so a
                       calendar or date picker shot is reproducible on any day.
                       -half=left|right sizes the window the way compare-story.ts does,
                       so a baseline shot is the same pixels as the compare capture
                       (-half=right is our side of the pair); sweep story pages with it.
cmd/imgdiff.ts         compare two shots, or two directories of them, and exit 1 if
                       anything differs. -skip=32 ignores the title bar, whose active
                       and inactive states are capture noise; -bbox prints where the
                       difference is, which usually names the widget; -tol sets what
                       counts as more than antialiasing. Identical renders are
                       identical files, so most pairs cost a byte compare.
                       It passes -gpui-window=X,Y,W,H, a runtime flag every example
                       understands: the window opens at that outer rect instead of
                       being moved into it, so the tree is laid out once. The runtime
                       takes its runtime flags, including Windows' __paint,
                       __msaa and __scene, out of argv before the example parses it.
cmd/compare-story.ts   screenshot a story page from the Rust app and this one
                       (rust left half, ours right half, both 80% work-area tall)
cmd/update-dist.ts     amalgamate src/** into gpui.h + gpui.cpp (`.work/` for
                       builds; run by hand to publish gpui-cpp-dist, which also
                       carries the examples, assets/, web/ and both scripts)
cmd/update-win-shaders.ts
                       compile paintgpu_win.hlsl's four SM5 entry points with
                       FXC and update the checked-in basE95 DXBC source
readme-dist.md         the published snapshot's readme, `<checkin-sha1>` filled
                       in on the way over; the dist copy is overwritten each run
cmd/test.ts            build tests/ and run it
tests/                 utassert ports of the pure-logic Rust tests
cmd/bench.ts           build bench/ and run it (-small, -large, -n=<count>)
bench/                 taffy's layout benchmarks, ported, and markdown parse
                       benchmarks of our own (`bun cmd/bench.ts markdown`)
cmd/vec-log.ts         run a target with the debug Vec/ArenaVec instrument on
                       and analyze the log: where the growth is, and what the
                       same run would have cost under another growth policy.
                       The instrument is in src/base.h behind #if DEBUG and
                       writes nothing unless this sets GPUI_VEC_LOG. Answer a
                       "should this capacity be bigger?" question with it
cmd/crlf-to-lf.ts      normalize line endings (run it after any scripted edit)
cmd/svg-to-bytecode.ts convert assets/icons into src/gpui/asset_icons.cpp
src/taffy/             the taffy layout crate, ported (see its readme.md)
src/markdown/          the markdown crate, ported (see its readme.md)
src/wry/               the wry webview crate, ported (see its readme.md);
                       WebView2 on Windows, WKWebView on macOS, a stub on
                       Linux and wasm
src/webview/           crates/webview (gpui-wry): the view that gives a
                       wry webview a box in the element tree
src/base.h/.cpp        vendored SumatraPDF subset
src/base_win.cpp       Windows platform layer (memory, paths, strings)
src/base_linux.cpp     the same, on POSIX
src/gpui/gpui.h        App, Window, Entity, Ctx, El, theme, paint
src/gpui/paint.h       the portable 2D canvas and shaped-text API
src/gpui/paint_win.cpp / paint_linux.cpp   its two backends
src/gpui/platform.h    the seam between window_common.cpp and the OS window
src/gpui/window_common.cpp   frame drawing, input dispatch, App lifecycle
src/gpui/window_win.cpp / window_linux.cpp  the two OS windows
src/gpui/entity.cpp    entity store, listeners, window subscriptions
src/gpui/drawops.h/.cpp  the icon byte code and the machine that draws it
src/gpui/svg.cpp       .svg -> that byte code, at load time
src/gpui/asset_icons.cpp  assets/icons as that byte code, generated
src/gpui/              layout, paint, assets, SVG, element tree
src/sys/               system metrics, the executor, the fetch table
                       (portable + one file per OS where the OS differs)
src/base/              crates/base unstyled primitives (Button, …)
src/ui/                themed crates/ui façade (component::Button, Func0/Func1 callbacks)
src/fps/               the crates/fps performance HUD (FpsMonitorEl)
examples/              AppLog.cpp (log hooks) + system_monitor, app_assets, showcase/, story/
assets/app_assets/     Lucide SVGs for the app_assets example
assets/icons/          Lucide SVGs; the source cmd/svg-to-bytecode.ts compiles in
assets/markdown_table/ report.md for the markdown_table example
```

## How to port a module

Port **gpui-base unstyled primitives** into `src/base/`, one Rust module at a time (`crates/base/src/<name>.rs`). Keep the type name (`Button`, `Checkbox`, `Accordion`, …).

These primitives own interaction (click, focus, open/checked state wiring). They do **not** own paint: the showcase (or a later themed façade) applies `.Bg()`, `.Border()`, `.H()`, `.Child()`, matching how Rust `Button::new(id).bg(...).child(...)` works.

```cpp
Button::New(cx, StrL("primary-button"), ClickSave)
    ->PadX(12)
    ->H(28)
    ->Bg(Rgb(0x17, 0x17, 0x17))
    ->Child(TextEl(cx->a, StrL("Save changes")));
```

Do not inline a styled `Div` tree in a showcase page when a primitive exists. `ButtonEl` in `src/gpui` is a _themed_ helper for older examples; new showcase pages use `src/base`.

When a primitive needs a GPUI capability we do not have (text input, overlay), add the smallest piece in `src/gpui` first, then the widget.

## Updating the vendored base

If `src/base.h` is missing an API you need, copy the corresponding bits from `C:\Users\kjk\src\sumatrapdf\src\base` into `src/base.h` / `src/base.cpp`. Provide `log` in `examples/AppLog.cpp` (linked into every example). Do not copy CrashHandler, GdiPlusUtil, Http, Zip, or other app-level Sumatra files — the HTTP client this tree does have is `src/sys/http.h`, written here against the OS's own library rather than copied from there.

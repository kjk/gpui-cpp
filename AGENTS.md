# gpui2 — C++ port of gpui-component

This repository is a C++ port of [longbridge/gpui-component](https://github.com/longbridge/gpui-component) targeting **Windows, Linux and macOS**. The goal is to port **as much of the Rust as this tree can hold**: every module of `crates/base` and `crates/ui`, the story gallery, the showcase and the examples. Assume a thing is in scope until a hard rule below says otherwise; the answer to "should we port X?" is yes unless X needs a dependency or a runtime we have ruled out.

The Rust sources live under `.work/gpui-component/` (gitignored clone). Do not treat that tree as something to compile into this binary. Read it as the specification. `bun cmd/build.ts` and `bun cmd/run.ts` clone that tree at the pinned SHA if it is missing.

**Upstream pins** — source of truth: [`cmd/versions.ts`](cmd/versions.ts) (`gpuiComponent`, `zedGpui`). How to ingest a later checkin: `port-upstream.md`.

## Goal

Port gpui-component to C++, module by module, and ship its examples as native
desktop apps. `system_monitor` was the first milestone and is done; the story
gallery and the showcase came after it, and the work now is depth — the parts
of a ported module that were left behind. `port-progress.md` is the running
log of what has been done and what a session found; read its tail before
picking up something new.

Fidelity is the bar. When a widget's look or numbers are in question, read the
Rust file under `.work/gpui-component/` at the SHA in `cmd/versions.ts` and
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
rather than port them. **Taffy is the exception**: `src/taffy/` is a full C++
port of the taffy crate at the version gpui-component pins, and every box in
this tree is laid out by it. See `src/taffy/readme.md`.

## Non-goals

These are the standing exclusions. Everything else in gpui-component is in
scope, and a module being large or unglamorous is not a reason to skip it.

- WASM
- The full GPUI GPU scene graph or async executor. We do have `App`/`Window`/
  `Entity`/`Ctx`, actions and a keymap, `EventEmitter` and window
  subscriptions — see below — but not refcounted entities, observers, or
  `Task`
- STL containers (`std::string`, `std::vector`, `std::map`, iostreams, `std::function` as the default callback style)
- Reusing `../gpui/` — that experiment uses STL heavily and is not the base for this port
- Anything that needs a third-party C++ library, by hard rule 3: tree-sitter
  and syntect (so `highlighter` stays the small hand-written lexer it is), a
  webview, an LSP client, resvg, ropey, html5ever. Where Rust reaches for one
  of these and the feature is still worth having, write the small version this
  tree needs — `ext/md4c` is the one exception, and `port-upstream.md` lists
  the rest

A thing that is *not* ported for a reason other than these belongs in
`port-progress.md` with the reason, so the next session does not have to
rediscover it.

## Hard rules

1. **No STL data structures.** C headers and the C++ headers SumatraPDF already uses (`cstdint`, `cstring`, `new`, `algorithm` for `std::min`/`std::max`, `utility`) are allowed. Do not introduce `std::string`, `std::vector`, `std::unique_ptr`, `std::optional`, `std::function`, `std::unordered_map`.
2. **Use SumatraPDF base types.** `Str`, `Vec<T>`, `Arena`, `StrBuilder`, `fmt()`, `uint8_t`/`int32_t`/`uint32_t`/`int64_t`/`uint64_t`, `Func0`/`Func1`. Source of truth: `C:\Users\kjk\src\sumatrapdf\src\base`. A curated copy lives in `src/base.h` / `src/base.cpp` so this tree builds without that checkout. All of `src/` lives in `namespace gpui` (themed widgets in `gpui::component`). Examples `#include "gpui.h"` and `using namespace gpui;`.
3. **Three platforms, no third-party C++ libraries.** Windows: MSVC `cl.exe` on PATH, static CRT (`/MT` / `/MTd`) — no VC++ redistributable DLLs. Linux: g++ or clang++ with the system X11, cairo and Pango, found through `pkg-config`. macOS: clang++ with Cocoa, Core Graphics, Core Text and IOKit from the system SDK. `bun cmd/build.ts` picks the toolchain by host. Do not add CMake, vcpkg, or a C++ package manager. The one vendored library is `ext/md4c` — the CommonMark parser behind `component::TextView`, a single C file with no dependencies, checked in with its version and refresh commands in `ext/md4c/readme.md` and amalgamated into `gpui.h` / `gpui.cpp` along with `src/**`. Adding a second one needs the same bar: no build system of its own, no transitive dependencies, and a reason the tree cannot write it itself.
4. **POD-friendly C++.** Prefer structs with explicit ownership. `Vec<T>` is memcpy/POD only. Heap strings are `Str` owned by `StrDup` / `StrFree` or an `Arena`. Frame UI trees allocate from a per-frame `Arena` and are discarded, not destructed as a graph of C++ objects.
5. **No exceptions, no RTTI needed.** COM (`Direct2D` / `DirectWrite`) uses HRESULT checks, not C++ exceptions.
6. **When unsure about a widget's look or numbers, read the Rust file** under `.work/gpui-component/` (the SHA in `cmd/versions.ts`) and copy constants (heights, gaps, colors, column widths). Do not invent a different design system.
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
src/ui/     Theme, TitleBar, TabBar, AreaChart, Progress, Icon, Table, Root
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
src/sys/    process/CPU/memory/disk/battery, per OS
        │
        ▼
src/base.h  Str, Vec, Arena, Geom, Color helpers
```

## Portability

`src/base.h` defines `GPUI_OS_WINDOWS`, `GPUI_OS_LINUX` and `GPUI_OS_MAC` from
the compiler's own predefines; exactly one is 1. They exist for one-expression differences
(the path separator, `SRWLOCK` vs `pthread_mutex_t`). Everything bigger is a
portable signature plus one implementation per platform:

| Seam                                       | Shared header          | Windows                   | Linux                       | macOS                     |
| ------------------------------------------ | ---------------------- | ------------------------- | --------------------------- | ------------------------- |
| virtual memory, paths, strings, self usage | `src/base.h` (`Plat*`) | `src/base_win.cpp`        | `src/base_linux.cpp`        | `src/base_mac.cpp`        |
| 2D drawing and shaped text                 | `src/gpui/paint.h`     | `src/gpui/paint_win.cpp`  | `src/gpui/paint_linux.cpp`  | `src/gpui/paint_mac.cpp`  |
| the OS window and its event loop           | `src/gpui/platform.h`  | `src/gpui/window_win.cpp` | `src/gpui/window_linux.cpp` | `src/gpui/window_mac.cpp` |
| system metrics                             | `src/sys/sysinfo.h`    | `src/sys/sysinfo_win.cpp` | `src/sys/sysinfo_linux.cpp` | `src/sys/sysinfo_mac.cpp` |

`src/gpui/window_common.cpp` holds everything a window does that is not the OS
window — frame drawing, input dispatch, the app lifecycle — and all platform
files call into it.

An example never names an OS API. It implements `int GpuiMain(int argc,
char** argv)`; the runtime provides `wWinMain` / `main`. Key codes are the
`Key*` constants in `Gpui.h` (the Win32 `VK_*` values, which the X11 window
maps keysyms onto), and the clipboard is `ClipboardSetText`.

`cmd/build-dist.ts` amalgamates `src/` plus `ext/md4c` into two files:
`gpui.h` and `gpui.cpp`. Both are the same on every platform. `.work/` is gitignored and is what
every build compiles — `bun cmd/build.ts`, `cmd/test.ts` and CI all go through
it. The published copy is a repo of its own,
[gpui-cpp-dist](https://github.com/kjk/gpui-cpp-dist), cloned to
`.work/gpui-cpp-dist` and refreshed only by running `bun cmd/build-dist.ts` by
hand: that syncs the clone, writes the pair into it, builds every example
against it (`GPUI_AMALGAM_DIR` points the platform build at that copy, and its
objects go to their own `out/*_dist` tree), rewrites its readme with the
gpui-cpp commit it came from and a compare link showing what it is behind by,
then commits and pushes it. Never regenerate a published copy as part of a
build, a test run or a commit; `buildDist()` takes a required `outDir` so an
automatic caller has to say `.work` out loud. The two differ in what they do
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
the mac half is. md4c is the tail of both outputs: `md4c.h` at the end of
`gpui.h`, `md4c.c` at the end of `gpui.cpp`.

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
| `cx.notify()`                    | `Notify(cx)`                                                                        |
| `Drop for T`                     | `~T()`, run when the entity is dropped                                              |
| `window.use_keyed_state`         | `KeyedState<T>(cx, key)`                                                            |

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
6. Window-level input is a subscription bound to a view, one per event type the way `window.on_mouse_event::<T>` is: `WindowOnKey`, `WindowOnMouseDown` / `Up` / `Move` / `Exit`, `WindowOnScrollWheel`, `WindowSetInterval` / `WindowSetTimeout` (GPUI spells the last two `cx.spawn` + `Timer::after`). The handler takes the matching GPUI event — `MouseDownEvent`, `ScrollWheelEvent` — and the platform window builds those into a `PlatformInput` for `WindowDispatchInput`, which is Rust's `Window::dispatch_event`. Any number of timers can be armed; each returns a handle for `WindowCancelTimer`, and one whose view goes stale is dropped the way Rust drops a `Task` with its entity.
7. A blinking caret is state, not a function of the clock: `BlinkStart` / `Stop` / `Pause` / `Visible`, a port of `crates/base/src/input/base/blink_cursor.rs`. Sampling `TimeNow()` at paint time instead makes the caret invisible whenever nothing happens to repaint during the lit half. One cursor per field, as an entity — `InputState::blink` is Rust's `InputState::blink_cursor`. `InputFocus` / `InputBlur` start and stop it, the way Rust's `on_focus` / `on_blur` do, and the runtime does the same handoff for whichever `InputState` a view points `win->input` at, so only a custom text widget has to do it itself. `AppRequestAnim` is for real animation (the FPS HUD), never for a caret.
8. `Notify(cx)` schedules a repaint. The frame tree is rebuilt from scratch every paint, so it is coarser than GPUI's per-observer invalidation — the API matches, the machinery does not.
9. Entity handles are generational, not refcounted. `Entity<T>::Get` returns null once the slot is recycled; check it.
10. **Geometry is `Point` / `Size` / `Bounds` / `Edges`, all DIP floats, named the way Rust names them.** Rust's `Point<T>` and friends are generic over a *unit* — `Pixels`, `ScaledPixels`, `DevicePixels` — not over an element type, and everything above `Paint.h` here is DIPs, so the parameter is gone and the arithmetic is written out once. Use them for what is produced, returned or passed whole: a measured `Size`, a hit box, the positioner's arguments. Code that writes one component at a time — the layout pass over `El`, a mouse event's position — keeps flat fields. A unit that is not DIPs gets a named struct of its own (`WinSize` carries both the DIP and the device-pixel size of a window), never a template.

`AppNew` → `WindowOpenView` → `AppRun` → `AppFree` is the whole lifecycle; `AppRunView` is the one-window shorthand. There is no hook table.

## Layout

`src/taffy/` is a C++ port of [taffy](https://github.com/DioxusLabs/taffy)
0.12.2 — the crate Zed's GPUI lays out with, and therefore the one that defines
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

When a layout question comes up, the answer is in `src/taffy/`, and behind that
in the Rust crate. Do not add a special case to `LayoutEl` for something CSS
already has a rule for; make the style translation say the right thing instead.

**The port is kept current.** When `cmd/versions.ts` moves to a gpui-component
whose `Cargo.lock` resolves a different taffy, the port moves with it — bump
`taffy.version` there and diff the crate. `src/taffy/readme.md` has the
file-for-file map and `port-upstream.md` has the procedure.

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
bun cmd/run.ts -rel -windbg showcase
```

`cmd/build.ts` and `cmd/run.ts` are dispatchers: they forward every flag to
`cmd/build-windows.ts` / `cmd/run-windows.ts` on Windows,
`cmd/build-linux.ts` / `cmd/run-linux.ts` on Linux, or
`cmd/build-mac.ts` / `cmd/run-mac.ts` on macOS. Use those names directly only
when you mean one specific toolchain.

No example name (or a flag last) prints the valid example list. The example is the last argument.

Debug: `bun cmd/build.ts -dbg system_monitor` (writes `out/dbg/` on Windows). Release+ASan: `bun cmd/build.ts -rel -asan system_monitor` (`out/rel_asan/` on Windows). Clean rebuild of that dir: add `-clean`. Linux and macOS write under `out/linux/` and `out/mac/`, so building the same checkout for multiple platforms never clobbers another platform's output.

`bun cmd/run.ts` takes the same flags as `build.ts`, plus `-windbg` on Windows (launch under `windbgx.exe`), `-gdb` on Linux, and `-lldb` on macOS. It does not accept `-all` — pick one binary.

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

The Rust reference (optional, slow first build because it pulls Zed). `bun cmd/build.ts` / `bun cmd/versions.ts` installs `.work/gpui-component` at the SHA in `cmd/versions.ts`:

```
bun cmd/versions.ts
cd .work\gpui-component
cargo run -p system_monitor
```

## Layout of this tree

```
AGENTS.md              this file
port.md                phased porting plan
port-progress.md       what is done / what is next
port-upstream.md       how to ingest later checkins (pins live in cmd/versions.ts)
cmd/versions.ts        exact gpui-component + zed gpui SHAs and the taffy
                       version we are porting
cmd/format.ts          clang-format src/**/*.{cpp,h} + examples/ and prettier cmd/*.ts (`-ts` / `-cpp` to run one)
cmd/build.ts           dispatches to build-windows.ts / build-linux.ts by host
cmd/build-windows.ts   MSVC compile/link via bun; also clones the pinned Rust spec
cmd/build-linux.ts     g++/clang++ compile/link, X11 + cairo + pango via pkg-config
cmd/run.ts             dispatches to run-windows.ts / run-linux.ts by host
cmd/run-windows.ts     build then run; same flags as build.ts plus -windbg / -compare
cmd/run-linux.ts       build then run; same flags plus -gdb / -compare
cmd/wsl-run.ts         run cmd/run-linux.ts inside WSL from a Windows checkout
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
                       It passes -gpui-window=X,Y,W,H, a runtime flag every example
                       understands: the window opens at that outer rect instead of
                       being moved into it, so the tree is laid out once. The runtime
                       takes -gpui-* out of argv before the example parses it.
cmd/compare-story.ts   screenshot a story page from the Rust app and this one
                       (rust left half, ours right half, both 80% work-area tall)
cmd/build-dist.ts      amalgamate src/** + ext/md4c into gpui.h + gpui.cpp
                       (`.work/` for builds; run by hand to publish gpui-cpp-dist)
cmd/test.ts            build tests/ and run it
tests/                 utassert ports of the pure-logic Rust tests
cmd/crlf-to-lf.ts      normalize line endings (run it after any scripted edit)
src/taffy/             the taffy layout crate, ported (see its readme.md)
src/base.h/.cpp        vendored SumatraPDF subset
src/base_win.cpp       Windows platform layer (memory, paths, strings)
src/base_linux.cpp     the same, on POSIX
src/gpui/gpui.h        App, Window, Entity, Ctx, El, theme, paint
src/taffy/taffy.md     see src/taffy/readme.md — the ported layout crate
src/gpui/paint.h       the portable 2D canvas and shaped-text API
src/gpui/paint_win.cpp / paint_linux.cpp   its two backends
src/gpui/platform.h    the seam between window_common.cpp and the OS window
src/gpui/window_common.cpp   frame drawing, input dispatch, App lifecycle
src/gpui/window_win.cpp / window_linux.cpp  the two OS windows
src/gpui/entity.cpp    entity store, listeners, window subscriptions
src/gpui/              layout, paint, assets, SVG, element tree
src/sys/               system metrics, portable + one file per OS
src/base/              crates/base unstyled primitives (Button, …)
src/ui/                themed crates/ui façade (component::Button, Func0/Func1 callbacks)
examples/              AppLog.cpp (log hooks) + system_monitor, app_assets, showcase/, story/
assets/app_assets/     Lucide SVGs for the app_assets example
assets/icons/          Lucide SVGs for sidebar
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

If `src/base.h` is missing an API you need, copy the corresponding bits from `C:\Users\kjk\src\sumatrapdf\src\base` into `src/base.h` / `src/base.cpp`. Provide `log` in `examples/AppLog.cpp` (linked into every example). Do not copy CrashHandler, GdiPlusUtil, Http, Zip, or other app-level Sumatra files.

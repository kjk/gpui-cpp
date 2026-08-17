# gpui2 — C++ / Windows port of gpui-component

This repository is a C++ port of [longbridge/gpui-component](https://github.com/longbridge/gpui-component) targeting **Windows only**. The north-star deliverable is the `system_monitor` example: a dark-theme desktop window with live CPU/memory area charts, a sortable process table, a custom title bar with segmented tabs, and a status bar.

The Rust sources live under `.work/gpui-component/` (gitignored clone). Do not treat that tree as something to compile into this binary. Read it as the specification. `bun cmd/build.ts` and `bun cmd/run.ts` clone that tree at the pinned SHA if it is missing.

**Upstream pins** — source of truth: [`cmd/versions.ts`](cmd/versions.ts) (`gpuiComponent`, `zedGpui`). How to ingest a later checkin: `port-upstream.md`.

## Goal

Ship the gpui-component examples as C++ Windows apps, starting with `system_monitor` and continuing one example at a time.

```
bun cmd/build.ts system_monitor
bun cmd/build.ts -rel hello_world
bun cmd/build.ts -rel showcase
bun cmd/build.ts -dbg all
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

It does **not** mean a line-for-line clone of Zed's GPUI renderer, Taffy, Blade, or the unused 60+ gpui-component widgets.

## Non-goals (until system_monitor is done)

- macOS / Linux / WASM
- The full GPUI GPU scene graph, entity system, or async executor
- The unused story-gallery widgets (editor, tree-sitter, webview, date picker, …)
- STL containers (`std::string`, `std::vector`, `std::map`, iostreams, `std::function` as the default callback style)
- Reusing `../gpui/` — that experiment uses STL heavily and is not the base for this port

## Hard rules

1. **No STL data structures.** C headers and the C++ headers SumatraPDF already uses (`cstdint`, `cstring`, `new`, `algorithm` for `std::min`/`std::max`, `utility`) are allowed. Do not introduce `std::string`, `std::vector`, `std::unique_ptr`, `std::optional`, `std::function`, `std::unordered_map`.
2. **Use SumatraPDF base types.** `Str`, `WStr`, `Vec<T>`, `Arena`, `str::Builder`, `fmt()`, `i32`/`u32`/`u64`, `Func0`/`Func1`. Source of truth: `C:\Users\kjk\src\sumatrapdf\src\base`. A curated copy lives in `src/Base.h` / `src/Base.cpp` so this tree builds without that checkout.
3. **Windows + MSVC.** `cl.exe` is on PATH. Build with `bun cmd/build.ts`. Static CRT (`/MT` / `/MTd`) — no VC++ redistributable DLLs. Do not add CMake, vcpkg, or extra third-party C++ libraries.
4. **POD-friendly C++.** Prefer structs with explicit ownership. `Vec<T>` is memcpy/POD only. Heap strings are `Str` owned by `str::Dup` / `str::Free` or an `Arena`. Frame UI trees allocate from a per-frame `Arena` and are discarded, not destructed as a graph of C++ objects.
5. **No exceptions, no RTTI needed.** COM (`Direct2D` / `DirectWrite`) uses HRESULT checks, not C++ exceptions.
6. **When unsure about a widget's look or numbers, read the Rust file** under `.work/gpui-component/` (the SHA in `cmd/versions.ts`) and copy constants (heights, gaps, colors, column widths). Do not invent a different design system.

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
gpui + gpui_platform   (Zed: window, flex layout via Taffy, GPU scene,
                        Entity/Context, timers, input)
        │
        ▼
sysinfo + battery      (process/CPU/mem/disk + battery)
```

C++ stack we implement:

```
examples/system_monitor.cpp
        │
        ▼
src/ui/     Theme, TitleBar, TabBar, AreaChart, Progress, Icon, Table, Root
        │
        ▼
src/gpui/   Win32 window, flex layout, Direct2D/DirectWrite paint,
            hit-test, timer, frame arena element tree
        │
        ▼
src/sys/    Win32 process/CPU/memory/disk/battery
        │
        ▼
src/Base.h  Str, Vec, Arena, Geom, Color helpers
```

## Source of truth for visuals

Dark theme from `crates/ui/src/theme/default-theme.json` ("Default Dark") resolved against `default-colors.json`:

| Token | Hex |
| --- | --- |
| background | `#0a0a0a` (neutral-950) |
| foreground | `#fafafa` (neutral-50) |
| border | `#262626` (neutral-800) |
| muted.foreground | `#a3a3a3` (neutral-400) |
| title_bar / tab_bar / status_bar | `#171717` |
| title_bar.border / window.border | `#262626` |
| tab.active.background | `#0a0a0a` |
| tab.active.foreground | `#fafafa` |
| tab.foreground | `#d4d4d4` |
| table.background | `#0a0a0a` |
| table.head.foreground | `#525252` |
| table.row.border | `#262626` @ ~70% |
| table even row | `#171717` @ 40% |
| progress_bar | `#f5f5f5` |
| base.red | `#f87171` (red-400) |
| base.green | `#4ade80` (green-400) |
| base.blue | `#60a5fa` (blue-400) |
| base.yellow | `#facc15` (yellow-400) |
| danger (close hover) | `#f87171` |
| secondary.hover | `#292929` |
| secondary.active | `#212121` |

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

## Code style (match SumatraPDF `src/base`)

```cpp
#include "Base.h"

struct MetricPoint {
    float cpu = 0;
    float memory = 0;
};

void FormatBytes(u64 bytes, str::Builder& out);
```

- Include `"Base.h"` first. It pulls Windows headers, `Str`, `Vec`, `Arena`, `Geom`.
- `Str s = fmt("%.1f%%", cpu);` for formatting (temp-arena string; do not `free` it).
- Own a heap `Str` only if it must survive a frame: `str::Dup` / `str::Free`.
- `Vec<T>` for arrays of POD. Not for `Str` graphs — use `Vec<ProcessInfo>` where `ProcessInfo` holds a `char name[kMax]` or an arena `Str`.
- `logf("...")` for debug prints.
- Prefer `i32` indexes. `int` is fine when matching existing base APIs (`Vec::len` is `int`).
- COM interfaces: pair every successful `Create` with `Release`. No `CComPtr`.

## Build

```
bun cmd/build.ts
bun cmd/build.ts -rel system_monitor
bun cmd/run.ts
bun cmd/run.ts -dbg hello_world
bun cmd/run.ts -rel -windbg showcase
```

No example name (or a flag last) prints the valid example list. The example is the last argument.

Debug: `bun cmd/build.ts -dbg system_monitor` (writes `out/dbg/`). Release+ASan: `bun cmd/build.ts -rel -asan system_monitor` (`out/rel_asan/`). Clean rebuild of that dir: add `-clean`.

`bun cmd/run.ts` takes the same flags as `build.ts`, plus `-windbg` (launch under `windbgx.exe -g -G`). It does not accept `all` — pick one binary.

After changing `.cpp` / `.h` / `.ts` files, run `bun cmd/format.ts` on those paths (or with no args for `src/` + `cmd/*.ts`) before finishing. It runs clang-format on C++ (`/.clang-format`, Chromium-based, 80 columns) and Prettier on TypeScript (`.prettierrc.json`: `printWidth` 120, `endOfLine` lf). Use `-ts` or `-cpp` to run only Prettier or only clang-format. Add `-with-examples` to also clang-format `examples/`. Do not format `.work/` or `out/`. `.gitattributes` forces `eol=lf`.

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
cmd/versions.ts        exact gpui-component + zed gpui SHAs we are porting
cmd/format.ts          clang-format src/**/*.{cpp,h} and prettier cmd/*.ts (`-ts` / `-cpp` / `-with-examples`)
cmd/build.ts           MSVC compile/link via bun; also clones the pinned Rust spec
cmd/run.ts             build then run; same flags as build.ts plus -windbg / -compare
src/Base.h/.cpp        vendored SumatraPDF subset
src/gpui/              window, layout, paint, assets, SVG, element tree
src/sys/               Windows system metrics
src/ui/                gpui-base unstyled primitives (Button, …)
src/component/         themed crates/ui façade (component::Button, Func0/Func1 callbacks)
examples/              AppLog.cpp (log hooks) + system_monitor, app_assets, showcase/, story/
assets/app_assets/     Lucide SVGs for the app_assets example
assets/icons/          Lucide SVGs for sidebar
assets/markdown_table/ report.md for the markdown_table example
```

## How to extend after system_monitor

Port **gpui-base unstyled primitives** into `src/ui/`, one Rust module at a time (`crates/base/src/<name>.rs`). Keep the type name (`Button`, `Checkbox`, `Accordion`, …).

These primitives own interaction (click, focus, open/checked state wiring). They do **not** own paint: the showcase (or a later themed façade) applies `.Bg()`, `.Border()`, `.H()`, `.Child()`, matching how Rust `Button::new(id).bg(...).child(...)` works.

```cpp
Button::New(a, StrL("primary-button"), ClickSave)
    ->PadX(12)
    ->H(28)
    ->Bg(Rgb(0x17, 0x17, 0x17))
    ->Child(TextEl(a, StrL("Save changes")));
```

Do not inline a styled `Div` tree in a showcase page when a primitive exists. `ButtonEl` in `src/gpui` is a *themed* helper for older examples; new showcase pages use `src/ui`.

When a primitive needs a GPUI capability we do not have (text input, overlay), add the smallest piece in `src/gpui` first, then the widget.

## Updating the vendored base

If `src/Base.h` is missing an API you need, copy the corresponding bits from `C:\Users\kjk\src\sumatrapdf\src\base` into `src/Base.h` / `src/Base.cpp`. Provide `log` / `loga` in `examples/AppLog.cpp` (linked into every example). Do not copy CrashHandler, GdiPlusUtil, Http, Zip, or other app-level Sumatra files.

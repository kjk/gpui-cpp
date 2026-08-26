# Porting plan: gpui-component → C++ / Windows

## 1. Why this plan is a subset

[gpui-component](https://github.com/longbridge/gpui-component) is 60+ widgets on top of Zed's GPUI. GPUI is a GPU scene graph + Taffy flex/grid + entity/observer runtime + Metal/Vulkan/DX backends. A faithful engine port is a multi-year project.

`system_monitor` only needs:

| Layer | Rust | C++ we build |
| --- | --- | --- |
| App | `examples/system_monitor/src/main.rs` | `examples/system_monitor.cpp` |
| Components | Theme, Root, TitleBar, TabBar(segmented), Tab, AreaChart, Progress, Icon, DataTable | `src/ui/*` |
| Runtime | gpui window, flex `div`/`h_flex`/`v_flex`, text, gradient, path, timer, input | `src/gpui/*` (Win32 + Direct2D + DirectWrite) + `src/taffy/*` (taffy, ported) |
| Metrics | `sysinfo`, `battery` | `src/sys/*` (Win32) |
| Strings/arrays | `String`, `Vec` | `Str`, `Vec` from SumatraPDF |

An earlier STL-based sketch exists in `../gpui/`. Ignore it for implementation. Type names there can be used as a glossary for GPUI vocabulary only.

## 2. Rust dependency graph (what we are *not* porting)

Workspace crates: `gpui-component` (`crates/ui`), `gpui-base`, `gpui-component-assets`, `gpui-component-macros`, `story`, `fps`, `webview`.

Pinned GPUI from `zed-industries/zed` (`gpui`, `gpui_platform`, `gpui_macros`). That pull brings Taffy, Blade, font-kit, cosmic-text/rustybuzz, and a huge Windows crate surface. Exact SHAs: [`cmd/run.ts`](cmd/run.ts).

Taffy is the one of those we **do** port: `src/taffy/` is a C++ port of taffy
0.12.2, the version that `Cargo.lock` resolves for `gpui`. See §4.3.

`system_monitor` extra crates: `sysinfo 0.37`, `battery 0.7`, `smol` (500 ms timer). The `windows` GPU features in its `Cargo.toml` are unused by the example source.

`gpui-component` also depends on ropey, tree-sitter, markdown, html5ever, chrono, lsp-types, resvg (Windows native menu). None of that is on the system_monitor path.

## 3. system_monitor surface (authoritative)

From `.work/gpui-component/examples/system_monitor/src/main.rs`:

**Window**

- `TitleBar::window_options()` — transparent / client-drawn title bar
- Bounds `680×600`, centered
- Title `"System Monitor"`
- `ThemeMode::Dark`
- Quit: Alt+F4

**Title bar**

- Height 34, gradient mix of title_bar and background
- Left: segmented `TabBar` with "System" / "Processes"
- Right: total RAM as `"X.X GB"`
- Windows caption buttons: minimize, maximize/restore, close (34×34 hit targets)
- Drag region; double-click maximize

**System tab**

- Two `AreaChart`s stacked (`flex_1`, `min_h 160`, `gap_4`, `p_3`)
- Header row: title + current `"X.X%"`
- CPU = theme red, Memory = theme blue
- Fill: vertical linear gradient, series color @ 0.4 → background @ 0.1
- X labels every 15 samples, dashed horizontal grid (4 lines), 18 px axis gutter

**Processes tab**

- `DataTable`, not bordered, striped, small
- Columns: PID, Name, CPU %, Memory — all sortable
- Default sort CPU descending
- Name truncated; CPU color by threshold (>50 red, >20 yellow, else blue); memory green, human bytes
- Top 200 after sort

**Status bar**

- Height 28, top border, tab_bar background
- Left: disk / memory / CPU chips (icon + 48×8 progress + percent), width 135 each
- Right: battery icon + percent (if a battery exists)

**Loop**

- `sys.refresh_all()` + disks + battery every 500 ms
- Ring buffer of 120 `MetricPoint`s

## 4. C++ architecture

### 4.1 Frame loop

```
WM_TIMER 500ms → SysRefresh() → InvalidateRect
WM_PAINT / D2D →
    ArenaReset(frame)
    El* tree = AppRender(frame)
    Layout(tree, windowSize)
    Paint(tree, d2d)
    HitTest cache for next input
```

The tree is rebuilt every frame from app state (GPUI `Render` / `RenderOnce`). No retained widget objects except app state and table sort state.

### 4.2 Element (`src/gpui/el.h`)

Arena-allocated node, sibling linked list (not `Vec<El*>` — `Vec` would heap-allocate per node):

- `kind`: Div, Text, Chart, Progress, Icon, Hit
- `style`: flex direction, grow/shrink, sizes, min/max, pad/margin, gap, align, justify, overflow, bg, border, radius, font size, color, truncate
- `text`, `id`
- `clickId` + `user` for hit-testing
- layout output: `x,y,w,h`

Fluent methods return `El*` so call sites read like the Rust builder.

### 4.3 Layout

`src/taffy/` — a C++ port of [taffy](https://github.com/DioxusLabs/taffy)
0.12.2, the layout crate Zed's GPUI itself uses, so layout here means what
layout means in gpui-component. This started as a hand-written flex subset and
was replaced once the subset stopped being able to answer the questions the
widgets were asking of it.

What the port covers: flexbox, CSS Grid, block layout with margin collapsing,
floats, `calc()` handles, content sizes and the per-node layout cache. What it
does not: the crate's `parse` and `serde` features, and
`detailed_layout_info`. `src/taffy/readme.md` is the file-for-file map, the
list of deliberate differences, and the refresh procedure.

`LayoutEl` in `src/gpui/gpui.cpp` is the seam between the `El` tree and the
taffy tree: it translates `gpui::Style` into `taffy::Style`, runs the layout,
and writes the boxes back. Text, icons, images and progress bars reach taffy as
measured leaves. The handful of gpui-component positioning rules CSS has no
word for — `fixed` against the window, an overlay anchored under its trigger, a
`relative(f)` inset — are applied around it.

### 4.4 Paint

Direct2D + DirectWrite (GPU, no extra deps):

- filled / stroked rounded rects
- clip
- linear gradient (title bar, chart fill)
- path (area series + stroke)
- dashed lines (grid)
- text
- simple lucide-like icon paths (cpu, memory, disk, battery, window chrome)

GDI+ is pulled in only because `Base.h` includes it. Do not paint the UI with GDI+.

### 4.5 Window

Win32 + DWM:

- `WS_OVERLAPPEDWINDOW` without the default caption (handle `WM_NCCALCSIZE`)
- `DwmExtendFrameIntoClientArea` so Win11 rounded corners stay
- Caption buttons are our hit targets (or `WM_NCHITTEST` HTCLOSE / HTMINBUTTON / HTMAXBUTTON)
- Title-bar drag = `HTCAPTION`
- Alt+F4 / `WM_CLOSE` quits

### 4.6 System metrics (`src/sys`)

| Metric | API |
| --- | --- |
| CPU total | `GetSystemTimes` delta |
| Memory | `GlobalMemoryStatusEx` |
| Processes | `CreateToolhelp32Snapshot` + `GetProcessMemoryInfo` + `GetProcessTimes` |
| Per-process CPU | kernel+user delta / (elapsed × ncpu) |
| Disks | `GetLogicalDriveStringsW` + `GetDiskFreeSpaceExW` (first fixed drive) |
| Battery | `GetSystemPowerStatus` (`ACLineStatus`, `BatteryLifePercent`) |

First CPU sample is 0 (same as sysinfo). Keep previous process times in a `Vec<ProcSample>` keyed by pid.

### 4.7 Components (`src/ui/`)

Implement only what the example calls, with Rust names:

- `ThemeDark()` — the table in AGENTS.md
- `TitleBar(...)`
- `TabBar` segmented + `Tab`
- `AreaChart(points, ySelector, color)`
- `Progress(id, value)`
- `Icon(IconName)`
- `DataTable` + `Column` + sort
- `Root` — just a full-size background wrapper (no dialog/sheet/toast)

## 5. Phases and commits

| Phase | Commit | Done when |
| --- | --- | --- |
| 0 | docs: AGENTS.md, port.md, port-progress.md | Plan exists |
| 1 | vendor `src/base.h` + `cmd/build.ts` | A console or empty Win32 exe links Str/Vec |
| 2 | D2D window + dark fill + title text | Empty 680×600 dark window, custom chrome |
| 3 | flex layout + text + TitleBar + tabs | Can switch System/Processes |
| 4 | AreaChart | Two live-looking (or dummy) charts |
| 5 | SysInfo + 500 ms loop | Charts show real CPU/memory |
| 6 | Table + sort + scroll | Process list matches Rust columns |
| 7 | Status bar + icons + battery | Full chrome |
| 8 | Visual pass vs Rust | Spacing, colors, fonts, caption buttons |

Each phase updates `port-progress.md` and is committed.

## 6. Verification

1. `bun cmd/build.ts` succeeds with `cl.exe`.
2. `out\rel\system_monitor.exe` starts, paints, updates every 500 ms.
3. Side-by-side with `cargo run -p system_monitor` when the Rust tree has been built.
4. Click tabs, sort columns, scroll the table, min/max/close, Alt+F4.
5. If Rust is not built yet, verify against the constants in this file and screenshots in `port-progress.md`.

## 7. Risks

- **Flex edge cases.** Charts must share leftover height (`flex_1` + `min_h 160`). Test at 600 px and after maximize.
- **Process CPU %.** Need two samples; values will not be bit-identical to sysinfo but should be in the same ballpark.
- **Name encoding.** Process names are UTF-16 from the Toolhelp API; convert with `strconv` / `WideCharToMultiByte` into `Str`.
- **Direct2D resize.** Recreate the HWND render target on `WM_SIZE`.
- **Base.h GDI+ include.** Link `gdiplus.lib` even though we do not paint with it.

## 8. After system_monitor

### app_assets (ported)

Rust: `.work/gpui-component/examples/app_assets` — rust-embed `Assets` implementing GPUI `AssetSource`, then `IconName::Inbox` and `IconName::Bot` centered in a light `v_flex`.

C++:

- `src/gpui/assets.cpp` — search roots for `icons/<name>.svg` (cwd, exe dir, parents, `assets/<example>`, rust `examples/<example>/assets`)
- `src/gpui/svg.cpp` — Lucide subset (path including arcs, rect, polyline, line, circle, polygon) stroked with `currentColor`
- `IconNamePath` maps `Inbox` → `icons/inbox.svg` (same as `icon_named!`)
- `ThemeSet(Light)` — rust `gpui_component::init` defaults to light
- Window `800×600`, title `App Assets`
- Assets live in `assets/app_assets/icons/` and are copied next to the exe (`out/rel/assets`, …) by `cmd/build.ts`

### Simple examples (ported 2026-08-16)

Each has a matching `examples/<name>.cpp` and `bun cmd/build.ts <name>` target:

| Example | What landed |
| --- | --- |
| `hello_world` | Primary Button, hover, light theme |
| `window_title` | In-client 34 px title strip + Hello World body |
| `root_borderless` | Documents `Root::bordered(false)`; Win32 chrome kept |
| `tooltip_top_edge` | Absolute top-edge trigger; tooltip flips below |
| `input` | `InputState` + `WM_CHAR`; `Hello, {name}!` |
| `focus_trap` | Two Tab traps plus buttons outside |
| `dialog_overlay` | Center dialog, bottom sheet, right-click menu |
| `sidebar` | Collapsible icon/offcanvas/none + Lucide nav |
| `table_in_scrollable` | Nested scroll; inner table uses a y-band heuristic |
| `text_selection` | Selectable text block |
| `markdown_table` | Heading / hr / paragraph / pipe-table parser + `report.md` |
| `fps_monitor` | Hilbert + Catmull-Rom + HSL `customPaint`, 16 ms timer |

### gpui-base showcase (ported 2026-08-16)

Rust: `.work/gpui-component/crates/base/examples/showcase` via `cargo run -p gpui-base --example components -- [slug]`.

C++: `examples/showcase/` — `bun cmd/build.ts showcase`, window 840×640, light `#ffffff` / `#171717`. No slug opens the 3-column overview; `showcase.exe button` jumps to that page without the back bar.

Each component is a `SHOWCASE_PAGE` translation unit committed on its own. Shared helpers live in `showcase.cpp` / `Showcase.h`.

### gpui-component story gallery (ported 2026-08-17)

Rust: `.work/gpui-component/crates/story` via `cargo run -p gpui-component-story -- [slug]`.

C++: `examples/story/` — `bun cmd/build.ts story`, window 1280×800, light theme, sidebar + 62 stories matching the Rust gallery list.

### App / Window / Entity / Ctx (ported 2026-08-18)

The runtime originally had one fused `AppHost`: D2D factories, the font cache and the message loop sat next to `hwnd`, hover, focus and the frame arena, and app state lived behind a `void* user` with dispatch through a nine-entry `AppHooks` table and integer click ids.

It now follows GPUI's split. `App` owns the factories, shared fonts, window list and entity store; `Window` owns its render target, frame arena, input state and root view. A view is a struct with state, a `static El* Render(T*, Ctx*)` and static handlers bound with `Listen(cx, &T::Handler)`. `Ctx` carries `{app, win, a, self}` — GPUI splits that into `&mut App` / `&mut Window` / `&mut Context<T>` only because of the borrow checker.

Entity handles are generational rather than refcounted: `App` owns the state, `Entity<T>` is POD, and a stale handle reads back null instead of dangling. Dispatch resolves the handle and drops the event if the view is gone, which is what `cx.listener` does with its weak entity.

Landed in order, each step building all 16 examples:

1. Split `AppHost` into `App` + `Window`; add the entity store, `Ctx`, `Listener`.
2. Render windows from a root entity; dispatch clicks to listeners.
3. Convert all 16 examples; `system_monitor`'s `onShutdown` became `~MonitorApp`.
4. Story gallery and showcase become entities; every page takes `Ctx*`.
5. Components take `Ctx*` instead of `Arena*` (~40 builders, ~100 call sites).
6. One entity per story page; `StoryApp` drops from 56 fields to the shell.
7. Delete `AppHooks`.

Details and the rules for new code: the *App, Window, Entity, Ctx* section of [AGENTS.md](AGENTS.md). Known deviations: [port-progress.md](port-progress.md).

### Next

Fidelity on story pages vs Rust (`crates/story/src/stories`), then optional heavy surfaces (full editor, TextView, dock/tiles, webview). On the runtime side: real second windows for dialogs and notifications, actions / key bindings, and finer-grained `Notify`.

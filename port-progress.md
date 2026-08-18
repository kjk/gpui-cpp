# Port progress

## Current phase

**system_monitor**, **app_assets**, the twelve simple examples, the **gpui-base showcase**, and the **crates/ui story gallery** all run on Windows.

```
bun cmd/build.ts all
bun cmd/build.ts -rel showcase
out\rel\showcase.exe
```

## Status

| Phase | Status | Notes |
| --- | --- | --- |
| 0. Plan + AGENTS.md | done | `AGENTS.md`, `port.md` |
| 1. Vendored base + build | done | `src/Base.h` / `src/Base.cpp` from SumatraPDF + `cmd/build.ts` |
| 2. D2D window + chrome | done | Win32 + `ID2D1DCRenderTarget` (HWND target did not present) |
| 3. Flex + TitleBar + tabs | done | Segmented System / Processes tabs |
| 4. AreaChart | done | Grid, stroke, gradient fill, 120-sample history |
| 5. SysInfo + 500 ms loop | done | CPU, memory, disk, battery, processes via Win32 |
| 6. Process table | done | Sortable columns, stripes, virtualized rows, top 200 |
| 7. Status bar + icons | done | Disk / memory / CPU chips + battery if present |
| 8. Visual match vs Rust | mostly | See remaining gaps below |
| 9. app_assets | done | Light window, Inbox + Bot from `assets/app_assets/icons/*.svg` |
| 10. hello_world | done | Button, hover, light theme tokens |
| 11. window_title | done | In-client title strip + Hello World body |
| 12. root_borderless | done | Root.bordered=false note; still uses Win32 chrome |
| 13. tooltip_top_edge | done | Absolute top-edge button; tooltip flips below |
| 14. input | done | LineInput + WM_CHAR; Hello, {name}! |
| 15. focus_trap | done | Two Tab traps + buttons outside |
| 16. dialog_overlay | done | Center dialog, bottom sheet, context menu |
| 17. sidebar | done | Collapsible icon/offcanvas/none + Lucide nav |
| 18. table_in_scrollable | done | Nested scroll; inner table y-band heuristic |
| 19. text_selection | done | Selectable text block |
| 20. markdown_table | done | Heading / hr / paragraph / pipe-table parser |
| 21. fps_monitor | done | Hilbert + Catmull-Rom + HSL customPaint, 16 ms |
| 22. showcase | done | `crates/base/examples/showcase` — overview + 39 component pages |
| 23. story gallery | done | `crates/story` — sidebar + 62 stories (`bun cmd/build.ts story`) |
| 24. App / Window / Entity / Ctx | done | GPUI's runtime shape; see AGENTS.md |

## What matches the Rust example

- Dark theme tokens from Default Dark (`#0a0a0a` background, `#fafafa` text, red/blue/green/yellow scales)
- 680×600 client, centered, title "System Monitor"
- Segmented tabs **System** / **Processes**
- Total RAM on the right (`X.X GB`)
- CPU and memory area charts with dashed grid, axis gutter, live `%`
- 500 ms refresh, 120-sample ring buffer
- Process table: PID, Name, CPU %, Memory; default CPU descending; striped rows; color thresholds; human-readable bytes
- Status bar: hard-drive / memory / CPU icons + 48×8 progress + percent
- Battery chip when `GetSystemPowerStatus` reports a battery
- Alt+F4 / system close quits (WS_SYSMENU)

## Remaining gaps vs Rust

- **Window chrome.** GPUI draws a client title bar (min/max/close, drag, 34 px). This port keeps the standard Win32 title bar (dark if DWM allows) and puts the tab bar underneath. `WM_NCCALCSIZE` custom frames made the DC render target present as black.
- **HWND / GPU path.** Painting is Direct2D *DC* target (GDI-compatible), not a GPU HWND/DXGI swap chain like GPUI/Blade.
- **Chart interaction.** No hover tooltip / crosshair (Rust `AreaChart::id`).
- **Process CPU %** is a Win32 times delta, not `sysinfo`; first sample is 0; values are in the same ballpark, not bit-identical.
- **Icons** are Lucide SVG strokes when assets exist (`app_assets`, `sidebar`); otherwise Direct2D path sketches.
- **Client title bar / borderless Root.** `window_title` and `root_borderless` still keep standard Win32 chrome (custom `WM_NCCALCSIZE` frames present black on the DC target).
- **Markdown** is a heading / hr / paragraph / `|` table parser, not GPUI `TextView`.
- **Nested scroll** in `table_in_scrollable` now uses a real inner `ScrollY` body plus thumbs.
- **Showcase editor** is a line-numbered textarea with simple keyword colors, not Syntect + folding.
- **Showcase text-selection** is character-accurate via DirectWrite hit-test.
- **Showcase virtual-list** virtualizes 100k rows with a spacer + always-on thumb; not GPUI `v_virtual_list`.
- **Story gallery** pages are themed façades of `src/component`, not a line-for-line port of every Rust story variant (editor/highlighter, full DataTable, native menus, dock/tiles).
- **`Notify` is coarse.** The frame tree is rebuilt every paint, so `Notify(cx)` invalidates the window instead of tracking which views observe an entity. The API matches GPUI, the invalidation does not.
- **No actions or key bindings.** GPUI dispatches `Box<dyn Action>` through the focus chain; here a window-level `WindowOnKey` listener plus per-element click listeners cover the same ground. The story gallery adds a per-page key subscription (`STORY_PAGE_KEYS`) so Esc reaches the page with an overlay open.
- **One window.** `App` holds a window list and the loop ends when the last one closes, but nothing opens a second window yet — dialogs, sheets and notifications are still drawn inside the main window.
- **No `EventEmitter` / `subscribe`.** Views talk to each other by holding an `Entity<T>` and calling `Get`.

## How to run

```
bun cmd/build.ts              # prints example list
bun cmd/build.ts -rel hello_world
bun cmd/build.ts -rel showcase
bun cmd/build.ts -rel story
bun cmd/build.ts -dbg all
bun cmd/build.ts -rel -asan system_monitor
out\rel\system_monitor.exe
out\rel\showcase.exe
out\rel\showcase.exe button
out\rel\story.exe
out\rel\story.exe Alert
```

`all` builds `system_monitor`, `app_assets`, `showcase`, and every name in `simpleExamples` in `cmd/build.ts`.

Rust reference (slow first build; pulls Zed GPUI). Pins: [`cmd/versions.ts`](cmd/versions.ts). `bun cmd/build.ts` clones `.work/gpui-component` at that SHA if missing.

```
bun cmd/versions.ts
cd .work\gpui-component
cargo run -p system_monitor
```

## Decisions locked

- Windows only, MSVC `cl.exe`, `bun cmd/build.ts`
- No STL containers; `Str` / `Vec` / `Arena` from SumatraPDF (`src/Base.h`)
- Direct2D DC render target + DirectWrite
- Frame-rebuilt element tree on a frame arena
- Win32 APIs instead of `sysinfo` / `battery`
- Do not reuse `../gpui/` (STL experiment)

## Log

- 2026-08-16: Analyzed gpui-component, system_monitor, theme tokens, SumatraPDF base. Wrote AGENTS.md and port.md.
- 2026-08-16: Vendored base, implemented gpui subset + system_monitor. App runs: charts, process table, status bar, 500 ms refresh.
- 2026-08-16: Ported `app_assets`. Asset roots + Lucide SVG stroke renderer. Light theme. Inbox and Bot icons load from `assets/app_assets/icons`.
- 2026-08-16: Replaced `cmd/build.bat` with `bun cmd/build.ts`.
- 2026-08-16: `build.ts` flags `-rel` / `-dbg` / `-asan`; outputs in `out/rel`, `out/dbg`, `out/rel_asan`, `out/dbg_asan`.
- 2026-08-16: Static MSVC CRT (`/MT` / `/MTd`).
- 2026-08-16: Ported twelve examples, each in its own commit: `hello_world`, `window_title`, `root_borderless`, `tooltip_top_edge`, `input`, `focus_trap`, `dialog_overlay`, `sidebar`, `table_in_scrollable`, `text_selection`, `markdown_table`, `fps_monitor`. Runtime grew Button/hover/focus/tooltip, Tab traps, LineInput, overlays/menus, customPaint, nested wheel scroll.
- 2026-08-16: Ported `crates/base/examples/showcase` as `bun cmd/build.ts showcase`. Overview grid plus one commit per component page.
- 2026-08-17: Closed remaining showcase content diffs (calendar weekday, resizable divider, tree chevrons, thumbs, shrink-wrapped buttons, click coverage).
- 2026-08-17: Ported `crates/story` as `bun cmd/build.ts story`. Sidebar gallery plus one commit per story (62 pages).
- 2026-08-17: Recorded upstream pins in `port-upstream.md`: gpui-component `da4f93696dc2`, zed gpui `cc053a4a6fa2`.
- 2026-08-17: Moved pins to `cmd/versions.ts` (source of truth). `build.ts` / `run.ts` clone `.work/gpui-component` at that SHA.
- 2026-08-18: Click dispatch moved onto the elements. Listener carries the value a Rust closure would capture; component callbacks became Listeners instead of raw-pointer Func1s; the 62 story pages, 33 showcase pages and 6 examples dropped their click-id switches. `El::Click(id)` is identity only now (hit-test, hover, focus, Tab traps) and WindowOnUnhandledClick is just the outside-click dismiss.
- 2026-08-18: Adopted GPUI's runtime shape. `AppHost` split into `App` (factories, fonts, window list, entity store) and `Window` (render target, frame arena, hover/focus, root view). Added generational `Entity<T>`, `Ctx`, `Listen`/`Notify`, and window subscriptions. Every example, the showcase and all 62 story pages became view entities; components take `Ctx*` instead of `Arena*`; `StoryApp`'s 56-field god struct became one entity per page. `AppHooks` removed.

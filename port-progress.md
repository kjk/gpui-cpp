# Port progress

## Current phase

**system_monitor**, **app_assets**, the twelve simple examples, the **gpui-base showcase**, and the **crates/ui story gallery** all build on **Windows, Linux, macOS and wasm**. The macOS `system_monitor`, `hello_world`, and `story` examples have also been run and visually smoke-tested.

```
bun cmd/build.ts all
bun cmd/build.ts -rel showcase
out\rel\showcase.exe      # Linux: out/rel/showcase
```

## Status

| Phase                           | Status | Notes                                                                                                                               |
| ------------------------------- | ------ | ----------------------------------------------------------------------------------------------------------------------------------- |
| 0. Plan + AGENTS.md             | done   | `AGENTS.md`, `port.md`                                                                                                              |
| 1. Vendored base + build        | done   | `src/base.h` / `src/base.cpp` from SumatraPDF + `cmd/build.ts`                                                                      |
| 2. D2D window + chrome          | done   | Win32 + a DXGI flip-model swap chain with an `ID2D1DeviceContext` on its back buffer                                                 |
| 3. Flex + TitleBar + tabs       | done   | Segmented System / Processes tabs                                                                                                   |
| 4. AreaChart                    | done   | Grid, stroke, gradient fill, 120-sample history                                                                                     |
| 5. SysInfo + 500 ms loop        | done   | CPU, memory, disk, battery, processes via Win32                                                                                     |
| 6. Process table                | done   | Sortable columns, stripes, virtualized rows, top 200                                                                                |
| 7. Status bar + icons           | done   | Disk / memory / CPU chips + battery if present                                                                                      |
| 8. Visual match vs Rust         | mostly | See remaining gaps below                                                                                                            |
| 9. app_assets                   | done   | Light window, Inbox + Bot from `assets/app_assets/icons/*.svg`                                                                      |
| 10. hello_world                 | done   | Button, hover, light theme tokens                                                                                                   |
| 11. window_title                | done   | `component::TitleBar` over the body, no server chrome                                                                               |
| 12. root_borderless             | done   | Root.bordered=false note; genuinely frameless, like `titlebar: None`                                                                |
| 13. tooltip_top_edge            | done   | `component::TitleBar` + absolute top-edge button; tooltip flips below                                                               |
| 14. input                       | done   | InputState + WM_CHAR; Hello, {name}!                                                                                                 |
| 15. focus_trap                  | done   | Two Tab traps + buttons outside                                                                                                     |
| 16. dialog_overlay              | done   | Center dialog, bottom sheet, context menu                                                                                           |
| 17. sidebar                     | done   | Collapsible icon/offcanvas/none + Lucide nav                                                                                        |
| 18. table_in_scrollable         | done   | Nested scroll; inner table y-band heuristic                                                                                         |
| 19. text_selection              | done   | Selectable text block                                                                                                               |
| 20. markdown_table              | done   | `src/markdown` parses, `component::TextView` renders                                                                                |
| 21. fps_monitor                 | done   | Hilbert + Catmull-Rom + HSL customPaint, 16 ms; `crates/fps` HUD in `src/gpui/fps.h`                                                |
| 22. showcase                    | done   | `crates/base/examples/showcase` — overview + 39 component pages                                                                     |
| 23. story gallery               | done   | `crates/story` — sidebar + 65 stories; upstream-style client title bar on all three (`bun cmd/build.ts story`)                      |
| 24. App / Window / Entity / Ctx | done   | GPUI's runtime shape; see AGENTS.md                                                                                                 |
| 25. Linux port                  | done   | X11 + cairo + Pango behind `Paint.h` / `Platform.h`; every target builds and runs; undecorated windows resize from their own edges  |
| 26. macOS port                  | done   | Cocoa + Core Graphics + Core Text behind the same seams. Every target builds; native traffic lights coexist with client title bars. |

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
- Client title bar: segmented tabs, total RAM, and the min/max/close controls, with the rest of the strip the drag region
- Alt+F4 / system close quits (WS_SYSMENU)

## Remaining gaps vs Rust

- **The GPU path stops at the tessellator.** Painting goes to a DXGI flip-model swap chain with an `ID2D1DeviceContext` on its back buffer, which is the shape GPUI's `directx_renderer.rs` has. What is still not GPUI's is what happens above that: D2D tessellates strokes and paths on the CPU (`FillNonOverlappingRectangles_SlowPath`) where GPUI rasterizes them in a shader, so a scene made of many thin antialiased paths costs more here — 59% of `fps_monitor`'s frame is D2D widening and tessellating ~2300 hairlines, and the same frame costs 1.02 ms on Direct2D against 0.364 ms under `GPUI_PAINT=gpu`. See "Thirty-two frames a second, and where they went" at the end of this file for the profile and the numbers.
- **Process CPU %** is a Win32 times delta, not `sysinfo`; first sample is 0; values are in the same ballpark, not bit-identical.
- **Icons** are Lucide's own SVG files: `AppNew` registers the default asset roots when nothing else has, so any app finds `assets/icons/*.svg` without asking. Where the folder is genuinely missing they fall back to the stroke sketches in `DrawIcon`, which cover all 74 `IconName`s.
- **Markdown is the crate, ported.** `src/markdown/` is markdown-rs 1.0.0 — the `markdown` crate `crates/ui` parses with — so a `TextView` reads the same mdast Rust does. HTML is `src/ui/html.cpp` in html5ever's place, folding into the same tree. Headings, lists, tables, quotes, images, footnotes, task lists, inline HTML and raw HTML blocks. The code fences are coloured by the scanner below rather than by tree-sitter.
- **Nested scroll** in `table_in_scrollable` now uses a real inner `ScrollY` body plus thumbs.
- **Syntax colouring is a scanner, not a parser.** `src/ui/syntax.cpp` is what a per-language scanner can carry of upstream's tree-sitter queries — comments, strings, numbers, keywords, type names, and what position alone settles, like a name before `(` being a call. It has no tree, so nothing that needs one — a rename, a semantic scope — can be asked of it. Code folding turned out not to be one of those: upstream's own fold extractor is "every named node spanning two rows or more", and its showcase highlighter finds the same blocks by scanning brace pairs, which is what this does.
- **Showcase text-selection** is character-accurate via DirectWrite hit-test; a double click takes the word and a triple the paragraph.
- **Showcase virtual-list** virtualizes 100k rows with a spacer + always-on thumb; not GPUI `v_virtual_list`.
- **Story gallery** pages are themed façades of `src/ui/`, not a line-for-line port of every variant on every Rust story page. The pages the earlier version of this line named as missing — editor/highlighter, the full DataTable, native menus, dock and tiles — are all there now.
- **`Notify` is coarse.** The frame tree is rebuilt every paint, so `Notify(cx)` invalidates the window instead of tracking which views observe an entity. The API matches GPUI, the invalidation does not.
- **The keymap and its users.** `src/gpui/keymap.*` is `Keystroke::parse`, the binding table, key contexts with `key=value` pairs, `KeyBindingContextPredicate` (`"Editor && mode == full"`, `"Workspace > Editor"`, `!`, `||`, parentheses) and multi-stroke bindings (`"ctrl-k ctrl-o"`), and `WindowDispatchKeyAction` resolves a chord against the contexts stacked over the focused element and then walks the handlers out from it. Every component keyboard is on it: the popup menu, the dialog and alert, the select and combobox, the list, the tree, the data table, the date picker, the popover, the sheet and the colour picker each call their `init` and declare their context the way Rust's modules do, so no application wires a component's keys up by hand any more. What a component's state cannot hold — a dialog's handlers, a date picker's — waits in a keyed entity beside it, since the port's components are builders where Rust's are views. `InputState` is the one keyboard still translated rather than bound: the window offers it a chord before the keymap, which is what makes a focused field's editing the innermost context.
- **A second window opens.** `App` has held a window list and ended its loop with the last one for a while; the story's Window menu is what finally opens one. `create_new_window_with_size` is `StoryOpenWindow` — a fresh `StoryApp` entity, `TitleBar::window_options()`, and the window's own unhandled-click and key subscriptions — and `GpuiMain` goes through it too, so the first window and the second are the same thing. Dialogs, sheets and notifications are still drawn inside the window that owns them, which is where Rust draws them too: they are `Root`'s layers, not windows of their own.
- **Multi-click is on the event, and in the input.** `ClickEvent::clickCount` is GPUI's `click_count`; the selectable-text paths use it, and so does `InputState` — a double click takes the word and a triple the line, which is `input/base/selection.rs`. `component::DataTable` uses it for both `TableEvent::DoubleClickedRow` and `DoubleClickedCell`; cell selection is on the story's Options menu, and with it the row header column and the escalation a click on an already-selected cell makes without one.
- **The input engine is ported bar the language server.** `crates/base/src/input/base` is there, and so is the display map the arrows walk — soft wrap, display rows, wrapped-line movement — along with the IME marked range, `scroll_to`, the highlighted runs, the decorated ranges, the indent pair on tab and on `ctrl-]` / `ctrl-[`, and number stepping. The search session and code folding have since been ported too, and so has every seam in `input/editor/lsp` — completion with its trigger, its resolve and its ghost text, hover, code actions, document colours, range semantic tokens, go to definition and the overlay the host draws its own menus through. What no seam can supply is a language *server*: there is no JSON-RPC, no child process and no `lsp_types`, by the same hard rule that keeps tree-sitter out, so a provider here is a function pointer an application fills. `start_of_line` / `end_of_line` take the wrapped row first and the logical line on a second press, which is what Rust gates on `soft_wrap && is_code_editor()`.
- **Mouse capture is `PlatSetMouseCapture`.** The press is the window's for as long as the button is down: `SetCapture` on Windows, an active `XGrabPointer` on X11, and nothing on macOS, where Cocoa already routes `mouseDragged:` and `mouseUp:` to the window the press went to. Windows also answers `WM_CAPTURECHANGED` by ending the press where it was, since a capture handed to a title-bar drag never gives the button back.
- **`EventEmitter` is `cx.emit` / `cx.subscribe`.** `EntityEmit` fans an event out to every live subscription and `Subscribe(cx, emitter, &Self::OnEvent)` makes one, handing back a `Subscription` that `EntityUnsubscribe` gives up; a subscription whose emitter or subscriber has gone stale is swept on the next emit, which is what Rust's guard does on drop. There is no trait to mark what an entity emits, so nothing checks that the subscriber's handler takes the type the emitter sends — that is the one thing Rust's `EventEmitter<E>` gets that this cannot. `ListState`, `TableState` and `TreeState` emit through it; their `onEvent` field stays as the shorthand for the single-subscriber case and hears the same event, and the other states (`InputState`, `SliderState`, `PopupMenuState`, `SearchableListState`, `ClipboardState`) still carry a listener each, `InputState` because it is not an entity at all.
- **The dock is both halves.** `crates/ui/src/dock` is ported as a tree of tab groups and splits — `DockArea` with a Dock on the left, the right and the bottom, tabs dragged between groups or onto an edge to split one, resize handles everywhere, zoom and close — and `tiles.rs` is there too: the free canvas of overlapping panels, each moved by its drag bar and resized by its edges, both magnetic against a neighbour's edge or the grid. `dock/state.rs` round-trips a layout through the JSON reader in `src/base/json.h` rather than through serde, `TileMeta` and all.

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

`all` builds `system_monitor`, `app_assets`, `showcase`, and every name in `simpleExamples`.

`cmd/build.ts` and `cmd/run.ts` dispatch by host: `build-windows.ts` / `run-windows.ts` on Windows, `build-linux.ts` / `run-linux.ts` on Linux, and `build-mac.ts` / `run-mac.ts` on macOS. On Linux the binaries land in `out/linux/rel/showcase` (no `.exe`) and need X11 + cairo + Pango; `bash cmd/ubuntu-install-deps.sh` installs the lot. macOS binaries land under `out/mac/`. From a Windows checkout, `bun cmd/wsl-run.ts -rel showcase` builds and runs the Linux binary under WSLg.

Rust reference (slow first build; pulls Zed GPUI). Pins: [`cmd/versions.ts`](cmd/versions.ts). `bun cmd/build.ts` clones `.work/gpui-component` at that SHA if missing.

```
bun cmd/versions.ts
cd .work\gpui-component
cargo run -p system_monitor
```

## Decisions locked

- MSVC `cl.exe` on Windows, clang on Linux and macOS, all through `bun cmd/build.ts`. "Windows only" was the decision for the first milestone and stopped being true at phases 25 and 26.
- No STL containers; `Str` / `Vec` / `Arena` from SumatraPDF (`src/base.h`)
- Direct2D + DirectWrite on Windows, over a DXGI flip-model swap chain. The DC render target this used to say was replaced once profiling showed its GDI interop costing ~70% of the frame; only the offscreen target, which hands its pixels back as a DIB, still uses one.
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
- 2026-08-18: `fps_monitor` matches the Rust example. Ported `crates/fps` (the `gpui-fps` crate) to `src/gpui/fps.h` / `Fps.cpp`: frame sampler over the window's own frame trace, frame time chart, FRAME / DROP / CPU / MEM readouts, click-to-collapse, and the 8-way anchored overlay. Runtime grew `RgbaHsla`, `El::Mono()` (Consolas, inherited like `font_family`), `El::ItemsEnd()`, `TimeNow()` (QPC) and `WindowCollectFrames` (GPUI's `FrameTimingCollector`). The example's own fixes: HSL lightness is clamped instead of wrapping (blue where the original is white), non-finite projections are dropped, the load buttons use the original's translucent style, and the tilt eases in render rather than on a timer.
- 2026-08-18: `input` matches the Rust example. It builds on `component::Input` bound to a `LineInput` (GPUI's `InputState`) instead of a hand-rolled field, and subscribes to the state's change event rather than watching every key. `LineInput` grew that `onChange` listener (`InputEvent::Change`, fired by the window after an edit); the themed Input took Rust's Medium metrics (h 32, px 10, py 8, text_sm, radius, `input.border` / `ring` / `caret` theme tokens) and pushes them into the base as an `InputEditorStyle`, the way Rust calls `set_editor_style`. Inputs fill their parent like Rust's, so `component::Setting` rows became full width and the story settings page sizes its two controls.
- 2026-08-18: Text is laid into GPUI's line box. Every line now gets a box phi (1.618) times the font size — `TextStyle::line_height`'s default — with ascent+descent centered in it, instead of DirectWrite's tighter natural height, so text blocks and the rows that shrink-wrap them stop coming out shorter than the original. `El::LineHeight` is the per-element override (`line_height(relative(1.))` on the FPS figure, 1.25rem inside an Input), keyed into the shaped-text cache. The local workarounds this replaces — explicit line boxes in the FPS HUD, a hand-computed load-button height — are gone.
- 2026-08-18: Markdown is parsed by md4c. `ext/md4c` vendors the CommonMark parser (0.5.3, one C file, MIT) and `component::TextView` became what `crates/ui/src/text` is: md4c's SAX callbacks build an `MdNode` block tree — the shape of `text/node.rs`'s `BlockNode` — and the renderer walks it. That replaces the line-at-a-time parser and brings blockquotes, fenced code blocks, nested and ordered lists, task lists, strikethrough, autolinks, link and image alt text, HTML entities and content-proportional table columns. `examples/story/welcome.cpp` collapsed from 434 hand-built lines to what `welcome_story.rs` is — `markdown(README.md).selectable(true)` — rendering `assets/story/README.md`, gpui-component's README at the pinned SHA. Still short of Rust: no syntax highlighting (no tree-sitter), no images, no link clicks, no strikethrough or table column alignment in the paint layer.
- 2026-08-18: Compiled and ran the macOS port locally for the first time. All examples build under Apple clang; `hello_world` and `system_monitor` were visually smoke-tested, including tab switching and live process sorting. Fixed Core Text foreground colors, Mach-time process CPU conversion, initial window activation, chart-header spacing, and Apple clang-format discovery.
- 2026-08-18: Matched the story gallery's macOS window chrome. `WinOpts::clientTitleBar` gives Cocoa a transparent full-size content view while preserving native traffic lights; the story draws the pinned 34 px AppTitleBar with its 80 px traffic-light reserve, menu labels, and tool icons. Client controls opt out of AppKit's implicit title-bar dragging, while empty chrome supports drag and double-click zoom. The Mac display clamp now uses the full display bounds like GPUI, restoring the reference's 85% window height.
- 2026-08-18: The story owns its title bar on Windows and Linux too. `crates/ui/src/title_bar.rs` became a real `component::TitleBar` — 34 px tall, `TITLE_BAR_LEFT_PADDING` (80 on macOS for the traffic lights, 12 elsewhere), the `default_title_bar_background` mix, its children justified across the bar, and off macOS the three 34 px `WindowControls` cells with `window-minimize` / `window-maximize` / `window-restore` / `window-close` and the danger hover on close. `WinOpts::clientTitleBar` now means the same thing on all three platforms. The Win32 window drops `WS_CAPTION` but keeps the thick frame, hands the caption band back in `WM_NCCALCSIZE` (the frame thickness returns at the top when maximized so the bar is not clipped), adds the top-edge and top-corner resize band to `WM_NCHITTEST`, and forwards `WM_NCMOUSEMOVE` so the control cells can hover while Windows keeps snap layouts. Creation only asks the `wParam == FALSE` form of `WM_NCCALCSIZE`, so the window forces the real one with `SWP_FRAMECHANGED` before it is shown. The X11 window drops the frame the same way `borderless` did and grew what an undecorated window has no frame for: a 6 px resize band around the client that starts the matching `_NET_WM_MOVERESIZE` drag and shows the eight edge cursors. `root_borderless` gets all of this too, which is what `titlebar: None` means in the Rust example.
- 2026-08-18: `system_monitor` and `window_title` moved onto `component::TitleBar`, which is what their Rust `TitleBar::window_options()` asks for — the segmented tabs and the total-RAM label now sit in the client title bar next to the window controls instead of under a Win32 caption, and the last two "still keeps standard Win32 chrome" gaps are gone. `El::HoverFg` is the missing half of `HoverBg`: GPUI's `hover(|style| style.text_color(..))` cascades, but a Text or an Icon here resolves its own color when it paints, so a hovered element stamps its hover color onto the descendants that set none — a child with a color of its own keeps it, and so does its subtree. That is how the close cell turns its glyph `danger_foreground` when it fills with danger. Right-clicking the X11 title bar's drag region asks the window manager for `_GTK_SHOW_WINDOW_MENU`, the menu a server-decorated window would have given; Windows already gets it from DefWindowProc on `WM_NCRBUTTONUP` over HTCAPTION.
- 2026-08-18: The amalgam is two files instead of four. `cmd/build-dist.ts` now emits only `gpui.h` and `gpui.cpp`, the same pair on every platform: each `_win.cpp` / `_linux.cpp` / `_mac.cpp` / `_posix.cpp` goes into `gpui.cpp` inside its own `#if GPUI_OS_*`, which keeps `<windows.h>`, `<X11/Xlib.h>` and `<Cocoa/Cocoa.h>` out of one translation unit exactly as the separate platform file did — the preprocessor drops the other two halves before anything parses them. macOS compiles the whole file as Objective-C++, because the mac half is. Static-name collisions are now computed per platform's view of the tree, so a `ClientDecorated` in both `Window_win.cpp` and `Window_linux.cpp` costs neither of them a rename. `ext/md4c` joined the amalgam as its tail — `md4c.h` at the end of `gpui.h`, `md4c.c` at the end of `gpui.cpp` — so it compiles as C++ rather than as its own C translation unit; the vendored files stay byte-for-byte upstream and the amalgamator applies the six casts C++ needs, each asserted to match exactly once. `JoinPath` builds its path by copy-and-append instead of `snprintf`, because one big translation unit gives gcc enough inlining context to call the deliberate truncation a bug.
- 2026-08-18: Every example window is titled `<name> C++`, so a screenshot says which of the two implementations it came from without having to look at the pixels. `tooltip_top_edge` draws `component::TitleBar` too, so the top edge its trigger sits against is ours rather than the window manager's; the tooltip still has to flip, since the three wrapped lines it needs are more than twice the 34 px above the trigger.
- 2026-08-18: The tree has tests. `tests/` is `utassert(cond)`, a counter, and one file per ported Rust module; `bun cmd/test.ts` builds and runs it, and CI does the same on all three platforms after the `-all` build. 143 checks, all ports of pure-logic tests in `.work/gpui-component`: `crates/base/src/positioner.rs` (both groups, the positioner's own and the ones that migrated in from the tooltip module), `crates/ui/src/plot/scale/{linear,point,ordinal}.rs`, `crates/fps/src/sampler.rs`, and `default_title_bar_background` from `crates/ui/src/title_bar.rs`. Two of those needed the code first: `src/gpui/positioner.h` is a port of the shared positioner — prefer a side, flip when it does not fit, fall back to the roomier one, align start/center/end, clamp into the viewport — and the tooltip's own four-line placement is gone, so tooltips now center on their trigger with no gap the way `TooltipPositioner` does. `src/ui/plot.h` was a three-field stub and is now the d3 scales, over float domains rather than Rust's generics. `FrameSamplerIngest` splits the drain half out of `FrameSamplerTick` so the rolling FPS window can be driven without a window. What did not port: the 413 `#[gpui::test]` cases need GPUI's `TestAppContext`, and `crates/ui/src/theme/color.rs` tests an `Hsla` that mixes along the shorter hue arc, which our 8-bit `Rgba` is not.
- 2026-08-19: Double clicks reach the element tree. `ClickEvent` and `MouseEvent` carry `clickCount`, GPUI's `MouseDownEvent::click_count` — Rust's `on_double_click` is `on_click` plus `click_count() == 2`, so the count on the event is the whole API. Every press now dispatches whatever its count: `WM_LBUTTONDBLCLK` is the second press of a run under another name and falls through to the same `WindowMouseDown`, and the X11 and Cocoa windows stopped returning early on one. Before this a fast double click on a button fired it once instead of twice. The counting moved into `WindowClickCount` in `window_common.cpp` — same button, inside `PlatDoubleClickMs()` (`GetDoubleClickTime`, `[NSEvent doubleClickInterval]`, 400 ms on X11), within 4 DIPs — so all three platforms agree on what a run is, a third press counts as 3 where Win32 has no message for it, and the X11 window's file-static detector is gone. `WindowDoubleClick` folded into the tail of `WindowMouseDown`: the caption still zooms, but the press is dispatched first, and the empty-chrome half of the heuristic now requires `clientTitleBar` so a double click near the top of a system-decorated window is not a zoom. The X11 title bar zooms at all now — it used to hand the first press to `_NET_WM_MOVERESIZE` and never see the second. `crates/base/src/text_boundary.rs` is ported as `TextWordRangeAt` / `TextLineRangeAt` (`TextMultiClickRange` is `points_for_multi_click`): word characters join word characters and spaces join spaces, so punctuation and CJK come out one character at a time and `résumé` comes out whole. `dialog_overlay`, the story gallery and the showcase text-selection page select the word on two clicks and the paragraph on three. `tests/TextBoundaryTests.cpp` ports the boundary table from `crates/ui/src/text/selection.rs` and the `line_range_at` case from `text_selection.rs`.
- 2026-08-19: The mouse events are GPUI's. `MouseKind` + one `MouseEvent` became `MouseDownEvent`, `MouseUpEvent`, `MouseMoveEvent`, `MouseExitEvent` and `ScrollWheelEvent` from `crates/gpui/src/interactive.rs`, each carrying what Rust's does: a `MouseButton`, `Modifiers`, the click count, `firstMouse`, the pressed button on a move, a two-axis scroll delta with `precise` and a `TouchPhase`. Four of Rust's shapes do not survive the crossing and say so in the header — `MouseButton::Navigate(NavigationDirection)` becomes two constants, `Option<MouseButton>` becomes a flag plus a value, `ScrollDelta::Pixels | Lines` becomes a delta plus `precise` (the windows here turn a notch into 48 DIPs at the seam rather than deferring to `pixel_delta(line_height)`), and `Point<Pixels>` stays `x` and `y`. `PlatformInput` is the tagged union Rust's enum is, and `WindowDispatchInput` is `Window::dispatch_event`: the five `WindowMouse*` seam calls collapsed into it, and the platform files build events with the `Input*` constructors that stand in for Rust's tuple variants. Window subscriptions split the same way `window.on_mouse_event::<T>` does — `WindowOnMouseDown` / `Up` / `Move` / `Exit` and `WindowOnScrollWheel` — so a handler takes the event it is about instead of switching on a kind. What this buys beyond shape: every mouse event now carries the modifier keys, which none of them did before (`Modifiers::secondary()` included, Command on macOS and Control elsewhere); the middle and both thumb buttons arrive on all three platforms, where only left and right did; horizontal wheels arrive (`WM_MOUSEHWHEEL`, X11 buttons 6 and 7, `scrollingDeltaX`); a macOS trackpad's gesture phase and precise deltas come through as themselves; and `MouseDownEvent::firstMouse` marks the press that activated the window, which Windows knows from `WM_MOUSEACTIVATE`. `ClickEvent` keeps its flat shape — it also carries the hit rect's id and box, which a Rust hitbox does not have to — but gained the button, the modifiers and a `keyboard` flag, which is `ClickEvent::Keyboard`: Space or Enter on the focused element. What is still not GPUI: a click fires on the press rather than the release, and there is no per-element `on_mouse_down`.
- 2026-08-19: Geometry is `Point`, `Size`, `Bounds` and `Edges`. `crates/gpui/src/geometry.rs` spells them `Point<T>` / `Size<T>` / `Bounds<T>` / `Edges<T>`, where `T` is a unit rather than an element type — `Pixels`, `ScaledPixels`, `DevicePixels`, `Rems`, `Length` — so the compiler refuses to add device pixels to logical ones. Everything above `Paint.h` here is DIPs, which leaves that generic with one instantiation (`Point<Pixels>` is 170 of the 185 `Point<T>` in gpui-component; the rest are the generic definitions themselves), so these are plain float aggregates and the arithmetic — `Contains`, `Right`, `Inset` — is written out once, under Rust's names: `Bounds::Inset(float)` is `inset` (`dilate` negated) and the `Edges` overload is `extend` negated. The two four-float rectangles the tree already had, `RectF` in `Paint.h` and `Rect` in `Positioner.h`, are that one `Bounds` now. What moved onto them: `MeasureText` returns a `Size` instead of filling two out-params, and so does `TextLayoutNew` across all three backends; `TextLayoutRangeRects` writes `Bounds`; the positioner takes `PositionSide(trigger, popup, view, …)` and `PositionCorner(anchor, at, popup, view, …)` instead of eight loose floats; `Style::padL/T/R/B` is one `Edges pad`; `HitRect`, `ScrollRect`, `TextHit` and `FocusRect` carry a `Bounds bounds`, which is what a GPUI hitbox has, and hit-testing is `bounds.Contains({x, y})` rather than four comparisons spelled out at each site; `ClickEvent::elX/elY/elW/elH` is `Bounds el`; and `El::Bounds()` hands the laid-out box over as a value. What deliberately stayed flat: `El`'s own `x/y/w/h`, which the layout pass writes a component at a time, and an event's position, where `ev->x` is what every handler wants. A unit that is not DIPs gets a named struct instead of a parameter — `WinSize` names the DIP and device-pixel pair, and the backends scale on the way to Direct2D / cairo / Core Graphics. `Corners` is the one member of the set that is not here: nothing rounds a box per corner yet (`radius` is a single float), and the first user would be the NumberInput step buttons, whose outer corners are rounded in Rust and square here.
- 2026-08-19: The slider is Rust's. `crates/base/src/slider.rs` keeps min/max/step/value/percentage/bounds/scale and a dragging flag in an `Entity<SliderState>`, hangs the behavior off the elements — `SliderTrack::on_mouse_down` and `on_drag_move`, `SliderThumb`'s drag, `Slider`'s `on_mouse_up` + `on_mouse_up_out` — and emits `SliderEvent::Change` while the value moves and `Release` when the user lets go. Here there was no state at all: `component::Slider` took a 0..1 fraction, and the pages did the arithmetic — the story page turned a `ClickEvent` into a value with its own `ClickFraction`, owned the logarithmic playback scale as a `powf`, and picked which end of a range to move by hand, while the showcase page kept a `draggingSlider` flag and rescanned `win->paint.hits` on every window-level mouse move. `SliderState` is now that state, a POD the view owns and hands over by pointer the way `LineInput` is, with `percentage_to_value`, `value_to_percentage`, `update_thumb_pos`, `update_value_by_position` and `handle_release` ported onto it and `SliderStateNew` standing in for Rust's `SliderState::new().min(..).max(..).default_value(..)` chain. Values are in their own units again: the color channels are 0..360 and 0..100, the price range is a real `SliderRange`, playback speed is `SliderScale::Logarithmic` over 0.25..4, and a value snaps to `step`. What made that possible is a seam the tree did not have: `El::OnMouseDown` / `OnMouseUp` / `OnDragMove` are `div().on_mouse_down(..)` and `on_drag_move`, dispatched from the same hit rects the click path walks, and `Window::pressedId` gives the element that took the press every move until the release — GPUI's drag entity without the entity, which is why it needs a `Click(id)` to find the element again in a tree that is rebuilt every frame. An element has no closures to capture a state handle, so it names its state instead (`El::BindSlider`, `El::BindSliderBounds`) and the window does what those closures do: jump to the press, take the nearer end of a range by Rust's midpoint rule, follow the pointer, and release. `BindSliderBounds` is `SliderIndicator::on_prepaint` — the rail reports its own box, so the value maps against the rail rather than the taller box that catches the press. `on_mouse_up_out` did not need to cross: every slider the frame painted is asked on any release, which is what Rust's up/up-out pair achieves. Both slider pages answer a drag now, which neither did before. `tests/SliderTests.cpp` ports three of the four Rust cases — the fourth asserts a panic on a logarithmic min <= 0, which this nudges to a usable pair instead — and adds the position, clamp, step, range-ordering, midpoint and release cases.
- 2026-08-19: The input is Rust's. `crates/base/src/input/base` is nine modules; the port had one 512-byte `LineInput` whose cursor was pinned to the end of the buffer, and ~20 lines inlined in `window_common.cpp` that handled ASCII 32..126 and backspace — arrow keys only paused the blink. Each Rust module now has a C++ counterpart, all of them in `src/base/input.cpp` — the `input/` directory is one file here: `cursor.rs` is `Selection`, `rope_ext.rs` is `RopePoint`, `Bias` and the `RopeExt` methods as functions over a `Str` (because an input holds a form field or a page of code and the piece table a rope buys has nothing to do), `mask_pattern.rs` is `MaskPattern` / `MaskToken` (both the pattern and the number kinds, with the token derived from its pattern character instead of parsed into a `Vec` up front), `change.rs` + `undo_manager.rs` are `Change`, `UndoTransaction` and `EditIntent` (the coalescing rules, where Rust's `Vec<Change>` inside a `Vec<UndoTransaction>` becomes a hand-managed array because `Vec<T>` here is memcpy-only), `kind.rs` and `mode.rs` are `InputKind` and `LayoutMode`, and `state.rs` + `movement.rs` + `selection.rs` are `InputState` and its actions. `LineInput` is gone; `InputState` is Rust's name and Rust's shape — a document, a `Selection` with a reversed flag and a pinned word range, an `UndoManager`, a `MaskPattern`, a placeholder, disabled / readonly / masked / clean-on-escape / submit-on-enter, and the blink handle. Rust's `InputBaseState<M>` is generic over a mode marker so a method that makes no sense for a single-line field does not exist on it; there is nothing to bound on here, so the marker is a runtime `InputKind` and those methods return early. The action table is the other half: `state.rs::init` binds 36 key chords, and `InputActionForKey` + `InputPerform` are that keymap and that dispatch, so the window turns a chord into an `InputAction` and hands it to the focused field the way GPUI dispatches an action through the focus chain. Every edit goes through `InputReplaceTextInRange`, which is `replace_text_in_range` down to the order of its steps: normalize, splice, validate, re-mask, record the change, move the caret, emit. A press focuses the field and places the caret, a drag extends the selection, a double click takes the word and a triple the line — `El::BindInput` names the state the way `BindSlider` does, and `PaintCtx::inputs` is a list of its own rather than a hit rect, so the editor's mouse handling does not shadow the click on the box around it, which is how Rust installs both. The caret and the selection are quads the run paints over itself (`El::Caret`, `El::SelRange`) rather than a 2 px bar wedged between two text elements, which used to shift the glyphs beside it every time it blinked; `InputFocus` / `InputBlur` start and stop the cursor, as `on_focus` / `on_blur` do. `component::Textarea` and `component::Highlighter` bind to a state instead of a borrowed `const char*`, so the showcase textarea and editor pages and the story's five textareas are editable rather than painted, and ~150 lines of hand-rolled buffer editing came out of `examples/showcase/showcase.cpp`. `ClipboardGetText` is the new platform seam behind paste — a read on Windows and macOS, a `XConvertSelection` round trip with a half-second deadline on X11. Four test files port every Rust `#[test]` in those modules that is not a `#[gpui::test]`: `RopeTests.cpp`, `MaskPatternTests.cpp`, `UndoManagerTests.cpp` and `InputStateTests.cpp`, 1553 checks in all. What is deliberately left is listed under the gaps above: the IME marked range, the display map, the language-server and code-editor features, the search session, `scroll_to`, syntax highlighting and number stepping.
- 2026-08-19: Eight more of `crates/ui/src` answer their mouse and keyboard the way Rust does, one commit each, easiest first. **breadcrumb**: a level is an element with its own click and disabled flag, not an index into one trail-wide handler. **clipboard**: the button writes the clipboard itself, shows a checkmark for two seconds, and declines a second click while it does. **rating**: a keyed state, a hover that previews a value, and a click on the star already reached that gives it up. **stepper**: StepperItem owns its trigger, its icon and its content; the connector is absolutely placed between indicators, which needed `left_0().right_0()` stretch and `El::LeftRel` / `RightRel` in the layout. **sidebar**: the real Sidebar / Group / Menu / MenuItem, with the submenu open state keyed on the item and click_to_open / click_to_toggle; both hand-built sidebars in the tree were rewritten onto it. **list**: `ListActionForKey` and the wrapping walk, a click that selects and confirms at once, and sections. **menu**: `PopupMenuActionForKey` with its six chords, the walk that steps over separators, submenus, and a right press that opens a context menu where it landed — which needed `MouseDownEvent::el`. **data_table**: the selection, the sort cycle and the ten key chords, with the story sorting its own rows from the event. Along the way: `Side`, the sidebar_accent / sidebar_border tokens, the four panel icons, and four test files (`ListTests`, `PopupMenuTests`, `DataTableTests` plus the existing suite) — 1905 checks.
- 2026-08-20: The drag payload, and a table column that resizes by dragging its edge. `El::OnDrag(kind, ix)` is `on_drag(payload, ..)` — a press picks the payload up and every move until the release carries it back through `El::OnDragMove` as a `DragMoveEvent`, which is Rust's `DragMoveEvent<T>` with a name where the type would be. `El::OnMouseUpOut` is `on_mouse_up_out`: the release an element did not get, which is how a drag that ended somewhere else is heard. `TableState` now owns the column widths (seeded from what the caller declared, `min_width` 20 and no ceiling, `col_resizable`), `resize_cols` clamps and only notifies when the clamp let something through, and the release emits `TableEvent::ColumnWidthsChanged` with every width. The handle is Rust's: two pixels straddling the column's right edge, a one-pixel line down it, and `cursor_col_resize` — which needed a third `CursorKind` and the shape an element asks for to reach the hit rect. One fix fell out of it: truncated text is clipped by the box at paint, since a run that does not wrap is cached without one and the shaped run could not do the cutting.
- 2026-08-20: The dock. `crates/ui/src/dock` is a tree — `DockItem` is a tab group or a split of other items, and `DockArea` holds one in its centre with a fixed `Dock` on the left, the right and the bottom. Rust builds it out of entities holding `Arc<dyn PanelView>`; there is no dyn here, so a panel is a title and a function that renders it, and the tree is `DockNode`s naming each other by index (`src/base/dock.h`). What the tree does is `src/base/dock.cpp`: `split_placement_at` is `DockDropAt`, its five zones at 35% and 65% of each side; `DropPlaceholderBounds::for_placement` is `DockDropPlaceholder`; a drop merges into the target's tabs or splits it, joining the parent split when it already runs that way and nesting a new one when it does not; `remove_self_if_empty` is `DockPrune`, so an emptied group leaves its split and a split down to one child becomes that child — except at a root, where the Dock keeps its empty group the way Rust keeps a `closable = false` TabPanel. `src/ui/dock.cpp` renders it: a tab bar per group with close, zoom and the three toggle buttons, a four-pixel handle between every pair of split children and along each Dock's inner edge, and the drop placeholder over the half a release would take. Dropping is the seam this needed: `El::OnDrop(kind, ..)` is `on_drop::<T>`, matched by a named kind rather than by type, `WindowActiveDrag` / `WindowDragOverId` are `window.active_drag` and what `drag_over::<T>` consults, and `El::BoundsOut` is the element Rust uses to keep `DockArea::bounds` — last frame's boxes are what a drop zone is worked out against. `CursorKind::RowResize` is the third cursor. `tests/DockTests.cpp` ports `drop_placeholder_bounds_cover_each_target_placement` and pins the merge, the two splits, the pruning and the locked dock; 1958 checks.
- 2026-08-20: The tree is virtualized, and the virtual list has a scroll handle. `crates/base/src/tree.rs` was three pure functions here — the key table and the selection wrap — with the themed layer holding sixteen nodes and walking them itself. It is now a `TreeState`: the items in one array naming their parent (Rust's `TreeItem` holds its children and shares its expanded flag through an `Rc`, which is the same tree without the reference counting), `entries` as the flattened list `add_entry` builds, selection, the right-clicked row, and `TreeEvent::Expanded` / `Collapsed`. `toggle_expand` rebuilds the entries, so a collapsed folder's descendants are not in the list at all — which is what makes the row count the tree's own — and `expand_ancestors` is `TreeRevealItem`. `crates/ui/src/tree.rs` renders it through `uniform_list`: only the rows the viewport can show are built, with a spacer at each end standing in for the rest, and `component::Tree` binds to the state rather than taking a node list. The virtual list gained the other half of `VirtualListScrollHandle`: `VirtualListScrollTo` is `scroll_to_item`, where `ScrollStrategy::Center` centres the row and Top and Bottom both scroll as little as they can — Rust matches only `Center` and lets the rest fall into one branch — and `VirtualListVisibleRows` is the same visible range worked out by division for a list whose rows are all one size, so a hundred thousand rows cost nothing to skip. The themed `component::VirtualList` uses it and grew the trailing spacer it was missing, without which the list scrolled only as far as the last row it built. The story's tree page is the working directory two levels deep, with reveal, the arrow keys and the expand events; 2000 checks.
- 2026-08-20: Tabs are five variants, not one. `crates/ui/src/tab` is a `Tab` and the `TabBar` that hands each one the variant, the size and its index; the port had eight labels and a two-pixel underline, and the story hand-rolled the pill, outline and segmented bars out of divs beside it. `component::Tabs` is the bar, a tab is a `TabItem` in its list, and the tables from `impl TabVariant` are functions of their own — height, inner height, padding, margins, the bar's gap and padding, and the three radii — so the look is the numbers Rust wrote rather than numbers that resemble them. The four states are `TabNormal` / `TabHovered` / `TabSelected` / `TabDisabled`, each a port of the match it came from: the folder tab's first left border is dropped, the segmented one paints the selected chip on the inner box so the strip keeps its own background, and a capped bar truncates the label because the label is the only part allowed to give way. Icons, disabled tabs and a bar prefix and suffix are there; the sliding indicator is not — it animates what the selected style already paints, and Rust suppresses the selected visuals only while it is in flight. `tests/TabTests.cpp` reads every table back; 2046 checks.
- 2026-08-20: Scrolling has a second axis, and the wheel goes to the box under the pointer. `crates/ui/src/scroll` is `ScrollbarAxis::{Vertical, Horizontal, Both}` over one piece of arithmetic that reads whichever pair of numbers the axis names; the port had the arithmetic but only ever ran it down the right-hand edge. `Overflow` is now per axis (`overflowX` beside `overflowY`, which is what `overflow_x_scroll` and `overflow_y_scroll` are), `El::ScrollX` and `El::ClipX` are the other half of `ScrollY` and `ClipY`, layout measures a sideways-scrolling box's children unconstrained across and slides them by `scrollX`, and the horizontal thumb is painted, pressed and dragged through the same `ScrollbarThumbSize` / `ThumbPos` / `OffsetForTrackPress` / `OffsetForDrag` the vertical one uses. A scrolled axis is no longer stretched to: `align: stretch` made a scrolled box's content exactly as big as the box, so a row that scrolled down had nothing to scroll. `ScrollbarMode` is `Always` or `Hover`; Rust's default `Scrolling` fades out after two idle seconds and needs an animation clock per area, so it is not ported. The wheel now goes to the scrolled box under the pointer — `WindowOnScrollWheel` is the fallback for a box that asked for nothing — so a view binds `ScrollId` + `OnScroll` and the runtime does what every page used to hand-roll; the story's two panes lost their wheel handler and gained working scrollbars, and the scrollbar page's list is virtualized with a second frame that scrolls both ways.
- 2026-08-20: The list is a delegate over a virtual list. `crates/ui/src/list` is a `ListState` and a `ListDelegate` whose `render_item` is asked only for the rows on screen, with `cache.rs` flattening the sections into rows — a header, the items, a footer, and nothing at all for a section with none. The port had selection and the key table but built every row eagerly and knew nothing about sections beyond a header and a footer element. `ListState` now holds the section counts and answers `ListRowCount` / `ListRowAt` / `ListRowOfEntry`, which is `RowsCache` worked out by walking at most sixteen sections rather than kept in a vector; `component::List` takes the delegate as the caller's data plus three render functions (an element carries no closure), builds only the visible range over `VirtualListVisibleRows` with a spacer at each end, and has `render_empty` — a muted Inbox icon — the loading skeletons, and `load_more` once the last row built is within `load_more_threshold` of the end. Arrow keys scroll the selection into view, which is `scroll_to_selected_item`. `component::SearchableList` and the story's list page moved onto the delegate; 2072 checks.
- 2026-08-20: The table moves its columns and virtualizes its rows. `crates/ui/src/table` keeps the column order in `col_groups` and rewrites it on a head drag; the port drew the caller's array in the caller's order and built every row. `TableState` now holds `colOrder` — display position to the caller's column, which every render and hit test goes through — with `move_column` as `TableMoveColumn` and `drag_gap_at` as `TableDragGapAt`, the gap after the last column whose centre is left of the pointer, and no gap at all where the column already is. A head takes `kTableColDrag` and the whole head cell is a drop target, so the seam the dock uses is what carries a column too; the gap it would land in is drawn down that column's edge. The body is virtualized when the caller gives it a height (`DataTable::H`), over `VirtualListVisibleRows` with a spacer at each end, and `TableScrollToRow` is `scroll_to_row` — the story's Go To menu uses it and its table is the five thousand rows the toolbar always claimed. `render_empty`, the loading skeletons and `load_more_if_need` are there too; 2097 checks.
- 2026-08-20: Notifications stack. `crates/ui/src/notification.rs` is a `NotificationList` that Root renders in a corner of the window: notifications pushed into it, keyed so the same one replaces rather than repeating, auto-hidden on the toast clock, capped at ten and stacked the way Sonner stacks — three visible layers peeking fourteen apart, each a little narrower, opening out into a list while the pointer is over them. The port had the card and the toast lifecycle but nothing that held more than one. `ToastMotion::sonner`'s numbers and `stack_geometry` are in `src/base/toast.*` — the collapsed and expanded heights and every card's offset either way, for a stack anchored at the top or at the bottom — and `component::NotificationListState` is the list itself, advanced by a 50 ms window timer the way Rust advances it from a spawned task. The story page pushes into it instead of rendering a card inline per section. One runtime bug fell out: a `Fixed` element was placed against the window and then dragged along when an ancestor was slid into place, so anything fixed inside a page landed at the page's origin — `TranslateSubtree` now leaves a fixed subtree where it is, which is also what finally puts the dialog a tenth of the window down from the top rather than a tenth of the page. 2113 checks.
- 2026-08-20: The inspector. `crates/ui/src/inspector.rs` is GPUI's devtools panel: Ctrl+Shift+I opens it, the magnifier picks an element out of the window, and the panel shows where that element came from and lets its style be edited live as Rust or JSON through an editor with LSP completions. The last part needs `#[track_caller]` source locations and a style reflection table this tree does not have, so what is ported is the panel and the picking. `WindowToggleInspector` / `WindowInspectorPick` are `window.toggle_inspector` and `Inspector::is_picking`; while picking, every box under the pointer is written down as it paints, so the deepest — the one a click would land on — is the one left standing, and it is highlighted the way GPUI highlights it. A press settles the pick against the next frame rather than the last one, because the frame that painted last was aimed at wherever the pointer was then. `component::Inspector` is the panel, docked on the right off the story's root the way Rust hangs it off Root: the magnifier, the close button, and what the element can say for itself — its kind, its id, its box, its depth, and the style it was built with.
- 2026-08-20: Settings is a component, not a page. `crates/ui/src/setting` is a sidebar of pages, each page a list of groups and each group a list of items — a title, a description and the field that changes it — with a search box at the top of the sidebar that filters the whole tree by what an item says. The port was a title and twelve label-and-control rows, and the story hand-rolled the sidebar, the group cards and the rows beside it. `component::Settings` is the tree, built fluently (`Page` / `Group` / `Item`, then `Keywords` / `Disabled` / `Resettable` / `Layout` for the item just added), and `is_match` / `filtered_pages` are `SettingItemMatches` / `SettingGroupMatches` / `SettingPageMatches` — the query against the title, the description and the keywords, lowercased, with a group dropped when everything in it fell out and a page dropped when every group did. The fields stay the caller's: Rust builds them from the type of the value behind the setting, which needs a reflection table this tree has no use for. `tests/SettingTests.cpp` pins the matching; 2131 checks.
- 2026-08-20: The searchable list is the machinery, not a façade. `crates/ui/src/searchable_list` is what a Select and a ComboBox are built on: a query, items in sections, one or many selected, and a click worked out into atomic `SearchableListChange`s that the delegate applies. The port was a 32-string list with a substring filter. `component::SearchableListState` now holds the list's own selection (as indices into the caller's items), the mode, the matches the query left and whether it is open; `SearchableListChangesFor` is the mode strategy — Single deselects everything and selects the one clicked, Multi toggles it, both by the item's *value*, which is what the check beside a row goes by — and `SearchableListApply` is `on_will_change`'s default, where a Select of a value already selected changes nothing and a Deselect takes out whatever carries that value before falling back to the index. `component::SearchableList` renders it: the query field, section headings for the sections the query left something in, disabled rows, the trailing check, and the empty state. A story page drives a single and a multiple list. What is still its own is `Select` and `Combobox`, which keep their own selection rather than going through this. 2149 checks.
- 2026-08-20: Select and ComboBox are the searchable list. Rust's `Select` is a trigger over a `SearchableList` and its `ComboBox` is a `Select` that is always searchable — both forward the items, the query and the selection to the same state. The port had three separate widgets, each with its own option array and its own idea of what was picked. `component::Select` now takes an `Entity<SearchableListState>` and the caller's `SearchableItem` array, renders its dropdown through `component::SearchableList`, and gets sections, disabled rows, multi-select, the trailing check and the empty state from it; `component::Combobox` builds a `Select` with a query field and forwards everything, which is what `Combobox::render` does. `SearchableListState` keeps the items it was last shown so a click can work out what it changed, so `SearchableListClick` applies the change and closes a single-select list — no caller has to. `SelectTriggerTitle`, `SelectToggleOpen` and `SelectClear` are the three things a trigger asks of the state. The story pages hold one list per select and read the selection back out of it.
- 2026-08-20: The menu module is four things, not one. `crates/ui/src/menu` is `popup_menu.rs` — which was ported — plus `dropdown_menu.rs`, `context_menu.rs` and `app_menu_bar.rs`, which were not: the story hand-rolled a trigger with an open flag and a deferred menu under it, and a right press that wrote the position into the page's own fields. `component::DropdownMenu` is the trigger and the menu hanging off it (`Anchor::TopLeft` or `TopRight`), `component::ContextMenu` wraps an element whose right press opens the menu where the pointer is — the position lives on `PopupMenuState` now, which is where Rust's `ContextMenuState` keeps it — and `component::AppMenuBar` is the row of titles with one open at a time, switching as the pointer moves across it and walking with the arrows (`on_move_left` / `on_move_right`, both wrapping, neither doing anything while nothing is open). The story page drives all three. 2156 checks.
- 2026-08-20: The sidebar's layout is a function with Rust's own tests behind it. `SidebarLayout::new` in `crates/ui/src/sidebar/mod.rs` is what a collapsible mode and a collapsed flag come to — which rendering the rows take, what the wrapper does with its width, and which end the content is pinned to while that width changes — and it is the one part of the sidebar Rust unit-tests. It was inline in `Sidebar::IntoEl` here and missing `align_child_to_end` entirely; it is now `SidebarLayoutFor`, with `tests/SidebarTests.cpp` a port of all five Rust cases. `SidebarMenuItem::ContextMenu` is `context_menu(..)`, which the menu work just made possible: a right press on a row opens the caller's menu where the pointer is, and the story's Sales and Marketing row has one. 2183 checks.
- 2026-08-20: The caret takes the view with it. `InputState::scroll_to` is what keeps a caret on screen in Rust, and it was one of the pieces this port had left out: an editor painted from its first line whatever the caret was doing, and the wheel over it scrolled the page behind it instead. `InputScrollToCaret` is that function — both axes, a line's clearance from either edge, the margin Rust keeps past the caret sideways, the clamp into the content, and the rule that a move up is never answered by scrolling down (nor the other way), so a vertical walk does not fight itself. The field's box, the caret's x and the scrolled height are written at paint the way Rust reads them off `last_layout`, `InputMoveTo` calls it, and `component::Textarea` and `component::Highlighter` scroll their rows by what it answers. A multi-line field under the pointer takes the wheel before anything around it. `NumberStepForKey` is the other half of `number_input.rs`'s key context — up increments, down decrements — which the story's number page now honours. 2199 checks.
- 2026-08-20: The window border is a rule, not a band. `crates/ui/src/window_border.rs` is what a client-decorated window draws around itself — shadow padding, a one-pixel frame inside it, and the band a press counts as a resize — with `client_frame_insets` and `resize_edge` deciding all of it and a tiled side getting none of it. The port was a one-pixel border and nothing else, and the X11 window had a six-pixel band of its own with the rule written a second time. `WindowBorderInsets` and `WindowResizeEdge` are those two functions, answering the directions `_NET_WM_MOVERESIZE` numbers so the X11 window can pass one straight through — which it now does instead of its own copy. `component::WindowBorder` draws the padding and the frame, dimmed while the window is not the active one (`Window::active`, which the three platforms now set from WM_ACTIVATE, FocusIn/FocusOut and windowDidBecomeKey), and `component::Root::Bordered` wraps its view in one, which is what `Root::bordered(true)` does. Two things the Linux build caught on the way: `El::Bounds*` needed qualifying for g++, and the story's tree page was building a path into a buffer g++ could prove too small. 2228 checks, and every example builds on Linux as well.
- 2026-08-20: A keystroke is spelled by the platform, not by the caller. `Kbd::format` in `crates/ui/src/kbd.rs` turns a keystroke into what that platform writes on a key cap: the modifiers in its own order — ⌃⌥⇧⌘ run together on macOS, Ctrl+Alt+Shift+Win joined with a plus everywhere else — then the key, named where it has a name (Esc, Backspace, Page Down, the four arrows) and capitalised where it does not. The port took a string the caller had already spelled, so every menu carried a shortcut that only read right on Windows. `component::Keystroke` and `KbdFormat` are that function, `Kbd::New` takes one, and the story's kbd and menu pages hand it keystrokes rather than strings. `tests/KbdTests.cpp` pins the order, the names, the capitalising and a buffer too small to hold the answer. 2246 checks.
- 2026-08-20: The form's fields carry the rest of what a Field is. `crates/ui/src/form/field.rs` has more per-field knobs than the port had: `visible(false)` leaves a field out without disturbing the ones around it, `label_indent(false)` drops the label column so the control starts at the form's edge, `items_start` / `items_center` / `items_end` line the label up against a tall control, and `label_fn` / `description_fn` let the caller build either. The gaps come from the form's size — eight at Large, four otherwise, halved between the parts of a vertical field — and `label_text_size` sets the label's own. All of that is in `component::Form` now, and the story's label-less rows say `LabelIndent(false)` rather than passing an empty label and hoping.
- 2026-08-20: Five charts, not two. `crates/ui/src/chart` is area, bar, candlestick, line, pie, radar and sankey; the port had area and pie. `ChartSeries` now names which one it is and the paint branches on it, over the same axis, grid and labels: a bar per band, a candle with its wick and a body colored by which way it closed, a line with nothing under it, and a radar's rings, spokes and closed shape. The y domain is Rust's — `ScaleLinear` over the data with zero chained in, so a bar or an area is read against its baseline while a line or a candle keeps the extent of its own values — which also means the system monitor's charts now scale to what they are showing rather than to a hardcoded 0..100. `ScaleBand` is the other missing scale, ported with Rust's own tests: the band width capped at thirty, the inner and outer padding, and the band a position falls in. What is still not ported is `sankey_chart.rs`. 2267 checks.
- 2026-08-20: A chart tells you what you are pointing at. `crates/ui/src/plot` has a tooltip and a crosshair hanging off `AreaChart::id` and the plot's hover state; the port drew the series and nothing else. `component::PlotTooltipPlace` is the placement rule — the box hugs the cursor, flips toward the middle past the halfway line on each axis, and clamps to the plot for a box too big to hold either way — and `AreaChart`, `LineChart` and `BarChart` each got `Tooltip(name)`. A chart that asked for one and has the pointer inside it draws a dashed hairline down the plot at the index under the cursor, a dot on that value, and a box with the label and the value; bar and candlestick pick the index by `ScaleBand::LeastIndex`, the rest by the nearest point. `kPlotAxisGap`, `kPlotTextSize` and `kPlotTextGap` are the constants from `plot/label.rs`. 2272 checks.
- 2026-08-20: The OS draws the menu. `crates/ui/src/native_menu` is a menu the operating system renders, which is what lets it extend past a window too small to hold it; the port had a façade that handed back a drawn `PopupMenu` and nothing else. `component::NativeMenu` is the builder — `Menu`, `MenuWithDisabled`, `MenuWithCheck`, `MenuWithIcon`, `Separator`, `Submenu`, `IsEmpty` — and `Show(x, y)` opens it where the press was. The rows are numbered the way Rust numbers its `actions` vector: 1-based preorder over what can be chosen, skipping separators, submenu rows and greyed rows, so the id the OS answers with maps back to the row the caller built and `onSelect` reports the caller's own id. The platform seam is `PlatHasMenu` and `PlatShowMenu` over `PlatMenuItem`: Windows builds an `HMENU` and runs `TrackPopupMenuEx` with `TPM_RETURNCMD` (including the three uxtheme ordinals that make a menu follow a dark theme), macOS builds an `NSMenu` and runs `popUpMenuPositioningItem`, and X11 has no menu of its own and answers false — which sends the caller to `IntoPopupMenu`, the same rows drawn, which is Rust's `FallbackMenuOverlay`. Icons are carried on the row and shown by the drawn fallback; a real OS menu wants a bitmap of one, which this port does not build. Verified end to end on Windows: a press opens the real menu, and Down Down Enter through it reports the row's id back to the page. 2301 checks.
- 2026-08-20: Tiles, the other dock layout. `crates/ui/src/dock/tiles.rs` is 1445 lines the port did not have at all: panels that float over an area rather than splitting it, each moved by the bar across its top and resized by any of its edges. `TilesState` in `src/base/tiles.h` holds where each tile sits, which one is being moved or resized, and the history that undoes it; `component::Tiles` draws them in z-order with a drag bar and five grab strips, over the same drag seam the dock's resize handles use. Both kinds of drag are magnetic, and Rust's own tests are ported with them: `TileSnapEdge` takes the nearest neighbour edge strictly inside the grid size, `TileComputeResizedBounds` reads which edge moves off which of the four values it was given and pins the opposite one, and an edge with nothing close rounds onto the 8px grid instead. A move snaps to the top and left of the area before it looks at any neighbour, keeps 64px of the tile reachable, and stays off the grid until the release, which is what makes the drag itself smooth. The tile that was dragged comes to the front, which reorders the list — so a tile names its panel rather than being it, and the elements on it are keyed by the panel so nothing is renumbered underneath the pointer. What is still not ported is the tiles' scrollbar and the serialised `TileMeta` layout. 2353 checks.
- 2026-08-20: Root holds the layers, and the story sits inside one. `crates/ui/src/root.rs` is the window's outermost view: the page, and over it the notifications, the one open sheet and the stack of dialogs. The port's `component::Root` was a background and a border and nothing else, and no example used it. It now takes the layers — `Notifications`, `Sheet(placement, size)` and one `Dialog` call per open dialog — and carries Rust's two rules about them, both tested: `RootDialogOverlayIndex` is `render_dialog_layer`'s `show_overlay_ix`, the last dialog that asked for an overlay, so a stack of them tints the page once rather than once each; and `RootNotificationInsets` is `render_notification_layer`'s margin, the room an open sheet takes on its own edge, so a sheet on the right pushes the notifications left instead of covering them. `ShadowSize` is `window_shadow_size`. The story's window is a Root now, bordered only where the window is client-decorated. The Tab and Shift+Tab half of root.rs was already ported — `FocusNext` with its trap — and the layers stay the caller's elements rather than entities Root owns, which is what this port does everywhere. 2367 checks.
- 2026-08-20: The virtual list has a handle. `VirtualListScrollHandle` in `crates/base/src/virtual_list.rs` is what a caller outside the list holds: the axis and the item count, the base handle's offset, viewport and content size, and — the part that matters — a `deferred_scroll_to_item` that waits until the list is next laid out, because at the moment a button asks to scroll to row 50 nothing knows where row 50 is. The port had the geometry (`VirtualListVisibleRange`, `VirtualListScrollTo*`) and callers that applied it inline because they already knew their row height; the handle itself was missing. `VirtualListHandleLayout` is Rust's prepaint: it takes the item count and viewport the list measured, answers the pending request against them, and clamps the offset to the list, so a list that shrank under a scrolled view comes back. `scroll_to_item_with_offset` and `scroll_to_bottom` come with it (the latter is the last item at the top of the view, with `saturating_sub` for an empty list). `component::VirtualList` takes a handle, variable item sizes, an id and the wheel listener, and the story's VirtualList page is a real virtualised list now — 500 rows over the standard dataset and 50,000 over the stress one, with the four scroll buttons going through the handle and the visible range reported off it. 2387 checks.
- 2026-08-20: The sankey. `crates/ui/src/plot/shape/sankey.rs` is d3-sankey ported to Rust, and it was the one chart shape the port did not have. `src/base/sankey.h` is the layout generator: the topology pass (a node's throughput, its longest path from any source and to any sink, and the column each of the four alignments puts it in), then the placement pass — the value-to-pixel scale the most crowded column allows, six relaxation rounds that pull every node towards the weighted centre of what flows into and out of it, d3's middle-out collision resolution, and the per-end ribbon widths that leave both sides of every node exactly covered even where the graph does not balance. Rust's own centring and stagger are here too: each column is translated to the middle of the extent, and a run of equal single-node columns is nudged off the centre line so the ribbon between them curves rather than running flat. Where Rust hangs a `Vec` of link indices off every node, the lists here are one array with a slice per node, so a graph is four allocations and the fifty-thousand-node chain in Rust's own test still resolves in one pass. All twelve of that module's tests are ported. `component::SankeyChart` is `sankey_chart.rs`: the two-pass flow (topology, measure the label margins against it, then place on what is left), ribbons drawn as horizontal cubics filled from the colour they leave to the colour they arrive at, and the labels beside the first and last columns and above the middle ones. `PathFillGradient` is new on all three backends — the vertical-only one is now written in terms of it — because a ribbon's two ends are side by side. 2571 checks.
- 2026-08-20: A native menu shows its icons. Rust rasterizes a menu item's icon and hands the OS a bitmap of it — an `HBITMAP` set as `MENUITEMINFOW::hbmpItem` on Windows, a template `NSImage` on macOS — and the port carried the icon on the row and then dropped it, so only the drawn fallback ever showed one. The seam that was missing is an offscreen paint target: `PaintTargetBeginOffscreen` / `PaintTargetEndOffscreen` on all three backends (a premultiplied DC render target over a DIB section on Windows, a cairo image surface on Linux, a flipped `CGBitmapContext` on macOS), and `SvgRasterize` over it, which draws an icon into a square of premultiplied BGRA instead of onto a window. `PlatMenuItem` carries the icon's asset path now: Windows builds a 32-bit top-down DIB per icon at the device pixel size — sixteen points times the window's scale, which is Rust's `MENU_IMAGE_SIZE` — in the colour the menu writes its text in, attaches it by position, and frees the bitmaps once the menu has been destroyed; macOS builds a 2x `NSImage` and marks it a template, which is what makes AppKit tint it with the row. Verified on Windows against the real menu window: New, Open… and Quit show their glyphs, alpha-blended over the menu background. 2571 checks.
- 2026-08-20: The tiles area scrolls. Rust's `Tiles::render` folds the panels' bounds into a scroll size and hands it to a `Scrollbar` over the whole area, with `set_scrollbar_mode` choosing whether the bars are always there; the port drew the tiles into a clipped box and left a tile dragged past an edge unreachable. `TilesContentSize` is that fold — from the leftmost and topmost edge any tile reaches, never past the origin, to the furthest right and bottom — and the area now scrolls over it on both axes, with a sizing child standing in for the tiles, which are all out of flow and so measure as nothing. The scroll is part of the coordinate system too: the pointer is read into the content's coordinates rather than the view's, so a drag or a resize in a scrolled area lands where the pointer is. 2580 checks.
- 2026-08-20: A dock layout can be written down. `crates/ui/src/dock/state.rs` persists a DockArea — the centre item, the three docks, and for every node what kind it is: a split with its sizes and axis, a tab group with its active index, a leaf panel, or a set of tiles with a `TileMeta` each. None of it was ported, and the tiles' metas were the piece the tiles port was missing. Rust reaches for serde; the port grew `src/base/json.h`, a small reader and writer — a document parsed into arena nodes with the escapes undone, and a builder that keeps track of where a comma is due — and `src/base/dock_state.h` over it. `TileMeta`'s default is Rust's, a 200x200 box ten pixels in. `PanelState` nests by ownership in Rust and by index here, the way the dock's own tree already does. The tiles' half is `TilesToMetas` / `TilesFromMetas`, which carry the panel each meta belongs to beside it: the tiles are reordered as they come to the front, so a meta on its own does not say which panel it is for, and a restore after a drag would otherwise put every panel in its neighbour's box. Rust's `test_deserialize_item_state` is ported against the same layout its fixture holds, along with the round trip and the JSON reader's own cases. The tiles story saves and restores its layout through it — 494 bytes of JSON, and every panel goes back where it was after a drag that reordered them. 2683 checks.
- 2026-08-20: Ported `crates/base/src/focus_trap.rs`. A trap is now a property of the container, the way Rust hangs it off the one focus handle a dialog tracks: `FocusCollect` carries `El::TrapId` down the tree onto every focusable below it, so a dialog sets one id instead of every button inside it setting its own. `src/base/focus_trap.cpp` is the other half of what Rust's global FocusTrapManager answers — `FocusTrapActive` (find_active_trap: the trap the focused element sits in), `FocusTrapOf`, `FocusTrapTab` (Root::on_action_tab, which reads the trap before it moves so Tab off the end comes back to the trap's start rather than the page), and `FocusTrapEnter`/`FocusTrapArm`/`FocusTrapApplyPending` for the dialog that has just opened and has to take focus into itself — armed while the tree builds, settled after the frame's FocusCollect, since the focusables are not known until then. `Dialog`, `AlertDialog` and `Sheet` trap their popup and surface, each under a name it can override for a stack. The focus ring the runtime already painted for trapped elements now has something to draw for. One bug fell out of it: `component::Button` passed its click id where the unstyled Button takes `disabled`, so every enabled button was non-focusable and every disabled one had a focus handle — exactly backwards.
- 2026-08-20: Ported `crates/base/src/list_settings.rs`. `ListSettings::active_highlight` is what a selected row asks before it paints: on (the default), it takes the translucent `list.active` tint with a `list.active.border` rule drawn over its own box, so nothing moves; off, it is the plain `accent` block the port has always drawn. The theme grew the four tokens the setting picks between — `listActive` / `listActiveBorder` from default-theme.json (`#bfdbfe33` over `#60a5fa` light, `#1e40af33` over `#1d4ed8` dark) and the `tableActive` pair, which schema.rs falls back to the list's, so they are the same colors here. Rust keeps the settings on the theme and mutates it through `Theme::global_mut`; the themes here are immutable statics, so it sits beside the theme mode as one global. `component::ListItem` and the table's selected row read it, and the list story's Options dropdown toggles it the way the Rust story's settings menu does.
- 2026-08-20: Ported `crates/base/src/state_style.rs`. A semantic state — checked, pressed, selected, focused, disabled — says something about the few fields it names and nothing about the rest, and `resolve_style` lays them down in one fixed order so no control decides the order for itself: the instance style, then the value states, then disabled last. GPUI has StyleRefinement for the partial half; the Style here is a whole style, so `StateStyle` is a Style plus the set of fields that were named, and only the fields a semantic state overrides are refinable (background, foreground, border, radius, and the two hover colors) — the rest of a control's look is layout, and no state moves a control. `component::Button` grew the ButtonStyles pair, `SelectedStyle` and `DisabledStyle`, resolved in that order over what the variant computed, which is what `Button::resolved_style` does in Rust. The four tests from state_style.rs and the two priority tests button.rs keeps for it are ported.
- 2026-08-20: Ported `crates/base/src/macos_accessibility.rs`. VoiceOver hit-tests the window, and AppKit's NSWindow answers for itself rather than asking the view that drew the page, so nothing inside a gpui window is reachable through it. `PlatInstallAccessibilityHitTest` adds an `accessibilityHitTest:` to the window's class that forwards to the content view — `class_addMethod` with the `@@:{CGPoint=dd}` encoding Rust assembles from the same pieces — and `component::Root::New` calls it the way `Root::new` does. The window remembers that it took it, since a Root here is built every frame rather than created once. Windows and X11 have nothing to teach and answer with a no-op. The macOS half compiles nowhere reachable from this machine; CI's macos job is what checks it.
- 2026-08-20: A click fires from the release, which is what GPUI does and what every ported control was written against. `Interactivity` holds the press as `pending_mouse_down` and runs `on_click` from its mouse-up handler; `DispatchMouseDown` now only remembers the press — the id, the button, the count, the modifiers and where it landed — and `DispatchMouseUp` asks `ClickFromRelease` whether to dispatch: a press has to be waiting at all — `pending_mouse_down` being Some, which the scrollbar, the inspector and a non-focusing press each clear by taking the press for themselves — the button coming up has to be the one that went down, the element under the pointer has to be the one that took the press, and no drag may have happened in between. So a reader can press a button, think better of it, slide off and let go without it firing — which was not possible before — and dropping a dock tab back where it came from is a drop rather than also a click. A drag is now the move that leaves the press behind by more than 4 DIPs rather than the press itself, so a press on a draggable tab or tile is still a click when the pointer never travelled. The keyboard half is untouched: Enter and Space on a focused element still build a `ClickEvent::Keyboard` there and then. `WindowOnUnhandledClick` moved with the rest — an outside *press* is still available through `WindowOnMouseDown`, which is what Rust's `on_any_mouse_down` dismissals use. Checked across the gallery: buttons, checkboxes, list rows, dock tabs, tabs, selects and their menus, dialogs and the sidebar all still answer, and tiles still drag.
- 2026-08-20: The keyboard half of the click follows the same rule: Enter and Space arm on the key down and make the click on the key up. `div.rs` keeps `pending_keyboard_down` — the focus generation the activation keystroke went down at — and its key-up listener makes the `ClickEvent::Keyboard` only when that stamp still matches, so focus that moved between the two halves takes the click with it, any other key coming up cancels the press, and a modifier on either half means the keystroke was a shortcut rather than an activation. `WindowKeyUp` is the new platform seam behind it (`WM_KEYUP`, X11 `KeyRelease` with the auto-repeat pair filtered out by peeking the queue, Cocoa `keyUp:`), `WindowSetFocusId` is the one door every focus move goes through so `Window::focusGen` counts them all, and `ClickFromKeyRelease` is the rule itself. Two things the old key-down path got wrong came out with it: a keyboard activation ran only the element's listener and not its `Func0`, so half the controls in the tree could be focused and not activated, and when the focused element had no listener it fell through to the window's unhandled-click handler — the outside click that dismisses an overlay — which is not something a keystroke on a focused element should ever be. `ClickEvent::keyboardKey` is `KeyboardClickEvent::button`, the Enter or Space that made it. `cmd/shot.ts` sends the release with the press now, since `-key=` is a whole keystroke. Four cases in `tests/ClickTests.cpp`; 2766 checks.
- 2026-08-20: The focus ring is Rust's, and it shows outside a dialog. The runtime painted one only for an element inside a focus trap, so Tab in the story gallery moved the focus through 75 controls and nothing on screen changed — the ring was a dialog feature by accident. `styled.rs::focus_ring_style` hangs it off `is_focused` alone, which every control calls when it is focused however the focus arrived, so a button clicked with the mouse shows one as much as one reached with Tab. The trap condition is gone, and the ring is what Rust draws: `FOCUS_RING_WIDTH` of the theme's `ring` colour at `FOCUS_RING_OPACITY` in the three DIPs immediately outside the element's border with the corners widened to match, plus the element's own border tinted with the ring colour — the half of the appearance that costs no room. It was two DIPs of the blue accent before, and no border tint. Every focusable in the tree is a control (button, checkbox, radio, switch, toggle, link, date picker, OTP field, alert-dialog buttons), which is the same set Rust rings; `focus_ring_enabled` — the per-control opt-out nothing in the Rust tree uses — and `Theme::focus_ring` are not ported.
- 2026-08-20: The story's title bar does what `crates/story/src/title_bar.rs` does. Its three buttons were decorative: the Settings2, GitHub and Bell icons had no click handler at all. The GitHub one is `cx.open_url` now, which needed a platform seam — `OpenUrl` is `ShellExecuteW`, `[[NSWorkspace sharedWorkspace] openURL:]`, and a double fork to `xdg-open` on X11 so a browser that takes its time does not hold up the frame. The bell carries `Badge::count` capped at 99, which meant giving the window its notification list: Rust keeps it on the window, where Root renders it and `window.notifications(cx)` reads it, so it moved off the notification page and onto `StoryApp`, and a notification now outlives leaving the page that pushed it. The Settings2 button is `FontSizeSelector`, a `DropdownMenu` anchored top-right over a `PopupMenu` with the labels, separators and right-hand checks Rust builds; the menu reports the row it took and one table says what each row does. Four of its five sections needed something underneath. `ThemeSetRadius` writes `radius` and `radius_lg` on both palettes, following Rust's rule that the large one is two more except at zero. `ThemeSetFontSize` is `Theme::font_size`: the root size every element inherits, and the base an explicit `Font(12)` is measured against — Rust says its sizes in rems so they all follow the theme's, and multiplying at the one place layout resolves a size is the same thing. `ScrollbarModeSet` is `Theme::set_scrollbar_mode`, the default a box that did not name its own gets, which is how Rust's Scrollbar reads the theme unless the caller passed a mode. The list-highlight toggle drives the `ListSettings` global that was already there, and the FPS toggle overlays `FpsMonitorEl` the way `AppState::show_fps_monitor` does. `tests/ThemeSettingsTests.cpp` pins the three rules; 2779 checks.
- 2026-08-20: Motion. `crates/base/src/animation.rs` and `motion.rs` are `src/base/animation.*` and `src/base/motion.*`: the easing curves (`cubic_bezier` and the three cubics, with Rust's own detail that the bezier reads its parameter as the curve's t rather than solving for it from x), the Lerp trait's three implementations, and `motion::transition` — the CSS-like timing policy that answers what a caller-owned value should be *now*, on its way to the target it asked for. The state is window-keyed, which is what `window.use_keyed_state` is here, and a transition still going asks for another frame: `WindowRequestAnimationFrame` is `window.request_animation_frame()`, a one-shot beside the `anim` flag that keeps a window drawing back to back, cleared as each frame starts so something that has arrived stops asking. A frame stamps one `now` that every transition in it measures against, the way Rust's executor clock does not move inside a frame. `cx.reduce_motion()` is a platform seam — `SPI_GETCLIENTAREAANIMATION` on Windows, `accessibilityDisplayShouldReduceMotion` on macOS, false on X11 — and a value that is moving when it comes on adopts its target on the next frame rather than finishing the curve. The rule is a function over the state and the clock (`MotionAdvance`), so `tests/MotionTests.cpp` drives all six of Rust's `#[gpui::test]` cases without a window: the zero duration, the change over time, the reversal that carries on from the value it had reached, the delay that holds, the finish that stops asking, and reduced motion. What is deliberately not ported is `EffectTransition`, which hands GPUI a closure that restyles an element every frame; an element here holds no closures, so a caller reads the value and spells the effect. Three components opt in, which is how Rust puts it — no component installs motion by default: the switch thumb slides over 150 ms, the checkbox tick fades over 250 ms (as its own alpha, since there is no element opacity here), and the sidebar's wrapper takes the width over 200 ms with `ease_in_out_cubic`, clipping a sidebar that keeps its own width — `sidebar_wrapper` plus `EffectTransition::width`, including `render_child`, which keeps an offcanvas sidebar built while it is on its way out. Still on their own clocks or still unanimated: the toast and notification stack (which has one already), the tab indicator, the accordion, the dialog and sheet entrances, the skeleton, the spinner and the indeterminate progress. The story's Appearance menu gained a Reduce Motion row, which Rust's has not: the desktop's setting is the default, and a gallery of components that move is where you would want to see both. 2842 checks.
- 2026-08-20: The rest of the animation, which is every site the last entry left on its own clock. Two primitives came first. `MotionRepeat` is `Animation::repeat()` — a cycle with no target and no end, which is what a spinner, a pulse and an indeterminate bar are made of — and it comes with GPUI's own easings (`quadratic`, its `ease_in_out`, `ease_out_quint`, and `bounce`, which runs a curve forwards and then back). `MotionAppear` is the one-shot `with_animation`: how far into a run an element is, counted from the frame it first appeared in. That one needed the state's lifetime to be GPUI's rather than the window's: a motion slot now lives in its own store and is dropped when a frame does not ask for it, which is what makes a dialog that closes and opens again play its entrance a second time. `El::Rotate` is `Transformation::rotate(percentage(..))`, applied where `SvgDraw` already maps the viewBox onto the element's box, so no backend learned about transforms and only an icon with an asset behind it can turn. What now moves: the **spinner** turns over its `speed` along its easing (the story's three curves are Rust's); the **skeleton** pulses `1 - delta * 0.5` of its alpha over a two-second `bounce(ease_in_out)`, every block on a page in phase; **progress** grew both halves it was missing — a value that changes glides over 0.15 s, and `loading` is the indeterminate sweep, the bar's two eased edges and the circle's two eased ends, with the story's toolbar gaining Rust's value presets and its Loading switch; the **dialog** comes down from the top over a quarter of a second along `cubic_bezier(0.32, 0.72, 0, 1)` while its backdrop fades in; the **sheet** slides a hundred pixels in from its own edge over 0.15 s; the **accordion** opens and shuts on its measured height, the panel staying mounted while it collapses the way `AnimatedAccordionPanel` does, with the natural height kept in a slot the closed item still asks for; the **tab indicator** slides to the selected tab over 200 ms of `ease_in_out_cubic`, and the tab it is heading for holds back its own selected look while it is in flight, which is Rust's rule too; and the **notification stack** transitions its height, each card's offset and each card's inset along CSS `ease` instead of snapping open under the pointer. Two things stay unported for want of element opacity, and say so where they are: the dialog's own fade (the backdrop's is what you see) and the fade of a toast that is past the third in a closed stack. 2856 checks.
- 2026-08-20: Element opacity, which two of the animations were waiting on. `Style::opacity` is GPUI's, and so is the way it travels: the window keeps the opacity in force (`element_opacity`), an element multiplies it by its own while it and everything under it paints and puts it back afterwards, and every colour handed to the backend is faded by it — at the moment the primitive is drawn, not when the style was built, which is what makes a whole subtree fade as one thing instead of each of its boxes separately. Nested opacities multiply, as Rust's do. It cost one line per backend: every colour already passes through `Brush` on Direct2D, `SetColor` on cairo and `SetFill`/`SetStroke` on Core Graphics, with the gradient stops the only other seam. What it bought: the **dialog**'s own fade — the backdrop and the panel arriving together as one layer, which is `with_animation("fade-in", .., |this, delta| this.opacity(delta))` — and the **toast** stack's `"visibility"` transition, so a card pushed past the third in a closed stack fades out rather than vanishing (one better than Rust there: a card that has finished fading is left out of the tree, where Rust keeps it at zero opacity and in the way of the pointer). The **checkbox** tick and the **skeleton** pulse moved off their per-colour alphas onto the element's, which is what Rust writes. Three places that had never had it are Rust's too, and none of them are animations: a **loading button** is the whole button at 0.8 — Rust says why, that fading `bg`, `border` and `fg` one at a time shows nothing on Ghost, Link and Text — and a disabled **input** or **select** is the whole control at 0.5 over the muted surface it already takes. `tests/MotionTests.cpp` pins the multiply, the clamp and that a colour which was already translucent fades from where it was; 2884 checks.
- 2026-08-20: The dock, closer to `crates/ui/src/dock`. Four things its TabPanel does that the port did not. **A drop can name a tab**: `on_drop(drag, ix: Option<usize>, ..)` puts the panel at that place in the row rather than at the end, which is what reorders a group's own tabs — `DockTabsInsert` is `insert_panel_at`, down to ending on `set_active_ix`, and the index is worked out before the panel is detached, so a tab dragged rightwards inside its row lands one place short of where it was let go, exactly as Rust's does. Every tab is its own drop target with the left border Rust draws while a drag is over it, and the run of bar past the last tab is `last_empty_space`: a drop there appends, or moves a tab of this row to the end. **The ⋯ menu** is `render_toolbar`'s: Zoom In / Zoom Out, a separator, and Close on the active panel, disabled where the panel says it cannot zoom. **A collapsed Dock is a tab bar and nothing else** — no body, none of the suffix buttons — and clicking a tab in one opens the Dock again, which is Rust's "Open dock if clicked on the collapsed bottom dock"; before this the shut bottom Dock still rendered its panel, spilling it past the bar. `DockPlacementOfNode` is what a node uses to find the Dock it is in. One gap in the runtime came out of it: `ListenTo` had no fill-me-in form for a handler that takes a value the component supplies, so the menu's own row index could not reach a listener bound to another entity — the one that existed bound the argument outright, and `ListenerFill` leaves a bound argument alone. `tests/DockTests.cpp` pins the insert, the detach order, the two same-group refusals and the placement walk; 2905 checks. Still not ported from Rust's dock: the floating preview a dragged tab carries, the tab bar's scroll when tabs overflow, and `InvalidPanel` for a saved layout naming a panel the app no longer has.
- 2026-08-20: The dragged tab carries its preview, which the entry above listed as missing. `TabPanel::render_drag_panel` is the little floating label GPUI renders as the drag's own view — `w_24`, a border, the active tab's surface and 0.75 opacity — and it follows the pointer at `cursor_offset`, the point inside the tab the press was taken from, so the label stays where it was picked up rather than snapping its corner to the cursor. The runtime keeps that offset now (`Window::dragOffX/dragOffY`, taken as the drag starts, read back through `WindowDragOffset`), since it is the one thing only the press knows. There is no drag view to hand the window here — an element carries no closures — so the dock draws the preview itself, fixed and deferred, which puts it over everything the way Rust's does. `cmd/shot.ts` gained `-draghold`, a drag that never lets go, so a screenshot can catch what a drag looks like in flight: the preview under the pointer, the drop placeholder over the half it would land in, and the tab it would land before all show up in one frame now.
- 2026-08-20: The last of the dock. **A layout can be saved and put back**: `DockDump` is `DockArea::dump` — a split written as a StackPanel with its sizes and axis, a group as a TabPanel with its active index, a panel as a leaf under the name it was registered with — and `DockLoad` is `DockArea::load` with `PanelState::to_item`, matching panels by name against what the host registered. That gave `DockPanelDef` a `name` beside its title, which is `Panel::panel_name()`: the title is what a tab shows and may change, the name is what a layout stores. A name nothing answers to becomes an **InvalidPanel**, registered on the spot under the name that was asked for, rendering Rust's own sentence — so the layout keeps its shape, says which type is missing, and writes the same name back when it is dumped again. The story's dock page has Save, Load and a Load-stale button that reads a layout naming a panel this build does not have. **The tab bar scrolls**: a row of tabs wider than the bar keeps its tabs at their own width and slides under the suffix, the wheel over it moves it, and a tab made active is brought into view — `scroll_to_item`, as `DockTabScrollTo`, applied on the frame after the one that measured the tab, since a tab just made active has no box yet. That needed one thing of the runtime: a box could not scroll without also painting a bar, which a tab bar must not; `El::HideScrollbar` is the split Rust gets for free by making the scrolling container and the Scrollbar two different elements. One real bug came out of it: `Window::activeDrag` was cleared only when a drag had actually moved, so a press that picked a payload up and let go without moving left the drag standing — GPUI takes it on the release either way, and the dragged tab's preview made the leftover visible. 2934 checks.
- 2026-08-20: The focus ring has an off switch at each end. `FocusableExt::focus_ring` (`crates/base/src/component_traits.rs`) is a control saying it draws its focus some other way: `El::FocusRing(false)` drops the whole appearance, the tinted border along with the ring, which is what Rust's `.when(is_focused && self.focus_ring_enabled, ..)` does. It is carried by Button, Checkbox, Radio, Select, Combobox, DatePicker, Input, NumberInput and OtpInput, the same nine that carry it there, and nothing in either tree calls it — the trait exists so an application's own control can. `Theme::focus_ring` is the other end and belongs to the application, not the control: off, a focused element keeps its border in the ring colour and gives up the three pixels outside it, which is for a layout that clips its containers and would cut the ring off anyway. The story's Appearance menu has a Focus Ring row so both can be seen. Two things fell out of putting the switch in: an Input's ring is gated on `appearance` the way Rust gates it, so a NumberInput's editor no longer draws a ring inside the frame that is already showing the focus for it, and the NumberInput frame now takes the ring colour on focus, which `number_input.rs` does and this did not — the frame was the only thing that could say the editor had the keyboard, and it was saying nothing. 2940 checks.
- 2026-08-20: `cursor_pointer` — the hand, which the port had no cursor for at all. `CursorKind::Pointer` is `IDC_HAND`, `NSCursor.pointingHandCursor` and `XC_hand2`, and it goes where Rust asks for it: a Link, a breadcrumb level that has somewhere to go, and the two Button variants that look like a link rather than a button (`interactive && (is_link() || is_text())` — a ghost button is still a button and keeps the arrow). The story's three title-bar tools ask for it themselves, which Rust's do not: over a title bar there is nothing else to say an icon is a control rather than an ornament. Not ported: `text/node.rs` and `text/inline_flow.rs` put the hand on a markdown image that is a link, which this has no linked images to put it on.
- 2026-08-20: Input methods, the shared half and Windows. `InputState` has an `ime_marked_range` and the three calls that move it — `InputReplaceAndMarkText` (each candidate replaces the last, because a null range means "over the mark"), `InputUnmarkText`, and `InputReplaceTextInRange`, which already was the commit and now takes the mark as its default range and clears it. The whole composition is one undo transaction, so it takes itself back in one step rather than candidate by candidate, and a readonly field refuses a composition as flatly as it refuses a keystroke. The marked run is drawn underlined with the caret at its end, which is `InputElement`'s rule; `El::MarkRange` and `PaintTextUnderline` are the element and paint side of it, hung off the same line rects the selection quad uses and off a new `TextLayoutBaseline` so the rule lands under the glyphs rather than at the foot of the line box. On Windows the IMM32 messages drive it: `WM_IME_SETCONTEXT` turns off the system's own inline composition window (the field draws it), `WM_IME_COMPOSITION` maps `GCS_COMPSTR` to a mark and `GCS_RESULTSTR` to a commit, and the candidate list is moved under the caret. 2961 checks.
- 2026-08-20: Input methods on the other two platforms. macOS: `GpuiView` is an `NSTextInputClient`, so a focused field's *text* goes through `interpretKeyEvents:` and comes back as `insertText:` or `setMarkedText:` — the keys still go through `WindowKeyDown` first, which is where this port binds them, and `doCommandBySelector:` is a no-op so nothing runs twice and Cocoa does not beep. `firstRectForCharacterRange:` puts the candidate list under the caret. Linux: the XIC now asks for `XIMPreeditCallbacks` and falls back to the `XIMPreeditNothing` it had, so where the input method offers on-the-spot preedit the field draws the composition in its own font and where it does not the method shows its own window as before; XIM builds the preedit by edits rather than sending it whole, so the window keeps the string and replays it into the mark. `Utf8OffsetToUtf16` / `Utf16OffsetToUtf8` are shared, since every input method counts in UTF-16 and a field counts in bytes. The macOS half compiles on a Mac but has not been run there, and neither WSL nor this machine has an input method installed to exercise the X11 half; the Windows half was driven end to end.
- 2026-08-20: `FocusHandle::tab_index` and `tab_stop`, which the port had neither of: Tab order was paint order and focusable meant tabbable. `El::TabIndex` groups the traversal — the lowest index first, paint order inside it, which `FocusCollect` gets from a stable insertion sort — and `El::TabStop(false)` leaves a control focusable, ring and all, while Tab walks past it. Rust turns the stop off in two places and both are ported: an input's clear button and mask toggle (`clear_button.rs`), and every tool on a dock tab bar — the two side toggles, the zoom button and the menu (`tab_panel.rs`). Before this, Tab out of a field with a value landed on its own X. 2975 checks.
- 2026-08-20: A keymap, which the port had been doing without. Rust dispatches an action *type*: `actions!` declares one, `KeyBinding::new("ctrl-c", Copy, Some("Root"))` binds a keystroke to it in a named key context, an element declares `.key_context(..)` and `.on_action(..)`, and a keystroke is resolved against the contexts stacked over the focused handle and then offered to the handlers along that same chain until one keeps it. There are no types to dispatch on here and no RTTI to ask, so `ActionOf("ns::Name")` is the hash of the name and a handler is a Listener under that id; the rest is Rust's shape. `src/gpui/keymap.*` is `Keystroke::parse` and the binding table, `El::KeyContext` / `El::OnAction` are the element half, and `WindowDispatchKeyAction` is the dispatch. The tree is frame-arena and gone by the time a key arrives, so the ancestry is recorded while it is still there: `win->dispatch` is the tree walk in order, each node carrying the index its subtree closes at, which makes "above the focused element" a comparison rather than a walk — an element with a focus handle gets a marker node of its own so it cannot share an index with the subtree that closed beside it. The first user is the inspector, whose chord was hardcoded and is now a binding, with the `cmd-alt-i` macOS spelling it never had. Not ported: multi-stroke bindings and predicate contexts. 3013 checks.
- 2026-08-20: The last of the keyboard gaps. **DatePicker's Delete**: `on_delete` calls the clear button's own handler, so Delete and Backspace clear the date whether the picker is open or shut and whether or not it is disabled — Rust's handler has no disabled check either. It was in the story by hand and is now in `DatePickerActionForKey`, where another application can find it. **`Confirm { secondary }`**: Rust binds the list's confirm twice, to `enter` and to `secondary-enter`, and the field it carries is what `delegate.confirm(secondary, ..)` reads. `ListActionForKey` now answers the pair rather than the bare action, so the table says which of the two spellings a keystroke was rather than leaving every caller to remember to pass the modifier — one of the two story pages was, the other was not. **The inspector's macOS chord** went in with the keymap. 3019 checks.
- 2026-08-20: Rich text depth — `TextView` grew the half of `crates/ui/src/text` it had been dropping. **HTML**: `src/ui/html.cpp` is `text/format/html.rs` without html5ever — a tokenizer and a stack of open elements that fold tags into the same `MdNode` tree md4c builds, so one renderer walks both. It reaches the tree from three directions, the way Rust's does: a raw HTML block inside markdown (parsed in `MdParse` rather than rendered as an empty div, which is what `markdown_ext.rs` hands to `format::html`), an inline `<b>` / `<a>` / `<br>` / `<img>` in a paragraph (md4c reports those as raw text and `MdInlineHtml` turns them into marks), and a whole document through `TextView::NewHtml`. The subset is Rust's: its block elements, its inline marks, tables with their alignment, `<pre>` with the language off the `<code class>`, `<script>` / `<style>` / `<head>` dropped, `<img>` reduced to its alt text since nothing here decodes a PNG. **Links** click: `TextView::OnLink` is `link_click_handler`, and with no handler a link opens in the desktop's browser — `handle_link_click`'s own fallback to `cx.open_url`. **Table column alignment** was parsed and thrown away; the cell's flow now justifies by it, from the delimiter row's colons or from an HTML `align` / `text-align`. **Strikethrough** is real rather than a muted stand-in: `kFontStrike` is `SetStrikethrough` on DirectWrite and a Pango attribute, and since Core Text has no such attribute (it is AppKit's) `paint_mac` draws the rule from the font's metrics. `<mark>` paints yellow-200 behind the text like `html.rs`, with the ink pinned dark so it reads in both themes. `examples/rich_text.cpp` shows the same document as markdown and as HTML; `tests/TextViewTests.cpp` pins both parsers. Still short of Rust: no syntax highlighting (no tree-sitter), no images, and selection is still per element — `text/window_selection.rs` spans a window's text elements and this does not. 3089 checks.
- 2026-08-20: Fenced code blocks are highlighted. Rust parses the fence's language with tree-sitter, runs its `highlights.scm` and looks the capture names up in a `HighlightTheme`; `src/ui/syntax.cpp` is the part of that a scanner can carry — comments, strings, numbers, keywords and type names per language, plus what position alone settles: a name before `(` is a call, a string before `:` in JSON is a key, a name after `<` is a tag, a capital starts a type in the languages that spell them that way. Fourteen rows cover c/cpp, rust, js/ts, python, go, java/kotlin, c#, shell, json, html/xml, css, sql, toml/ini and yaml, and a fence we do not know renders as it did before. The colors are `theme/default-theme.json`'s own `syntax` block for "Default Light" and "Default Dark", so a block comes out the color the Rust one does. `TextView::CodeLines` is where the one-TextEl-per-block rule gives way — a line has to be several elements to be several colors — so the rows carry the line box and every element in them is the same mono face at the same size. `tests/SyntaxTests.cpp` pins the language table, each language's tokens, and that the tokens partition the source. 3149 checks.
- 2026-08-20: Images are drawn rather than named. `paint.h` grew `ImageDecode` / `ImageDraw`, one implementation per platform and each the system's own decoder — WIC on Windows, `NSBitmapImageRep` on macOS, and cairo's PNG loader on Linux, which is the whole of what X11 + cairo + Pango offers, so a JPEG in a document comes out as its alt text there and as a picture on the other two. `src/gpui/image.cpp` is the portable half: what a `src` may name — an asset path resolved through `gpui/assets.h`, or a `data:` URI, base64 or percent-encoded — and one cache so the same badge twice decodes once, remembering a failure too or a page of remote images would retry every one of them every frame. An http(s) URL is not something this tree can fetch (a socket and a TLS stack it does not have), so it renders as its alt text, which is why `MdRun` keeps the alt beside the source. `ElKind::Image` is gpui's `img(..)`: its own pixels are its size, a width or a height from the document wins with the other side following the aspect, and it shrinks to the width it is given — `object_fit(Contain)` with `max_w(relative(1.))`, the pair node.rs gives a markdown image. An image that will not decode measures and paints its alt text instead, so the line it sits in is right either way. Both parsers now build image runs: `MD_SPAN_IMG` with the alt md4c hands over as text callbacks, and `<img>` with html.rs's own `attr_width_height` (a percentage is not a size this layout can use, so it reads as none). An image inside a link is a link, hand cursor and all, the way `ImageNode::link` is. 3169 checks.
- 2026-08-20: Selection belongs to the window. Rust puts it there — `crates/base/src/text_selection.rs` keeps a `WindowSelectionState` per window, every selectable run registers with it as it paints, and the *window's* mouse handlers drive the gesture, which is what lets a drag run from one paragraph into the next without either knowing about the other; `text/window_selection.rs` is 2000 lines of tests over that. The registrations here are the `TextHit`s a frame already collects and the endpoints are offsets into that same document order, so what was missing was the state and the handlers: `WindowSelection` on the `Window`, `WindowSelectionPress` / `Drag` / `Release` called from `DispatchMouse*` before the view's own handlers, `WindowSelectionApply` before the view renders, Ctrl+C in `WindowKeyDown` once the focused field has had its go, and a press that lands on no run clearing what was selected. Two things the hand-rolled copies did not have: **shift-click extends** from the anchor (`begin_in_window(.., extend)`), and **scopes** — a `TextSelectionScopeId` is the focus trap a run sits in, so a drag that began in a dialog cannot reach the page behind it, cannot copy it, and cannot paint over it. `dialog_overlay` exists to pin exactly that and used to approximate it by clamping to the nearest run. Three examples lost their copies of the gesture: `story` (60 lines), `dialog_overlay` (50), and the shell's Ctrl+C; an application now says `Selectable()` and nothing else. `tests/TextSelectionTests.cpp` stands a window up with hand-built runs and drives the same presses: the join across two runs, the scope that a drag cannot leave, the margin-only drag that publishes nothing, and shift-click. 3183 checks.
- 2026-08-20: The two halves of the keymap the port had left out. **Predicate contexts**: a binding's context is a `KeyBindingContextPredicate` now rather than a name — `"Editor && mode == full"`, `"mode != full"`, `"!Editor"`, `"Editor || Terminal"`, `"Workspace > Editor"`, with parentheses and Rust's precedence, where `>` binds loosest and `!` tightest. An element's `key_context` grew the other side of it: `KeyContextOf("Editor mode=full")` parses the identifiers and the `key=value` pairs and remembers them under the hash of the whole spelling, so a `DispatchNode` still carries one `uint32_t` and the predicate reads the parse behind it. Evaluation is Rust's: an identifier or a comparison is read against the innermost context, `a > b` wants the context immediately outside the one that matched `b`, and an empty stack matches nothing — negation included, since Rust returns before it looks at the operator. **Sequences**: `KeyBinding::new("ctrl-k ctrl-o", ..)` reads as its chords, and the matcher holds a chord that begins one instead of dispatching it. A complete binding beats one that is only begun, which is why `ctrl-k` bound beside `ctrl-k ctrl-o` still fires at once, and a chord that neither finishes nor continues anything drops what was held. A held chord changes who gets the next keystroke: `WindowKeyDown` asks `KeymapPending()` first and stands the focused field and the page's Copy down, the way GPUI matches a keystroke before the text input is offered it — and `win->eatChar` keeps the character the second chord also arrives as out of the field under it. A window that loses the focus drops all three, which is Rust's `Window::deactivate`: the rest of the sequence is going to be typed into whatever took the focus, the character a taken keystroke was going to arrive as never will, and the Enter held down over a focused element gets no release here — `WindowSetActive`, which all three platforms already call, is where that lives. 3262 checks.
- 2026-08-20: `ScrollbarMode::Scrolling`, the third mode and Rust's default. The bar is up while the offset moves, holds for `FADE_OUT_DELAY` idle seconds and then fades over the rest of `FADE_OUT_DURATION` along Rust's own curve, `1 - (elapsed - delay)^10` — flat for most of that second, then a drop off the end. What it needed was a clock per scroll area, which the frame-rebuilt tree has nowhere to keep: the pair Rust holds in the scrollbar's keyed state (`last_scroll_offset`, `last_scroll_time`) lives beside the tree and is found again by `El::ScrollId`, so an area with no id of its own keeps the bar rather than blinking it. The pointer resting in the band the thumb runs down holds the bar up, which is Rust stamping the time again on every frame it is there. A bar part-way through its fade asks for the next frame — one ask for the whole tree, after it has painted, rather than each bar scheduling its own idle timer — through `PaintCtx::wantsAnimFrame` and the `WindowRequestAnimationFrame` motion already uses. A scroll area the frame has never seen starts faded, so opening a page does not flash every scrollbar on it. The theme default stays `Always`: a story shot of a scrollable page should show its bar. The story's Appearance menu offers all three.
- 2026-08-20: Settings fields are built from the type of the value behind them, which is `crates/ui/src/setting/fields`. Rust's `SettingField<T>` is a getter, a setter and an `Option<T>` default that a `SettingFieldType` renders — a bool as a Switch or a Checkbox, an f64 as a NumberInput with min/max/step, a String as an Input or a Dropdown; there are no closures here, so the pair the closures would have captured is the caller's own value, and the field is handed the address of the `bool`, the `InputState` or the `SearchableListState` behind the setting. `SwitchField`, `CheckboxField`, `InputField`, `NumberField` and `DropdownField` each fill in the control of the item last added, and the component then does the reading, the writing, the dirty test, the reset and the number stepping itself. The listeners find their field again through a table on `SettingsState` rebuilt in the order the fields paint, which is what Rust keys off `options.item_ix()`; `Element` stays as `SettingField::element`, the escape hatch where the caller builds the control. `default_value` is what puts the reset button behind an item — no default, no button — and `SettingPage::resettable` plus the page header's Reset All came with it, there once anything on the page has left its default. The story page lost its eight toggle handlers and its hand-written dirty test (219 lines to 150) and gained what it never had: the font size's steppers now obey the 8..72 the description promises.
- 2026-08-20: The inspector's style editor, which was the half of `crates/ui/src/inspector.rs` the port had left out. Rust offers two editors over the picked element's `StyleRefinement`, one spelling it as Rust source and one as JSON, both with LSP completions; there is no reflection table here and no language server, so what is ported is the JSON half over a subset written out by hand — `StyleField`: the background, the two other colours, padding (a number or the four edges), gap, radius, border, font size, opacity and the two sizes. The panel gained the shape Rust puts around it: the editor, the parse error under it as an `Alert::error`, and Reset. Applying it needed a seam of its own, since the frame tree is rebuilt and its `El`s go with it: `StyleOverrideSet` keys a patch by the element's click id and `LayoutEl` applies it to the frame's copy on the way past, which is what taking over a `StyleRefinement` amounts to here. An element with no id says so rather than offering an editor that could not find it again. Picking got two fixes it needed for any of this to be usable: it now prefers an element that has an id, then one that draws something, and only then the deepest box — an unnamed label inside a button no longer stands in front of the button, and the full-window layer the overlays paint into no longer stands in front of everything just because it goes down last — and `depth` is the real tree depth rather than a counter of how many boxes the pointer happened to be inside. `tests/InspectorTests.cpp` pins the round trip, that an edit names only what it changed, that a bad value is an error rather than a value silently dropped, and that an override patches one id and nothing else. 3403 checks.
- 2026-08-20: A second window. The runtime has had the pieces for a while — `App` keeps a window list, `WindowOpenView` can be called at any point, and Windows' `WM_DESTROY` only posts the quit message once the last one has gone — and nothing had ever opened one, so nothing pinned that it worked. The story's title bar Window menu is now a real `DropdownMenu` rather than a decorative label, with New Window and Close Window under it, and `StoryOpenWindow` is Rust's `create_new_window_with_size`: a fresh `StoryApp` entity, `TitleBar::window_options()`, the 1600x1200 that `WindowOpen` caps at 85% of the display and centres, and the window's own `WindowOnUnhandledClick` / `WindowOnKey`. `GpuiMain` opens the first window through the same function, so there is one way to make a gallery window and not two. Checked by enumerating the process's top-level windows before and after the menu item and shooting the second one: two windows, both drawn.
- 2026-08-21: The fixed-size caps go. Eleven of them — 16 tabs, 16 setting items across 8 groups across 8 pages, 8 open dialogs, 32/64 sankey nodes and links, 16 list sections, 96 table columns, 32 dock nodes and 32 panels per group with 8 children per split, 64 saved-layout nodes with 16 children each, 16 tiles, 512 tree items, 64 searchable items and 16 selected — were fine for a story and wrong for anything else, and the ones on the state side quietly truncated rather than failing. Two shapes replace them. A **frame builder** — `Tabs`, `Settings`, `Root`'s dialogs, `SankeyChart`, `Tiles`' panels — lives on the frame arena and is never destructed, so its items are an `ArenaVec<T>`: a new container in base with no destructor that grows into the arena the builder was given, which is the whole of what `Vec<T>` does minus the free the arena already handles. **State that outlives a frame** — the searchable list's selection and matches, a list's sections, a table's per-column widths, order and head boxes, the dock's node pool and every node's panels and children, the saved layout's tree, the tiles and the tree's items — is a `Vec<T>` with a destructor to match. Two things came out of it: `kMaxDockNodes` had been doing double duty as the base a listener bound to one of the three docks packs its placement into, which is now `kDockSideBase` and a number no node index can reach; and `TableEnsureCols` grows the three column arrays in one go before the heads are built, since `BoundsOut` keeps a pointer into one of them. What is left with a cap says why it has one: four search keywords per setting item, 64 tile undos, 8 nested key contexts, 3 strokes in a chord. The Tree and Dock story pages are pixel-identical to the commit before.
- 2026-08-21: The showcase's resizable divider is the first in-tree caller of `El::OnMouseDown` / `OnDragMove` / `OnMouseUp`. It had been borrowing the window's pointer — `WindowOnMouseMove` bound to the page, a flag on the view, and a walk of last frame's hit rects to find its own box again — which is exactly what GPUI's `div().on_mouse_down(..).on_drag_move(..)` exists to avoid: the element that took the press keeps the moves until the button comes back up, and `DragMoveEvent::el` hands the handler the box the last frame laid out, so it reads its own answer back on the next move. The divider also gained `cursor_col_resize`, which it could not have had without a click id. The showcase's window-level mouse handlers stay for the text-selection page, which is the other thing they were doing.
- 2026-08-21: `DispatchPhase`, which is the shape of GPUI's `Window::dispatch_event` and the one thing its mouse dispatch had that this tree's did not. A press or a release used to go to exactly one element — the topmost hit rect under the pointer — where GPUI offers it to the whole chain twice: outside-in in the **Capture** phase, where an ancestor can pre-empt what is inside it, and then inside-out in the **Bubble** phase, which is where a handler that only cares about its own element sits. `El::OnMouseDown` and `OnMouseUp` take the phase (Bubble by default, which is what every existing caller meant) and `WindowStopPropagation` is `cx.stop_propagation()` — the rest of the chain does not hear it. The chain is the one the paint recorded, not every box that happens to contain the point: a hit rect names the enclosing hit rect as its parent, so two absolutely placed siblings that overlap do not become each other's ancestors. Its first caller is the one Rust has: `tiles.rs` hangs `on_mouse_down` and `on_mouse_up` off the tile *container* and lets them hear what the drag bar and the resize handles inside it took, which is what brings a tile you pressed anywhere on to the front. `MouseUpEvent` gained the `el` bounds `MouseDownEvent` already carried, since the chain fills one in as it walks. Every story page is pixel-identical.
- 2026-08-21: `state_style`'s resolver reaches the two primitives Rust calls it from. `crates/base/src/button.rs` and `link.rs` are the only two files in Rust that call `resolve_style`, and the port had it wired into the themed Button alone — the unstyled `Button` and `Link` handed back a bare `El*` with nowhere for a caller-supplied state to go. They now take `ButtonStyles` (`selected`, `disabled`) and `LinkStyles` (`disabled`), and `StateStyle` gained the `opacity` Rust's own test names. Making that mean anything needed one seam: a refinement has to win over the *instance* style, and a builder hands the element back before the caller has chained its instance style on. So `El::Refine` records the refinement and layout applies it — the same place the inspector's live edit lands, through the same `StyleApplyFields`, and `StateField` is `StyleField`'s bits rather than a set of its own, since a semantic state and a live edit are both refinements of a whole style. The showcase's link and button pages are the callers: the disabled link's muted border and grey text and the disabled button's half opacity are the state's now, not a second set of colours written out beside the first, and every one of them still wins over the chain below it. `tests/StateStyleTests.cpp` pins the new order — a state beats what was chained after it — as well as the old one. 3408 checks.
- 2026-08-21: `cmd/shot.ts -hover` says when it cannot do what it was asked. A window keeps a `TrackMouseEvent(TME_LEAVE)` up, and Windows answers a move with `WM_MOUSELEAVE` at once when the pointer is not really over the window — so on a desktop where the pointer cannot be placed at all (a locked session, a CI agent with no interactive one, both of which refuse `SetCursorPos`) the synthetic move sets the hover, the leave that follows clears it, and the frame that finally paints has none. Nothing between the two calls the harness can make is out of band enough to get a frame in edgeways. So `hoverClient` reports whether the pointer actually landed and the shot warns when it did not, beside the warning it already prints for a window that never reached the foreground. A hover shot taken on a real desktop was always right and still is; one taken here now says why it is not.
- 2026-08-21: The rest of the caps, and the bug the first sweep left behind. Three more arrays had a ceiling of their own — a popup menu's 128 rows, a themed menu's items, a native menu's 32 — and are `Vec` and `ArenaVec` like the others; `NativeMenu::Show` counts what its tree can offer and sizes the flattening table to that rather than to a constant times four. What stays capped says why: 16 in a toast stack (a screen's worth is the real bound), the notification list's own `max_items`, four search keywords on a setting item, 64 tile undos, 8 nested key contexts and 3 strokes in a chord. The bug: `nSelected = 1` reads as `selected.len = 1` and both compile, but on a `Vec` with nothing in it that is a write past the end — six call sites seeding a select's selection did exactly that, and the Select, Combobox, Settings, Form and ThemeColors pages had been walking off their arrays since the first sweep. `SearchableListSelectOnly` is the seam they wanted, and it is what `SettingsState::OnFieldReset` uses too. Every one of the 65 story pages and 40 showcase pages now opens.
- 2026-08-21: Focus handles per widget, which is `FocusHandle` and the three things a popover, a select and a popup menu do with one. The runtime had a single focused id and a trap; what it had no way to say was *which* element focus belonged to, or where focus had been before something took it. `WindowFocusedId` is `window.focused(cx)`, `WindowFocusWithin` is `FocusHandle::contains_focused` — focus on the element, or inside the trap it hosts — and `WindowRestoreFocus` is `previous.focus(window, cx)`, a no-op on a handle whose element has gone, which is what Rust's weak handle answers. Three widgets use them, each the way its Rust file does. **Popover**: `toggle_open` parks the focused element on the way in, focuses the popover's own handle — which hangs off the content, not the trigger's container, so Tab from the trigger still walks the page — and puts focus back on the way out, but only if the popover still has it, since a click elsewhere moved it on purpose. `Popover::TrackedFocus` is `tracked_focus_handle`, what it focuses instead of itself. **Select**: the content handle takes focus when the list comes up and the trigger takes it back when the list goes away — `SelectToggleOpen` does both, over two ids the element records every frame under the same names, since a state that outlives the frame cannot hold a pointer to one. **PopupMenu**: `previous_focus_handle` parked by `PopupMenuOpen` and given back by `PopupMenuDismiss`, with a submenu parking nothing, because the menu it came out of is what focus goes back to. `tests/FocusTrapTests.cpp` pins the three questions. 3419 checks.
- 2026-08-21: The OTP story types. `OtpEditValue` had been ported and tested for a while with nothing able to reach it: the runtime hands keys to `win->input`, which takes an `InputState`, and an `OtpState` is not one — a one-time-code field has no caret to move and no selection, only a run of digits. What it wanted was the seam GPUI already has and this tree did not: `El::OnKeyDown`, `div().on_key_down(..)`, the raw keystroke offered to the focused element and then out through the elements above it, before the keymap resolves the chord to an action. It rides the dispatch path the actions already walk, under a reserved action id no chord resolves to, and `KeyEvent::propagate` ends it the way an action handler's does. Both halves of a keystroke arrive there — the key, and the character it produced — because a digit reaches a window as a `WM_CHAR` and never as a chord. `component::OtpInput` takes an `Entity<OtpState>` now: the value, the masking, the length and the disabled flag are the state's, focus follows the window so a click elsewhere blurs the field, the caret sits in the first empty cell, and the story's five fields are five states rather than five string literals. `cmd/shot.ts` gained `-type=TEXT`, which sends the characters as `WM_CHAR` — `-key` carries the key, not the text it produced — and runs before `-key`, so `-type=42 -key=8` reads left to right.
- 2026-08-21: The slider answers the arrows. `slider.rs:477-506` is an accessibility block — `role(Slider)`, `aria_numeric_value/min/max/step`, `aria_orientation`, and `on_a11y_action(Increment | Decrement)` — and there is no accessibility layer in this tree for the role and the aria values to live in. The half that is reachable is the half a keyboard user needs either way: the track is focusable now, and Left/Down and Right/Up step the value by the slider's own step, which is exactly what Increment and Decrement do to it. A slider with no step of its own moves by a hundredth of its range, the ends of a range never cross, and a keystroke a slider took is not also a walk of the focus — an arrow on a slider already at its limit still belongs to the slider. `SliderStepBy` is the whole rule and `tests/SliderTests.cpp` pins it; the window finds the slider the way it finds the one under a press, by the hit rect the focused id names. 3434 checks.
- 2026-08-21: `Corners`, the one member of the geometry set that was missing, and the divergence it was holding up. `Style::radius` is a single float and stays what almost everything says; `El::Corners(tl, tr, br, bl)` is `rounded_tl` and its three siblings, for a box whose corners differ — which is a control butted up against its neighbour. It cost no backend work: D2D's rounded rectangle takes one radius and so do cairo's and Core Graphics', but the path API above them is already portable and a rounded box is nothing but four arcs, so `CornersPath` builds one there and `FillCorners` / `StrokeCorners` are what the paint reaches for when `hasCorners` is set. A corner is clamped to half the box, the way Rust clamps a radius. The first user is the one that was waiting: a NumberInput's step buttons round only their outer pair — `rounded_tl`/`rounded_bl` on the decrement and `rounded_tr`/`rounded_br` on the increment — at the frame's radius less its border, which is what number_input.rs does and what this tree drew square.
- 2026-08-21: The story's three masked fields are masked. `MaskPattern` and `InputSetMaskPattern` had been ported and tested for a while and nothing called them, so the phone field and the pattern field sat empty where the Rust story shows `(___)-___-____` and `___-___-___`. They take the patterns `input_story.rs` names — `(999)-999-9999`, `AAA-###-AAA`, and `MaskPattern::Number { separator: Some(','), fraction: Some(3) }` for the currency field — and `mask_pattern()` puts the derived cue in as the placeholder, which is where the empty field's shape comes from. The two readouts under each field are the two Rust shows: the masked value, and `unmask_value()`, which was printing the masked one twice. `cmd/shot.ts` moved `-wheel` ahead of the clicks, so a click coordinate is read off the scrolled page rather than the one before it.
- 2026-08-21: The palette matches `default-theme.json`. Both themes were checked token by token against the file resolved through `default-colors.json` — sixty of them — and five were wrong. Four are the same mistake read the wrong way round: a semantic *foreground* in a dark theme is the 600 of its own hue, not the near-black the light theme puts on the same surface, so dark's info, success and warning foregrounds are `#0891b2`, `#16a34a` and `#ca8a04` rather than white and black; light's warning foreground is `#fafafa` rather than `#171717`. The fifth is dark's skeleton, `#171717` rather than `#262626`. Everything else — the tab bar, the six base hues, the five chart blues, the sidebar set, the scrollbar thumb's alpha, the table row border's, and the four table tokens that fall back to the list's the way `schema.rs` says — was already exact. `tests/ThemeColorTests.cpp` pins the five and the anchors either side of them. 3453 checks.
- 2026-08-21: The selection colour, and the double click that was already working under it. `selection.background` is a token of its own in default-theme.json — `#55a0fc` light, `#1d4ed8` dark — and the port had no such token: an input tinted its selection with `accent` at 45%, which in the light theme is `#f5f5f5` at 45% on a white field, a tint nobody can see. So a double click did take the word and a drag did extend the selection, exactly as `input/base/selection.rs` says and as `tests/InputStateTests.cpp` had been pinning all along; nothing was drawn. `Theme::selection` is the token, the two inputs and the code editor's highlighter read it, and the story's default field now shows the word a double click took and the run a drag covered.
- 2026-08-21: A cell takes the click, which is the other half of `table/state.rs`'s selection. `SelectCell` and `DoubleClickedCell` had been reachable only from the Go To menu and the keyboard: the cells themselves were not clickable, so cell selection was a mode you could see and not use. Each `td` binds a click now when the table is cell-selectable — `TableCellPack` carries the row and the column in the one `intptr_t` a listener has, the way every two-number listener here does — and a second click on the same cell emits `DoubleClickedCell` whether or not the selection moved, since `set_selected_cell` says nothing when it does not change. A cell-selectable table stops binding the row click, which is Rust's `SelectionMode` picking one of the three. The selected cell paints with the table's own active pair, the same way the selected row does. The story reports both events, as `data_table_story.rs` does. One bug came out with it: the story's toolbar separator was `H(1)->W(kFill)`, and `kFill` in a floating menu is the width of what the menu is over, not of the menu — so the Options menu, the only one with separators in it, was a blank box the width of the page. `align: stretch` is what makes a separator as wide as its menu.
- 2026-08-21: The G is back on the Introduction page. The README's first line is `<img src="https://.../logo.svg" width="112">`, and converting the page to `component::TextView` had turned it into its alt text: a remote URL cannot be fetched here and would not decode if it could, since none of the three backends reads SVG. Three pieces put it back, each useful on its own. `ImageAssetFor` is what a src names locally — a local path is itself, a remote URL is its last path segment looked for in the asset roots and then under `story/` and `images/` — which is how a document written for the web shows a picture the application shipped beside it rather than its alt text; the story ships `assets/story/logo.svg`, upstream's own mark. An image element whose src resolves to an `.svg` is drawn by the icon renderer rather than the bitmap decoder. And that renderer learned colours: a file whose shapes name their own `fill` is a picture rather than an icon, so each shape is filled with what it said and only the shapes that said nothing take the caller's — every Lucide icon says nothing and still comes out as one path in one colour, and the three icon-heavy story pages are pixel-identical. `SvgViewBox` answers the aspect, so a document that gave a width and no height gets the height rather than a run of text's line height.
- 2026-08-21: The resizable panels resize. `crates/base/src/resizable`'s arithmetic — `resize_panel_at_handle` and `adjust_to_container_size` — had been ported and tested with only the dock calling it: the Resizable story drew fixed divs with a hairline between them and nothing to drag. `component::ResizableState` is Rust's state — the axis, a size, a minimum and a maximum per panel, and which handle is in flight — and `component::Resizable` is the group that draws the panels and the handles between them. A handle is `resize_handle.rs`: a one-DIP line with four DIPs of grab either side, absolutely placed over the boundary, `cursor_col_resize` or `cursor_row_resize`, in `theme.border` and in `drag_border` while it is held — a token the palette was missing. `Grow()` is Rust's growing panel: it flexes for the one frame before the container's size is known and takes a size of its own after that, which is what makes a drag either side of it the same arithmetic as any other. Every panel keeps its share when the container changes, which is `adjust_to_container_size` and the half the story never exercised. The story's four groups are four states, its `Compact left → 100` buttons call `resize_panel_at_handle` from an action the way the Rust story does, and its hand-drawn dividers are gone. One trap the seam had: `PanelMax` reads a number, not a flag, so a panel declared with no ceiling has to be given `Pixels::MAX` rather than 0 — a 0 read as a ceiling clamps the panel to nothing.
- 2026-08-21: The calendar's 8-column artifact, reproduced on purpose after all. `calendar.rs` puts the seven weekday headers and every day of a month into one `h_flex().flex_wrap()` and never says seven: the column count is whatever the container's width fits. At the medium size that is eight — 288 wide, 12 of padding each side, 32-DIP cells, no gap between them, so 264/32 = 8 — and a month comes out shifted by one from its second row on, with the first day cell sitting beside Saturday's header. Small and large happen to land on seven exactly (208/28 and 292/40), so the artifact is the default size's alone. This port built explicit seven-column rows instead; it now builds the wrapping row, so the column count falls out of the width the way upstream's does and a screenshot comparison matches. It was recorded here as deliberately-not-ported first, and that judgement was overruled: matching the Rust app is the point.
  One thing was still not upstream's and is now: with `number_of_months > 1`, Rust's `body` is a `v_flex` of one wrapping row per month, so each month is as wide as the whole panel rather than as wide as one month. This tree laid the months out side by side and gave each wrapping row one month's width, so every month wrapped at the column a single one does. The day view's body is a column now and a month's row is measured against the whole panel, so the wrap point is the panel's.
- 2026-08-21: Gradient backgrounds, all three layers. A theme file may spell a token as a CSS `linear-gradient`, and until now this tree threw those away: `ThemeParseColor` failed on `linear-gradient(180deg, #1E293B, #0F172A)` and the token fell back to the base palette, so Aurora Light reported its primary as `#171717` — Default Light's near-black — for all 44 of the gradients it names.
  `gpui::Background` is the type that carries a fill: one colour, or two colour stops and an angle. The conversion from `Rgba` is implicit, the way `impl From<Hsla> for Background` is, so `Style::bg` and `Style::hoverBg` changed type and every one of the several hundred `->Bg(theme.foo)` calls in the tree went on compiling unchanged. Painting one needed no backend work at all: the path API already carries a linear gradient on all three (D2D's gradient brush, cairo's linear pattern, Core Graphics' `CGGradient`), and a rounded box is four arcs, so `FillBackground` builds the corners path and hands over two points. The points are `BackgroundLine`, which is the CSS geometry — the line runs through the box's centre at the angle and is long enough that the two corners it points between land exactly on 0% and 100%, which is what makes a 45-degree gradient reach the corners instead of stopping short. The endpoints come back at the stops' own percentages rather than at the ends, because all three backends clamp past a gradient's ends (D2D's default extend, cairo's PAD, Core Graphics' draws-before/after), so a 25%..75% gradient paints its first quarter flat with no extra stops.
  `theme/color.rs`'s `parse_linear_gradient` is the grammar: an angle in degrees or one of the eight `to ..` directions, then two stops with optional percentages, with commas split at the top level so `rgb(1, 2, 3)` stays one stop. Upstream's own two tests for it are in `ThemeRegistryTests`.
  `Theme` gains `tokens`, which is `theme_color.rs`'s `ThemeTokens`: the palette a second time, where each entry is the fill rather than the colour. Only the tokens `schema.rs` reads with `apply_background_color!` are there — 53 upstream, less the ones this tree's `Theme` has no field for. A token's `color` always equals the flat field of the same name, so `theme.primary` still answers one colour and `theme.tokens.primary` answers what to paint with, which is exactly how Rust splits `ThemeColor` from `ThemeTokens`. Three flat fields are new — `statusBar`, `switchThumb`, `sliderThumb` — on the fallbacks `schema.rs` gives them, which is what this tree already painted them with; they exist only so a theme that spells one as a gradient gets one. The clamp on the three tokens that paint over text follows Rust's split: a value the file named has each stop capped on its own, so a bright second stop cannot push a highlight past the cap, while one that came from a fallback is scaled as a whole.
  Then 139 `Bg(..)` sites moved from the flat field to the token, and the places that fade a token before painting it take `BackgroundOpacity` rather than `RgbaOpacity` — which is what makes a danger button the danger *gradient* at 20% rather than its first stop at 20%. `ListActiveStyle`, `NumberInput`, `Avatar`, `Dialog` and `GroupBox` carry `Background` now for the same reason.
  What is deliberately not ported: `theme_tokens.rs`'s `SemanticThemeTokens`, a newer parallel layer upstream is migrating to and which no component here reads; and the button family of tokens (`button.primary.background` and its two dozen relatives), because Aurora sets each of them to the same value as the token it falls back to, and this tree's buttons already derive from `primary`/`secondary`/`danger` — so the fallback chain produces the right look without widening `Theme` by twenty fields.
  Verified with a throwaway theme of deliberately garish gradients: the title bar, the status bar, a primary button, a danger wash, the progress bar's track and fill, and the switch's track and thumb all ramp, with the rounded corners intact. `GPUI_THEME` names a theme for the story to open with, so that shot is reproducible the way `GPUI_TODAY` pins the calendar's today.
- 2026-08-21: The table's context menu, and the dispatch bug it turned up. `TableDelegate::context_menu(row_ix, menu)` is the direct completion of the `RightClickedRow` work: a secondary press marks a row, and the table hands that row and a menu of its own to the caller, which fills it in and hands it back. Rust hangs the wrapper off its inner table rather than off a row, so it is there before there is a row to name — a menu with no items renders nothing, which is what the trait's default returning the menu untouched comes to. The DataTable story fills it the way the Rust story does: "Selected Row: N", a separator, and the same five sizes the toolbar offers; the row line carries `OpenDetail(row_ix)` upstream, which nothing handles, so it does nothing here either.
  It could not work until the dispatch changed, and that is the part worth recording. A non-left press went to the one element under the pointer and stopped: `DispatchMouseDown` hit-tested, called that element's `on_mouse_down`, and returned. So a table row could take a right press *or* an ancestor's context menu could, never both — and the whole point of Rust's arrangement is that both happen from the one press. GPUI bubbles a press of any button, which is why `cx.stop_propagation()` exists on this path at all and why the table's `on_cell_right_click` calls it: to keep the row under a cell from taking the press too. The secondary press now walks the same capture-then-bubble chain the primary one does, which also makes the `WindowStopPropagation` already sitting in `OnCellMouseDown` — dead until now — mean what it says.
- 2026-08-21: The rest of `table/delegate.rs` — `render_loading`, `render_last_empty_col`, `visible_rows_changed`, `visible_columns_changed`, `cell_text`. With these the trait is ported whole.
  `render_loading` was five plain bars in a column, and `table/loading.rs` is a fake *table*: a head row on the head colour and four rows under it, each holding three skeletons on the left — 96, 192 and 64 wide, so they read as a name, a description and a number — and one on the right, all at half the row height and padded by `Size::table_cell_padding`. It also stands in for the whole table rather than sitting under the real head, which is why its first row is painted the head colour: Rust builds `loading_view` *or* `inner_table`, never both, and the point of the shape is that the layout does not jump when the rows arrive. `UiTableCellPadding` and `UiTableRowHeight` are the two scales it reads, and `WithSize` now goes through the second rather than repeating the four numbers.
  `render_last_empty_col` is the blank past the last column on the scrolling side. The trait's default is `h_flex().w_3()` and shows nothing, so this is an override point rather than an appearance — a table that wants a row action or an add-column button out there puts it here.
  `visible_rows_changed` / `visible_columns_changed` needed `TableVisibleRange` on the state first, which is `visible_range()` as well. The row range is what the body was built from; the column range this tree has to work out, because Rust culls the columns outside the viewport and we build them all, so `TableVisibleCols` answers the same question without being what decides anything. Both go through `update_visible_range_if_need`: the delegate hears only when the range moved, and never about a range of one — Rust skips that because its virtual list lays a single item out to measure with, and here it is the frame before the pane has been laid out, when the width it is measured against is still zero. The width comes off the clipping pane's bounds and is a frame behind, the way any laid-out box read at build time is.
  `cell_text` is the delegate's string form of a cell, and `dump` is what reads it. Rust hangs `dump` off the state because the state owns the delegate; here the `DataTable` *is* the delegate, so it hangs off that, and the rows come back flat — `nColumns` to a row — since a Vec of Vecs is not a thing this tree builds. The story implements all three the way the Rust story does: the status line now prints the ranges the table handed it rather than working the rows out for itself against a hardcoded viewport height, and Export CSV, which was a button with nothing behind it, dumps. Rust passes the pair to the `csv` crate and then to a save dialog; this tree has neither, so the count and the header row and the first row come back in the message line under the table.
- 2026-08-21: `scroll_to_col`, `refresh`, and the selection that follows itself. `TableScrollToCol` is the sideways twin of `TableScrollToRow`, and the column it takes is the caller's: its place in the display order less the pinned ones is what moves, which is Rust's `col_ix.saturating_sub(fixed_left_cols_count())` — a pinned column is already on screen, so asking for one asks for the start of what is not. That count now lives on the state as `fixedCols`, written by the themed table as it builds, because a scroll is a click and a click has no columns to count; `TableVisibleCols` reads it there too and lost the argument it used to take. The widths are gathered in display order and handed to `VirtualListScrollToItem`, so the offset moves as little as it can: a column already in view does not move it at all.
  The point of having it is that Rust's `set_selected_col` and `set_selected_cell` both end in it, and neither of ours scrolled at all — nor did `set_selected_row`, which Rust scrolls too. So a walk across the columns with Tab or the arrow keys selected cells that were off the side of the table, and a walk down selected rows below the bottom of it. All three follow the selection now. Rust picks Bottom going down and Top coming back up for a row, and both fall into the branch that scrolls as little as it can, so one strategy says as much here.
  `refresh` is `prepare_col_groups`: the widths and the order the table worked out for itself are dropped, so the caller's declarations are taken again on the next build — Rust rebuilds `col_groups` from the delegate, which loses a dragged width and a moved column the same way.
  `refresh_header_layout` has no counterpart and is not ported. Rust caches the header cells in `header_layout` and that call is what invalidates the cache; here the group bands are summed from the current widths in the current order every time the table is built, so there is nothing to invalidate and nothing a caller could get wrong by not calling it.
- 2026-08-21: The find bar. `crates/base/src/input/editor/search.rs` and `crates/ui/src/input/search.rs`, which are the two halves of ctrl-f over a code editor: the matcher, and the bar that drives it.
  The matcher went in first, on its own, because it has no UI in it — a query, where it is found in the text, and a cursor into that list that survives a replacement mutating the text under it. Rust builds an aho-corasick automaton over a single literal pattern, which is a substring scan with an ASCII case fold on the side, so no library came with it. All four of Rust's tests port over whole, plus three that pin what aho-corasick was doing for Rust and is ours now. The one cost is that the matcher keeps a copy of the document rather than a rope clone: that copy is what makes `update`'s "has anything moved" check exact, and the check is not an optimisation — it is what ends a replacement that did not move a byte.
  The state half hangs off `InputState`, as Rust hangs it off `InputBaseState`: `searchable` (false, and true for the code editor), `replaceable`, and the session. Every edit already funnels through `InputReplaceTextInRange`, so `update_search` goes there and a bar left open follows what is typed. Two things had no seam and got one. `anchor_offset` is Rust's `last_layout.visible_range_offset.start` — the first character on screen, so the first match chosen is the one you were looking at; this tree builds every row and has no such range, so it is worked back out of how far the field has scrolled, which is the same answer a frame stale. And scrolling to a match is a row to bring into view, not a caret: `InputScrollToCaret` now takes a negative x to mean "leave the sideways offset alone", because how far across a match sits cannot be measured outside a paint.
  Painting them needed a second array on `El`. The span painter partitions the text so that each glyph is drawn exactly once, which means it cannot take two runs over the same bytes — and a search match sits over whatever the highlighter already said about those bytes. So matches are `washes`: lo, hi and a colour, painted where the selection quad is and before the glyphs. The rows slice them out of the document's list the way they already slice the highlighter's.
  That turned up a real bug underneath. `PaintTextRange` — which is what draws the selection quad, and now the washes — measured its rects with weight 0 and line height 0 rather than the ones the run was laid out with. The mono family is a weight sentinel here, so in a code editor every quad was measured against the proportional font and drifted further from the glyphs the further along the line it was. The washes made it obvious because there are eight of them across a screen; the selection had it all along. It takes both now.
  The bar itself is `component::SearchPanel`, and it owns the two fields it holds, so a caller names only the field being searched. `Highlighter` puts one above its rows — Rust docks it at the top of the input through the overlay, and the overlay's other tenants are the LSP popovers, which are not ported. Enter and shift-enter walk the matches, escape closes it and gives the caret back to the editor, tab moves between the query and the replacement, and the case toggle, the prev/next pair, the `3/17` counter, Replace and Replace All are all where Rust puts them. Two icons came over from `crates/assets`: case-sensitive and replace.

- 2026-08-21: The Windows backend presents through a swap chain now, which is
  most of the reason `fps_monitor` was slow. Profiling it with `winperf`
  (`winperf record -- fps_monitor.exe -bench 12`) put ~70% of the frame inside
  `ID2D1DCRenderTarget`: `BindDC` and `EndDraw` each map a staging texture
  through the DXGI/GDI interop and copy the whole surface back, so the window
  paid a full readback twice per frame no matter what it drew. `PaintTarget` is
  a `IDXGISwapChain1` (flip model, three buffers, `Present(0, 0)`) with an
  `ID2D1DeviceContext` bound to its back buffer — the shape GPUI's own
  `directx_renderer.rs` has, down to the buffer count and the sync interval.
  `PaintTargetBegin` takes the HWND rather than an HDC; the offscreen target,
  which has to hand its pixels back as a DIB, still uses a DC render target.

  `ID2D1HwndRenderTarget` is the shorter version of the same idea and was
  tried first: it is faster still, but its blt-model present never reaches the
  redirection surface `PrintWindow(PW_RENDERFULLCONTENT)` reads, so every
  `cmd/shot.ts` capture came out an empty rectangle. A flip-model chain
  composites the way Rust's does and captures like it too.

  `SetMaximumFrameLatency(1)` looked like the right call and cost ~20%: with a
  single frame in flight `Present` blocks the draw on the previous frame's
  scanout. GPUI leaves the latency at DXGI's default, and so does this now.

  `fps_monitor` gained `-bench <secs>` (with `-bench-out` and `-curves`), which
  runs the window for real, collects `WindowCollectFrames` after a one-second
  warm-up and writes the distribution out — a number to compare against
  instead of a screenshot of the HUD. On this machine at 800x600 the mean draw
  went 7.2ms -> 3.4ms. Sized to Rust's own default window the HUD reads 7.0ms
  against Rust's 3.0ms — a bigger window is fill-bound rather than
  present-bound, so what is left shows up there. That remainder is nearly all
  D2D tessellating ~2300 antialiased hairlines on the CPU (`FillNonOverlappingRectangles_SlowPath`),
  where GPUI rasterizes paths in a shader; batching each colour run into one
  stroked `Path` was tried and came out slower, since a D2D path geometry per
  run costs more than the six `DrawLine` calls it replaces.

- 2026-08-21: Code folding, which two earlier entries said this tree could not
  have. Both gave the same reason — the scanner in `src/ui/syntax.cpp` has no
  tree, and a fold needs one — and reading upstream says it does not. The fold
  extractor in `crates/ui/src/highlighter/input_adapter.rs` offers every named
  node spanning two rows or more, sorted and deduped by start line, which is
  row geometry rather than anything semantic; and the showcase's own
  highlighter ships `brace_fold_ranges`, a thirty-line `{`/`}` scan with quote
  and `//` awareness, driving the same `.folding(true)` editor with no tree at
  all. So upstream has two fold sources and one of them needs nothing we lack.

  `display_map/fold_map.rs` and `folding.rs` port as `FoldMap` and `FoldRange`
  in `src/base/input.cpp`. Rust's map projects wrap rows to display rows,
  because its display map wraps first and folds second; the rows here are
  logical lines already — a soft-wrapped line is one row as tall as its text,
  which is what `rowBoxes` is indexed by — so this maps line to display row
  and the wrap half has nowhere to live. Everything else is as written: the
  candidate list is sorted and one range per start line, a closed fold hides
  the lines *between* its ends so a folded block still reads as its opening
  line and its closing brace, and `adjust_folds_for_edit` drops the ranges an
  edit ran through and shifts the ones below it rather than re-extracting on
  every keystroke.

  The candidates come from `Highlighter`, the way Rust's come from the
  highlighter through `apply_highlighter_fold_candidates`. The scan is
  upstream's brace pairs run over `SyntaxLexer` rather than over raw
  characters, so a brace inside a string or a comment is not a brace — which
  is what Rust's hand-rolled quote tracking is doing by hand, and does less
  well. A language with no braces has no candidates, and that is where this
  stops short of the tree: Rust would fold a Python suite and this cannot see
  one.

  The gutter is `element.rs`: the line-number column widens by
  `FOLD_ICON_HITBOX_WIDTH` (18) and every candidate line gets a cell that
  size, with a 14px chevron in it. The chevron is drawn only while the gutter
  is hovered, on the caret's own row, or over a fold that is closed, which is
  `paint_fold_icons`; the cell is built and measured either way, because Rust
  prepaints an icon for every candidate and only the painting is conditional —
  the click that reveals a chevron has to be the click that lands on it. The
  press is routed by `InputPress` and stops there, which is the
  `cx.stop_propagation()` on Rust's icon.

  A caret inside a closed fold is drawn at column 0 of the line the fold
  starts on, which is what `buffer_pos_to_display_pos` answers for a folded
  position; the offset itself does not move. The vertical walk crosses a
  closed fold in one press because a hidden line measures as no height at all,
  and the hit test, `scroll_to` and the first-visible-row walk all snap onto a
  line that is on screen. `tests/FoldMapTests.cpp` pins the projection, the
  nesting and the edit adjustment; 3902 checks.

- 2026-08-21: `system_monitor`'s process table sorts the way Rust's does, and
  two things underneath it were wrong. The visible half: Rust builds the table
  out of `DataTable` with `Column::sortable()`, so every sortable column head
  carries an icon — the two chevrons at half opacity while the column is not
  the sorted one, the single chevron the sort is in when it is — pushed to the
  right edge of the head by `justify_between`, and `perform_sort` cycles
  Default -> Descending -> Ascending -> Default with every other column
  dropping back to Default. This example predates `component::Table` and drew
  a text arrow appended to the label of the sorted column only, with a
  two-state toggle, so the third press had nowhere to go and a column could
  never be given up. It uses `TableNextSort` and `ColumnSort` now, and
  `is_descending = sort == Descending` — so a column cycled back to Default
  sorts ascending, which is what `sort_processes` does.

  Underneath: the two-chevron icon drew *nothing*. `IconNamePath` names
  `icons/chevrons-up-down.svg` for it and the file is there, but nothing had
  registered an asset root, so every icon fell back to the built-in stroke
  table in `gpui.cpp` — and 32 of the 74 `IconName`s have no case in it.
  `ChevronsUpDown` was one, which is why `component::Table`'s sort affordance
  went missing in any app that never mentioned assets. Both ends are fixed:
  the stroke table gained the case, and `AppNew` supplies
  `AssetsAddDefaultRoots` when no root has been registered, so an app gets
  lucide's own files without having to know to ask. A caller that wants an
  example's own subfolder still asks, and `AssetsClear` still replaces what
  this found. The other 31 icons without a stroke fallback are still without
  one; they are only reached now if the assets folder is genuinely missing.

- 2026-08-21: `component::Checkbox` lines its box up with the first line of
  the label, the way `checkbox.rs` does. Rust's root is
  `h_flex().gap_2().items_start().line_height(relative(1.))` and the label div
  carries `line_height(relative(1.))` of its own, so the label's first line box
  is exactly the font size and shares a top edge with the 16px indicator. Ours
  was `ItemsCenter`, which looks identical on a one-line label and drops the
  box half a description lower on a labelled one — measured against the Rust
  window, the box sat 7px *below* the label ink where Rust puts it 3px above.
  The description column was `gap_2` where Rust has `gap_1`, and the label was
  drawn at `UiFontPx` — the generic control font — where Rust uses `text_base`,
  a step above it. `component::Radio` already had all four right and spells the
  font table out in a comment; the checkbox had drifted from it and now reads
  the same.

  Rust also puts `flex_1` on that column and this does not: here it would make
  every checkbox row claim the full width of whatever holds it, which lays a
  row of checkboxes out as a column and pushes the Disabled pair off the edge.
  The label measures itself instead, so a wrapped label breaks a word later
  than Rust's — visible on the story's two-line label and nowhere else.

- 2026-08-21: The multi-month calendar stacks its months, which is the last of
  the differences the 08-21 entry above left open. One thing was still not
  matched at the time and is now, a level below the calendar: Rust's themed
  wrapper sets `w(px(288.) * month_count)` — 864 for three months — and the
  story's section is narrower, so taffy shrinks the panel to the room it has.
  The entry that stood here said an explicit `W()` was a fixed width in this
  tree and left it as a layout-engine gap; the entry below closes it.

- 2026-08-21: The palette is read from the file. `crates/ui/src/theme` is data upstream — `default-theme.json` names sixty-odd tokens, the twenty theme files under `themes/` name a subset each, and `schema.rs`'s `apply_config` resolves what a file leaves out from what it sets — and ours was two hand-written C++ functions, so the two could drift and did. `src/ui/theme_registry.*` is the reader: the colour grammar from `color.rs` (a hex string, or a shadcn name with an optional scale and an optional percentage), the hundred-entry fallback chain from `schema.rs`, and the sorted table from `registry.rs`. `cmd/gen-theme-data.ts` turns `default-colors.json` and `default-theme.json` into `src/ui/theme_data.cpp`, so the numbers still come from upstream's files and a later pin lands by re-running it. `ThemeDefaultLight` / `ThemeDefaultDark` are what a file is resolved against and never change; `ThemeLight` / `ThemeDark` are the pair in force, which `ThemeInstall` replaces — Rust's `ThemeColor::light()` against its `Theme::light_theme`. The test resolves the file the hardcoded palette was transcribed from and demands the palette back, which found three transcription errors: a text selection is capped at 30% by `apply_config` and ours was opaque in both themes, and a dark input's background is its border mixed 30% toward transparent, not 70%. It also found that `default-theme.json` spells nine of its own keys differently from the serde names `schema.rs` declares — `chart_1` for `chart.1`, `drag_border` for `drag.border`, `progress_bar.background`, `description_list_label.*` — so serde drops those values and upstream paints the fallback: five distinct chart blues collapse to one lightened ramp, and the drag border goes from blue to the primary at 65%. Both spellings are read here, since the file's values are plainly what its author meant and are the only spelling a third-party theme would copy. The Theme Colors page offered two entries and its "Set Theme" button had no `OnClick` at all; it now lists what the registry holds — the two defaults plus every theme file found under a `themes/` folder on the asset roots, and the pinned Rust clone is now a root of its own — and applying one resolves it, installs it and switches the window to its mode. Nothing is checked in: those files are Apache-2.0 and several are Zed's, so they are read where they lie. 3636 checks.
- 2026-08-21: The layers belong to the window. `crates/ui/src/window_ext.rs` routes everything through `Root`, which is the window's own view: `window.open_dialog(cx, ..)` pushes onto `active_dialogs`, and any handler anywhere can raise one. Here the layers were the *page's* — `component::Root` is a builder a page fills in each frame — so only the view that rendered a dialog could open one, and the story had to hang its notification list off `StoryApp`. `src/ui/window_ext.*` is the store, kept with `window.use_keyed_state`, which is the lifetime Rust's Root has. A layer is an entity, as it is in Rust, so the `Fn(Dialog, &mut Window, &mut App) -> Dialog` callback upstream stores has no counterpart here: a view with a `Render` is that already. The layer owns its entity and closing drops it, the way Rust's `Vec<Entity<Dialog>>` does. `Root::IntoEl` draws what the window holds alongside what the page passed in, so every existing caller is untouched. Two first users: the story's notifications are `WindowNotifications` / `WindowPushNotification` and off `StoryApp` entirely, and Help — a dead label until now — is a menu whose About item opens a dialog from a handler with no view that draws one. `AppOnShutdown` is the seam a teardown above gpui registers through, since `AppFree` cannot name `src/ui`.
- 2026-08-21: A selection drag that reaches the edge keeps scrolling. `crates/base/src/auto_scroll.rs` was not ported at all: it is the curve a drag past the edge of a scrolling box scrolls by — a dead zone, then one ramp from twelve to sixty-four DIPs a tick that starts sixteen *inside* the box, so the drag still works in a full-screen window where the pointer cannot get outside the element. The arithmetic is `src/base/auto_scroll.cpp`, one function, with its two Rust unit tests. What is not ported is the machinery: Rust spawns a 16 ms background task per state and shares an `Option<Pixels>` with it, where the frame loop is that clock here — one tick per frame, and the tick asks for the next while the pointer stays out. Its user is Rust's: a textarea's selection drag, with the selection re-run at the pointer's last place after each scroll since the text has moved under it. 3657 checks.
- 2026-08-21: A table right-click is an event. `TableEvent` in `table/state.rs` carries `RightClickedRow(Option<usize>)` and `RightClickedCell(usize, usize)` and ours had neither: we kept `rightClickedRow` as state and painted it, but never told anyone, so a caller could not hang a context menu off a row. Both are emitted now with the exclusivity Rust has — a cell mark clears the row one and the other way round — and `set_selected_row` emits `RightClickedRow(None)` beside `SelectRow`, which is the -1 row here. A cell right-click stops the press so the row under it does not also claim it, which is `cx.stop_propagation()` in `on_cell_right_click`. The DataTable story reports both on its status line where upstream's prints them.
- 2026-08-21: `IndexPath`, and the two directions between it and a flat index. `crates/base/src/index_path.rs` is how Rust addresses a row of a sectioned list — section, row, column — and every list-shaped state upstream is keyed on one; this tree keys on the flat entry index with the section counts beside it, which is the same information said differently. The type is ported with its builders, `eq_row` and the ElementId form, and `ListIndexPathOf` / `ListEntryOf` convert both ways. Its first user is the list itself: a row element is named by its path the way `impl From<IndexPath> for ElementId` names Rust's, so the name holds still when a section above it grows and the flat index shifts under it. Re-keying `ListState`, `SearchableListState` and Combobox on it outright is not done and is a much larger change. 3688 checks.
- 2026-08-21: The list module, audited against `crates/ui/src/list` — 1725 lines to our 917 — turned up three real gaps and no structural one: `cache.rs` is what `ListRowAt` and `ListRowCount` already work out, and every delegate hook has a counterpart. `render_loading` was five plain bars where `loading.rs` is three placeholder rows, each a wide bar over a narrower secondary one, and it is a delegate hook upstream, so `List::Loading(el)` replaces it. `render_initial` was missing: what a searchable list shows before anything has been typed, which Rust asks for only while the query is empty. `list/separator_item.rs` was missing — a disabled ListItem, never selected and never clickable; nothing calls it here and nothing calls it upstream either, where it is exported API just the same.

- 2026-08-21: `DrawIcon` draws every icon in the set. It is the fallback an
  app gets when no `assets/icons` folder is found, and it covered 42 of the 74
  `IconName`s — the other 32 drew nothing at all, which is how the sort
  chevrons went missing from `component::Table` earlier today. The 31 that were
  left are written from the real lucide geometry in `assets/icons/*.svg` rather
  than from memory, so the fallback and the file draw the same shape.

  Three primitives went in with them, because the icons that were left needed
  what the table did not have: `rect` for the many that are mostly a `<rect>`,
  `arc` for a stretch of a circle (`loader-circle`'s gap, `chart-pie`'s wedge,
  `palette`'s three quarters), and `fillPoly` for a solid shape — `star-fill`
  has to read as filled beside the hollow `star`, and a stroked outline does
  not. `github` is the one that is a drawing rather than a transcription: the
  octocat is forty-odd curves, so it is a hand-authored silhouette filled as a
  polygon. Drawn as a circle with arms and legs suggested it read as a smiley
  face, which is worse than nothing; the silhouette reads as the mark.

  Checked by running a copy of `story.exe` from a directory with no `assets`
  above it — which is the only way to see this path now that `AppNew`
  registers the default roots — with every `IconName` on the icon page at
  40px.

- 2026-08-21: Flex children shrink. `LayoutChildren` grew leftover space out
  to the children that ask for it and always had; the other half of
  `resolve_flexible_lengths` — handing a *deficit* back by shrinking them —
  was never written, so `flexShrink` was a field nothing read. An explicit
  `W()` was a hard width rather than a basis that can give, and a child wider
  than the box holding it simply hung over the edge where taffy would have
  squeezed it in.

  `FlexShrinkLine` is that half: the deficit shared out in proportion to
  flex-shrink times base size, which is CSS's scaled flex shrink factor. Two
  details are what make it behave rather than flatten things. A child with a
  definite width has to be *lent* the smaller one for the measurement — its
  own children measure against its width, and a definite `W()` would resolve
  to the old number again — while a child with an auto or `kFill` width needs
  no loan and reports back its own content minimum, which is the
  `min-width: auto` floor this engine has no other way to find. And it runs
  per line, not per container: a wrapping row puts an item that does not fit
  on the next line, so the shrink for one of those happens inside the wrap
  block, where a line that still overflows — a lone oversized child — is
  squeezed. A row that scrolls sideways is left alone, since its children are
  meant to overflow so the bar has something to travel over.

  The 70 `Shrink0()` calls this tree already carried, mirroring upstream's 67
  `flex_shrink_0()`, were opting out of something that had never happened.
  They do now, which is why turning this on landed where it did: of the 65
  story pages, four changed outside animation noise — the calendar, which is
  the point, and settings, tiles and checkbox by two or three pixels of
  vertical tightening. The visible win beyond the calendar is the
  `system_monitor` process table: narrow the window under the 630 its four
  columns want and they now shrink together, header aligned with the rows,
  where before the table ran off the right edge.

  The calendar needed one change of its own to benefit. `CalendarMonth` was
  told a width worked out from the size and the month count, which is the
  same number as the panel's while the panel gets what it asks for and the
  wrong one the moment it is squeezed. Rust's month row is `h_flex()
  .flex_wrap()` with no width at all, so it is `W(kFill)` here now and wraps
  where the panel ends. The three-month calendar breaks its rows on the same
  days as the Rust window.
- 2026-08-21: Taffy, ported. `src/taffy/` is a C++ port of the taffy crate at
  0.12.2 — the version `gpui-component`'s `Cargo.lock` resolves for `gpui`,
  which asks for `=0.12.2` — and `LayoutEl` now lays the element tree out
  through it. The hand-written flex engine that used to live in `gpui.cpp` is
  gone.

  This is a reversal of a standing decision. AGENTS.md said Taffy was the layer
  *under* gpui-component and that we reimplemented a subset rather than ported
  it, and for most of this tree's life that was the right call: a row, a
  column, grow, and the constants copied out of the Rust were enough. It
  stopped being enough one question at a time. The shrink half of
  `resolve_flexible_lengths` was the entry the day before this one; before that
  it was wrap, and stretch, and shrink-wrap, and the layout memo that existed
  because the engine ran a subtree three times per parent pass. Every one of
  those was a piece of taffy being rediscovered from the outside, with the Rust
  open in the other window. Porting the crate answers all of them at once, and
  answers the next one before it is asked.

  What is there: flexbox, CSS Grid, block layout with margin collapsing,
  floats, `calc()` handles, content sizes, and the nine-slot per-node layout
  cache. What is not: the crate's `parse` (needs `cssparser`, and nothing here
  parses CSS) and `serde` features, and `detailed_layout_info`, which is an
  accessor for the computed grid track sizes that nothing reads.
  `src/taffy/readme.md` is the file-for-file map and the list of deliberate
  differences; the two that matter to a reader of the C++ are that the traits
  are gone (there is one tree type and one style type, so the compute pass
  takes a `TaffyTree*` and reads `Style` fields) and that the generic
  containers are gone (`Size<Dimension>` is `SizeDim`, `Option<f32>` is `Optf`,
  and so on, one concrete struct per instantiation the algorithms carry).

  A `taffy::Style` owns nothing: its grid track lists are arena-backed
  `Slice<T>` and a custom ident is a `Str` into the same arena, so a style
  copies as bytes. That is hard rule 4 applied to a type Rust can afford to
  fill with `Vec`s.

  `LayoutEl` is the seam. It walks the `El` tree once to resolve the style
  refinement, the inspector's edit, the inherited font and the hover color
  stamp; builds a taffy node per element, with text, icons, images and progress
  bars as measured leaves (Rust's `request_measured_layout`); runs
  `ComputeLayoutWithMeasure` with rounding off, the way GPUI does; and writes
  the boxes back. The taffy tree is kept between frames and cleared, so its
  node slots and per-node child arrays are recycled rather than reallocated.
  The `memo*` fields on `El` are gone with the engine that needed them —
  taffy's own cache is what stops a subtree being measured twice.

  Four things do not go through taffy, and the comment above `LayoutEl` says
  so: `fixed` elements, which resolve their insets against the *window*, so
  they are re-parented onto the root taffy node as absolutely positioned
  children; `anchorBelow` / `anchorAbove` / `anchorCenterX` and the
  `relative(f)` half of a left/right inset, which are gpui-component
  positioning rules CSS has no word for and which move an already-laid-out
  subtree afterwards; and `scrollX` / `scrollY`, which taffy has no notion of —
  it lays a scroll container's content out at the origin and reports how big it
  is, and the offset is applied to the in-flow children as their absolute
  positions accumulate.

  Two style translations are deliberately *not* what Rust's `Style::to_taffy`
  does, both because doing it the Rust way would move pixels everywhere at the
  same moment the engine changed, and both worth revisiting on their own:

  - `border` is not handed to taffy. A border still paints over the box rather
    than reserving space inside it, which is what every widget in this tree was
    built against. Rust reserves it, so our content sits one border width
    closer to the edge than the Rust window's does.
  - `minW` / `minH` map to a length of zero rather than `auto`, so CSS's
    content-based automatic minimum size is off. `gpui::Style` stores these as
    plain floats whose default `0` means "unset", so there is no way to tell
    "no minimum" from "a minimum of zero" anyway; Rust's default is `auto`,
    which gives a flex item a min-content floor. **This one was a mistake and
    is gone — see the entry below.**

  `El::contentW` / `contentH`, which the scrollbars read, now come from taffy's
  `content_size` rather than from the old engine's intrinsic main/cross sums.
  Taffy's number includes the container's padding on the trailing side, and the
  comparison the scrollbar makes is against the border-box size, so the two
  line up; the old pair excluded padding on both.

  `tests/TaffyTests.cpp` ports every `#[cfg(test)]` module in the crate that
  pins behaviour rather than Rust specifics: `util/math.rs`, `util/resolve.rs`,
  `style/alignment.rs`, `style/flex.rs`, `style/mod.rs` (`defaults_match`),
  `compute/mod.rs`, `tree/taffy_tree.rs`, and the grid's `explicit_grid.rs`,
  `implicit_grid.rs` and `placement.rs` — plus end-to-end checks of flexbox,
  block and grid layout, and one of its own for the `CompactLength` bit
  packing, which is the one place the C++ layout had to be re-derived rather
  than translated. Left out and not coming: `style_sizes`, which asserts Rust
  `size_of`s; the `parse` and `serde` cases, for features that are not ported;
  and `new_should_allocate_default_capacity`, which asserts on a `SlotMap`
  capacity the C++ tree has no equivalent of.

  Three grid internals are reached through named seams in `compute.h` rather
  than a test harness, per AGENTS.md. The crate's larger generated suite is not
  available to port: it lives in taffy's `tests/` directory, and the published
  crate's `include` covers only `src/` and `examples/`.

  One porting bug the tests caught and worth naming, because it is the kind
  that hides: `Line<OriginZeroGridPlacement>::is_definite` and
  `Line<NonNamedGridPlacement>::is_definite` are different functions in Rust.
  The second treats line 0 as invalid, because 0 is not a valid CSS grid line;
  the first does not, because in OriginZero coordinates 0 is the left edge of
  the explicit grid. Both placements are one `LinePlain` here, and it started
  out with only the CSS-grid-line rule, so every item placed on the first
  explicit line was treated as auto-placed. Both spellings are there now, named
  `IsDefinite` and `IsDefiniteGridLine`.

  Verified: `bun cmd/test.ts` (4586 checks), `bun cmd/build.ts -rel -all` and
  `-dbg -all`, and the laid-out element tree of `system_monitor` dumped and
  read against the Rust example's numbers. And a screenshot sweep against the
  old engine: the examples (`hello_world`, `input`, `sidebar`, `showcase`,
  `markdown_table`, `table_in_scrollable`, `focus_trap`, `dialog_overlay`,
  `rich_text`, `text_selection`, `tooltip_top_edge`, `app_assets`,
  `root_borderless`, `window_title`) and fourteen story pages, shot from a
  build with the old flex engine stashed back in and again from this one. All
  29 pairs are identical pixel for pixel — not close, equal. `cmd/shot.ts`
  warns that the window never reached the foreground on some runs and leaves
  the previous PNG in place when it does, so check the file's mtime before
  reading anything into a capture.
- 2026-08-21: Taffy's benchmarks, ported, and the 94-second grid they found.
  `bench/` is a port of the crate's `benches/benches/flexbox.rs`, `grid.rs`
  and `tree_creation.rs`, plus the tree builders in `benches/src/lib.rs` that
  all three draw their shapes from. `bun cmd/bench.ts` builds and runs them;
  the whole suite is eight seconds.

  They are not in the published crate — its `include` covers `src/` and
  `examples/` — so they come from a git checkout, the same one the generated
  test suite needs. Yesterday's entry said that suite was "not available to
  port", which was true of the tarball and false of the repository. Both are
  reachable; `port-upstream.md` has the clone.

  `benches/benches/mixed.rs` is left out. It measures text leaves through
  `parley`, and with our own text measure substituted it would be a new
  benchmark rather than a port of that one. It is the one most worth having
  afterwards, because a measured leaf is where a real window's layout time
  goes, and nothing here measures that yet.

  The harness is criterion's `iter_batched` without criterion: setup builds a
  fresh tree untimed, the run is what the clock sees, and the row reports the
  median and the minimum over ten samples. Criterion's statistics are for
  telling a 3% regression from noise; these numbers are read for their shape.
  The random source is a PCG32 rather than Rust's ChaCha8, seeded the same
  12345 — reproducible against itself, not against a Rust run, which would
  have meant porting `rand`'s uniform sampling for numbers a different machine
  produced anyway.

  What they immediately found: `grid/wide/316x316` took **94 seconds**. Taffy's
  own published table has that case at 104 ms. The rest of the port was within
  the constant factor you would expect — `grid/deep/2x2/1024` at 9.9 ms
  against their 1.7 ms, `superdeep/1000` at 8.2 ms against 2.0 ms, all of it
  scaling linearly — but wide grids grew quadratically: ten times the cells,
  two hundred times the time.

  It was the sort. Rust's `sort_by_key` is stable and grid placement depends
  on that, so the port used an insertion sort, with a comment reasoning that
  "a grid's item list is short". That is true of every grid anyone writes by
  hand, and true of all 65 story pages, and false of a 316x316 one, which has
  99,856 items. `StableSort` is a bottom-up merge over insertion-sorted runs
  now — the shape of Rust's own `slice::sort`. 94 s to 0.30 s, and the curve
  is straight.

  Worth naming as a category: that is a bug no test in this tree could have
  caught, because every test and every screenshot is the size a person would
  draw. It took an input built by a machine to be big. The benchmarks are
  where that class of mistake surfaces, which is the argument for keeping them
  runnable rather than reading a number once.

  One harness detail. Layout recurses once per level, and `superdeep` nests a
  thousand of them, which overflows Windows' 1 MB default stack — Rust's main
  thread gets 8 MB. The bench binary links with `/STACK:8388608`, the same
  8 MB; reserve is address space, and pages commit as the stack grows. Nothing
  else needs it, and no real element tree comes close.

  Also fixed on the way past: `AGENTS.md` listed `src/taffy/taffy.md`, a file
  that does not exist.
- 2026-08-21: `min-height: auto`, and the screenshots that were never taken.
  `markdown_table` rendered every block of the document on top of every other
  one: a heading four pixels tall with three lines of paragraph drawn through
  it, the whole document crammed into one screen. The story gallery's
  Introduction page, which is the same markdown renderer, did the same.

  The cause is one line of yesterday's entry, the second of the two "deliberate
  deviations": `minW` / `minH` mapped to a length of zero rather than `auto`.
  Zero is a floor a flex item can be shrunk to. `auto` is CSS's default, and
  it means an item may not be shrunk below its own content. A markdown page is
  a column of paragraphs inside a box the size of the window, taller than the
  window because that is what a scroll container is for; with the floor at
  zero, flexbox reads that as a deficit to share out and squeezes every block
  until the column fits. The text still paints at its own size, so it paints
  over its neighbours. `minW`/`minH` are `auto` now when unset, which is what
  Rust's `Style::to_taffy` passes and what CSS says, and the document renders
  the way it did before the port.

  How it survived yesterday: `cmd/shot.ts` did not build. It ran whatever
  binary was already in `out/rel/`, so the "old engine" and "new engine"
  captures of the acceptance sweep were, for every target that had not been
  rebuilt in between, the *same binary photographed twice*. That is why all 29
  pairs came out identical to the byte — not a strong result, no result at
  all. It now builds the target first, the way `cmd/test.ts` does, with
  `-nobuild` for a caller that has just built or wants an older binary on
  purpose.

  Redone properly — build, shoot, build, shoot — the port is *not* pixel
  identical to the engine it replaced, and this entry is the correction of
  that claim. `hello_world`, `app_assets`, `dialog_overlay`, `root_borderless`
  and `tooltip_top_edge` match exactly. The rest differ, most of them by a
  sub-pixel drift in one direction, and four by something real:

  - `table_in_scrollable` squashes its 400px filler to 112. Ground truth says
    this one is *ours to answer, not taffy's*: the same tree run through the
    Rust crate squashes it identically. GPUI's `div()` is `display: block`,
    and a block container does not stretch or shrink its children; every
    element here is a flex container, because `gpui::Style` has no display
    property at all. That is the next thing to fix and it is a real piece of
    work.
  - `rich_text` lets its content run past the right edge instead of wrapping —
    a min-content width larger than the window, which nothing is allowed to
    shrink. Better than it was before this fix (33% of pixels differing, now
    13%) and still wrong.
  - `focus_trap` and the DataTable story page differ in the same family.
  - The sub-pixel drift is `snap_measured_size_to_device_pixels`: GPUI ceils
    every measured leaf to the device pixel grid before handing it to taffy,
    and `LayoutMeasure` hands over the raw float. That is one line and it will
    move a pixel on nearly every page, so it wants its own change and its own
    sweep.

  Fixed on the way past, and the same class of bug: the story gallery's
  sidebar was 180 wide instead of 255, because it is a plain flex item and the
  content pane's minimum was squeezing it. Rust puts it in a
  `resizable_panel()`, whose width is not up for negotiation; here it says
  `Shrink0()`. The gallery pages went from 12.9% of pixels differing to 3.2%.

  `tests/TaffyTests.cpp` gained the case this turned on: a scroll container
  holding a column of fixed-height children, whose numbers came from running
  the same tree through the Rust crate. The C++ engine already matched it —
  the port is faithful, the translation into it was not.
- 2026-08-21: `div()` is a block container, and the four things that came
  loose when it became one. `gpui::Style` had no display property: every box
  in the tree was a flex container, because the old engine only knew how to be
  one and the taffy port kept that by hardcoding `taffy::Display::Flex`. GPUI's
  `div()` is `display: block`, `.flex()` is what turns the flex model on, and
  `h_flex()`/`v_flex()` are `div().flex().flex_row()` / `.flex_col()`. `Style`
  carries a `Display` now, defaulting to `Block`, and `FlexRow`/`FlexCol`/the
  new `Flex` set it.

  The one deviation, and it is deliberate: the alignment setters —
  `ItemsCenter`, `JustifyBetween`, `Gap` and their neighbours — also turn the
  flex model on. Rust does not do that; `div().items_center()` there is a
  block container with a property that does nothing. But a Rust caller who
  wants a row writes `h_flex()`, and this tree's callers wrote the alignment
  instead, because until now every box was already a flex container. Reading
  the intent from the alignment is what keeps those several hundred call sites
  meaning what the person who wrote them saw. A bare `Div()` — no alignment,
  no gap — is the block container `div()` is, and that is the case that
  changed.

  `table_in_scrollable` is what this was for. Its page is
  `div().size_full().overflow_y_scrollbar()` holding a `v_flex` column of 400
  + 300 + 800, which is taller than the 700 window and meant to be: that is
  what the scroll container is for. As a flex container the page had a deficit
  to share out, so it squashed the 400px filler to 112. As a block container
  it does not stretch or shrink anything, the column overflows, and the page
  scrolls. Ground truth agreed the engine was right and the translation was
  wrong — the same tree through the Rust crate squashes it identically.

  What else moved, from a sweep of 15 examples and all 65 story pages against
  the same tree before the change. 4 examples and 16 pages differ, and every
  one of them differs by getting closer to the Rust:

  - `rich_text`'s table fits its three columns instead of running the third
    one off the right edge, and its paragraphs wrap. This was on the list as
    its own bug; a block container is the answer to it too.
  - The Form page's fields fill the row, which is what a full-width form
    field is.
  - `date_picker`'s sections stack. They say `.v_flex()` in the Rust and
    `FlexCol()` here, and were being laid out as several columns side by side
    — flex-wrap in a column direction, wrapping because the height was
    constrained. Nothing constrains it now.
  - `avatar`'s two groups stack, for the same reason and the same
    `.v_flex()`.
  - `virtual_list`'s rows have the `gap_1` between them that the Rust asks
    for. They were being shrunk flush against each other.

  `system_monitor` also differs, on the CPU line, because it is live data.
- 2026-08-21: an image is not measured, it is sized. `rich_text`'s inline
  picture grew on every layout pass: 120x80 in the first, 158.8x105.9 by the
  last, and the wrapping row it sat in kept the height it had computed before
  the growth, so the line under it was drawn outside the paragraph's box and
  the heading that followed sat on top of it.

  The loop: taffy hands a stretch-aligned item the container's cross size as a
  known dimension while it works out the item's flex base size — that is what
  the spec asks for. `LayoutImageSize` read that known height as if it were a
  height the document had asked for, answered with the width the aspect ratio
  implies, and that width came back as the base size the next pass stretched
  again.

  gpui does not measure images at all. `Img::request_layout`
  (crates/gpui/src/elements/img.rs) reads the decoded bitmap, stamps
  `aspect_ratio` on the style, and fills in whichever of width and height was
  auto — from the other one when that one is an absolute length, from the
  bitmap otherwise — so by the time taffy sees the node both axes are
  definite and there is nothing to feed back. `PrepareEl` does the same now,
  and `gpui::Style` carries the `aspect` that goes with it.

  An image that cannot be decoded has no size to resolve and stays a measured
  leaf, so its alt text is measured as the text it is. That is this tree's
  stand-in for gpui's `fallback` element.

  The Tree story page gained by it too: its file rows are images, and the list
  had been stopping eight items in rather than filling the box.
- 2026-08-21: `min-width: auto` is not `min-width: 0`, and the DataTable page
  that needed both told apart. The story gallery's content pane came out 4944
  wide inside a 1614 window — the width of a 45-column table, carried all the
  way up — so the page's toolbar and the right half of its status line were
  laid out off the side of the screen, and the footer was pushed off the
  bottom.

  `Style` kept `minW` / `minH` as plain floats defaulting to zero, and
  yesterday's fix mapped "zero" to `auto` so that an element which had never
  named a minimum got CSS's content-based one. That is right for an element
  that never named one and wrong for the several dozen in this tree that say
  `MinW(0)` — Rust's `min_w_0()`, the idiom for "this may shrink past its
  content", and exactly what the content pane says. The two cannot share a
  sentinel: they default to `kAuto` now, and an explicit zero is a length of
  zero.

  The cells of that table were empty as well, each with a one-pixel smear
  where its text belonged. The text cache keys a run by its width only when
  the run wraps — a run that cannot break is the same size whatever width it
  was measured against, which is true of its size and false of the shaped run
  the platform hands back, because that is laid out inside a box of that
  width and drawn clipped to it. A truncating cell asked for its min-content
  width got shaped at one pixel, and every later ask found that run in the
  cache. Non-wrapping runs are shaped unconstrained now, which is what makes
  the key's premise true; `truncate` was already doing its own cutting at
  paint time, against the box layout settled on.

  `focus_trap` was on the list of four and needed nothing: its trap boxes
  have `p_4` in the Rust and had been drawing their buttons outside the box
  they belong to. The taffy port fixed that one on its way past, and the
  screenshot diff was the fix, not the bug.

- 2026-08-21: The `markdown` crate, ported. `src/markdown/` is a C++ port of
  markdown-rs 1.0.0 — the version `crates/ui/Cargo.toml` asks for — and
  `component::TextView` now parses through it, folding the mdast into the
  `MdNode` tree exactly as `crates/ui/src/text/format/markdown.rs` folds the
  crate's with `ast_to_node`. `ext/md4c` is gone, and with it the tree's last
  vendored library: hard rule 3 now says there is none.

  This is the same reversal taffy was, for the same reason. md4c was a good
  CommonMark parser and it was not *this* parser, so every question about what
  a document means had two answers — GitHub's dialect as md4c reads it, and
  whatever markdown-rs does — and the second one is the only one that matters,
  because it is what upstream renders. The differences were not theoretical:
  md4c has no footnotes at all, drops a tight list item's paragraph (the tree
  shape `text/node.rs` expects), hands entities over undecoded, and reports
  table alignment per cell where mdast reports it per column. All of that is
  now what the Rust says it is.

  The port is 25 files and about 13k lines, a fifth of it the crate's own
  tables transcribed: the tokenizer and its attempt machinery, the 317 states of the state machine (one C++ function each,
  named as the crate's `StateName` names them), the 48 constructs grouped into
  seven `construct_*.cpp`, the resolvers, and `to_mdast`. `src/markdown/readme.md`
  has the file-for-file map. MDX is not ported — `TextView` never turns it on —
  and dropping it takes `to_mdast`'s only failure mode with it, so `ToMdast`
  returns the tree rather than a `Result`. Two of MDX's fingerprints are kept
  anyway, because they are visible with MDX off: a flow line starting with `e`,
  `i` or `{` jumps straight to content, so a GFM table whose header row starts
  with one of those bytes is not a table — in the crate and here alike.
  `to_html` is not ported either; nothing here renders HTML text.

  **Checked against the Rust, not against a reading of it.** A scratch cargo
  project holding this exact crate version dumped its event stream and its
  mdast for 3283 documents — every `.md` in this tree and in
  `.work/gpui-component`, an edge-case file, and 3000 generated from fragments
  (containers, tabs, CRLF, punctuation soup) — and this port produced the same
  events for all 3283, byte for byte, and the same tree for every one
  markdown-rs can parse. It found the `e`/`i`/`{` rule above, which no amount
  of reading had. On 35 of the generated documents markdown-rs panics
  (`- ~~~~
1. ~~~ meta` is the shortest); nothing here asserts, so those
  produce a tree instead of a crash.

  1.5 ms for the 13 KB story README, against 2.1 ms for markdown-rs built
  `--release` on the same machine — so a page costs what it costs Rust, and
  thirty times what md4c charged. The parse cache in `ui/text.cpp` already
  existed for exactly this and is why it does not show.

  `tests/MarkdownTests.cpp` ports the crate's in-`src` test modules and adds an
  end-to-end check per construct; the ~8000-case CommonMark suite lives in the
  crate's `tests/` directory, which the published crate does not carry — the
  same gap `port-upstream.md` records for taffy. 6878 checks.

  Two things the port keeps that look like bugs, both deliberate, both
  commented where they apply: `normalize_identifier` collapses whitespace to
  nothing rather than to a space when the value's first word starts at offset 0
  (markdown-rs does, whatever its doc comment claims, and a reference has to
  match the definitions it matches), and the character reference table is
  sorted here where the crate's is nearly-but-not-quite ascending, so the
  lookup can binary search 2125 entries instead of walking them.

  One thing removed on the way past: a dead `static float Clamp` in
  `gpui.cpp`. It compiled only because md4c sat at the tail of the amalgam
  with `#pragma warning(push, 0)` and no pop, which turned C4505 off for the
  whole translation unit. Taking md4c out turned the warning back on.
- 2026-08-21: The device-pixel snap, measured and not taken. The taffy entry
  listed a fourth thing to fix: gpui ceils what a leaf measures to the device
  pixel grid (`snap_measured_size_to_device_pixels`) and `LayoutMeasure` hands
  over the raw float, so the sub-pixel drift between this tree's screenshots
  and the engine it replaced looked like that missing ceil.

  It is not. Ceiling the measure was written, built and swept: 65 story pages
  against a build of `d270de4`, the last commit before the port. Every one of
  the 65 moved *further* from it, and the pixels that differ went from 324,663
  to 1,001,435 — three times worse, with no page improved.

  The reason is that gpui's snapping is not a step, it is a coordinate system.
  `Window::layout` multiplies the available space by the scale factor going
  in, `Style::to_taffy` rounds every authored length — borders, padding, gaps,
  explicit sizes — in that same space, and bounds are divided back out on the
  way to paint. Taffy sees whole device pixels for everything. Ceiling only
  what a leaf measures imports a quarter of that: text boxes quantised while
  the padding, gaps and borders around them keep their fractions, which is a
  new kind of wrong rather than less of the old one.

  So the drift stands, and the entry above it was wrong about the cause. What
  would actually close it is adopting the device-pixel layout space whole —
  the transform on available space, the rounding in `ToTaffyStyle`, the
  division on write-back — which is its own change with its own sweep, and
  worth doing only with a reason better than "the screenshots differ from an
  engine that is gone". `LayoutMeasure` carries the finding in a comment so
  the next reader does not spend the afternoon rediscovering it.
- 2026-08-21: `namespace base`, so the two ported crates depend on it and
  nothing else. `src/base.h` / `src/base.cpp` and the three platform halves —
  `Str`, `Vec`, `Arena`, `Func0`/`Func1`, `fmt`/`logf`, the `Plat*` shims —
  moved out of `namespace gpui` into `namespace base`.

  The reason is `src/taffy` and `src/markdown`. Both are ports of crates that
  have never heard of gpui, and both were already written that way in spirit:
  each included `base.h` and its own headers and nothing else, and reached for
  `gpui::Str`, `gpui::Arena` and `gpui::Alloc` only because that was where the
  base happened to live. Now the name says what the dependency is. Each names
  `base::`, and a reader can check the port against the Rust — or lift it out
  of this tree entirely — without deciding what part of gpui came with it.

  gpui is the one module that treats the base as its own vocabulary, so
  `gpui.h` takes the whole namespace in with a using-directive. Its own code
  goes on writing `Str` unqualified, and qualified lookup still reaches through
  the directive, so `gpui::Str` outside names what it always did and no example
  or test changed. The one thing that had to move is a definition:
  `examples/AppLog.cpp` implements the `log` the base declares, and a
  definition has to name the namespace the declaration is in.

  `cmd/build-dist.ts` fails the build if either directory includes anything but
  `base.h` and its own headers, or names `gpui::` outside a comment. That guard
  is not decoration: the amalgam compiles the whole of `src/` as a single
  translation unit, so the compiler would never notice the day one of them
  reached across. Checked by breaking it on purpose before trusting it.

  Verified: `bun cmd/build.ts -rel -all`, `-dbg -all`, `bench`,
  `bun cmd/test.ts` (6880 checks), and screenshots of `hello_world`, the
  DataTable story page and the Introduction page, which is the markdown port
  end to end.
- 2026-08-21: One point, one size, one edge-set. `SizeF`, `PointF` and `RectF`
  moved from `src/taffy/geometry.h` into `base.h`, and gpui's `Size`, `Point`
  and `Edges` are aliases for them. There were two sets of the same three
  shapes before, converted at the seam between the two modules; there is one
  now.

  What stayed a method is what both sides use: the fields, `Zero`, the
  comparison and arithmetic operators, and `Rect`'s three axis sums. What
  taffy does with a flex direction or a writing-mode axis — `Main`, `Cross`,
  `SetMain`, `WithCross`, the `Rect` edge pickers, and the axis-free `Max`,
  `Min`, `Transpose` and `IntoSize` that only taffy calls — is a free function
  in `namespace taffy` now, because `FlexDirection` is taffy's and has no
  business in the base. `size.Main(dir)` reads `Main(size, dir)`.

  `SizeF` keeps gpui's `.w` / `.h` rather than Rust's `width` / `height`. That
  is the field naming the tree already had everywhere, and taking it made the
  gpui side of this change fourteen lines instead of eighty-five — the whole
  of it is the four assignments at the taffy seam and three `Edges` accessors
  renamed. taffy's own `SizeFOpt`, `SizeDim` and `SizeAvail`, which are not
  this type, still spell their fields out.

  The rename across ~150 call sites was driven by the compiler rather than by
  a regex: build, read the `C2039: 'Main' is not a member of 'base::SizeF'`
  lines, rewrite exactly those sites, repeat. That matters because `Bounds`
  also has `.w`, and `SizeFOpt` also has `Main` — a textual pass would have
  hit both. It is self-checking too: where the rewriter over-reached on a line
  holding two types, the next round failed naming the other one, which is how
  the nine mixed-type overloads in `math.h` (`MaybeMax(SizeFOpt, SizeF)` and
  its neighbours) were found.

  One real trap, caught by a test: `Edges` was `{top, right, bottom, left}`
  and the shared `Rect` is `{left, right, top, bottom}`, so a braced
  initialiser silently changed meaning. `InspectorTests` failed on `pad.left`
  and pointed at the two places that did it — that test and
  `UiTableCellPadding`, which is button and table-cell padding. Both name the
  order now through `Edges::New(l, r, t, b)`.

  Verified as a pixel-for-pixel no-op: 65 story pages and 28 example captures
  against a build of the commit before it. Five differed and none of the five
  was this change — `skeleton` and `spinner` animate, `system_monitor` is live
  CPU, and `tree` and the Form page reproduced identical once the two shots
  came from the same directory instead of a worktree and the main tree.
- 2026-08-21: `cmd/imgdiff.ts`, because the sweep was not what was slow.
  Comparing one sweep took six and a half minutes and taking it took eighty
  seconds. The comparison was a per-pixel loop in PowerShell — 4.1 s an image
  pair — and two thirds of that was spent proving identical files identical.
  The encoder is deterministic, so a byte compare answers most pairs outright.
  `bun cmd/imgdiff.ts A B -skip=32 -bbox` does the same 93-pair job in 0.2 s,
  a thousand times faster, and prints the bounding box, which usually names
  the widget that moved. It carries a 40-line PNG decoder: the only PNGs it
  sees are the ones `cmd/winapi.ts` writes.
- 2026-08-22: `Option<f32>` is a NaN-tagged float. `taffy::Optf` was a
  `{ float, bool }` pair: eight bytes for one bit, and it doubled every
  `Size`, `Point` and `Rect` of them layout carries. It is `using Optf =
  float` now, with one reserved quiet NaN — `0x7fc0beef` — standing for
  `None`, which is V8's NaN tagging with nothing in the payload. `SizeFOpt`,
  `PointFOpt` and `RectFOpt` become aliases of `SizeF`, `PointF` and `RectF`,
  so `Size<Option<f32>>` and `Size<f32>` are one type and nothing converts
  between them; the methods that were on them are free functions, the way
  `Main` and `Cross` already were.

  Reserving one bit pattern rather than "any NaN" is the whole correctness
  argument. taffy's own arithmetic can make a NaN — `INFINITY - INFINITY` in
  `MaybeSub`, a zero aspect ratio — and `F32Min` / `F32Max` have a rule for
  one, which is why they exist. Reading every NaN as `None` would quietly
  change what layout computes; reading exactly one does not, because nothing
  produces that payload (x86 makes `0xffc00000`, and propagation copies an
  operand's).

  The alias is what collapsed `math.h`. Rust's `MaybeMath<In, Out>` trait was
  three overload families here — `Optf op Optf`, `Optf op float`,
  `float op Optf` — and they are one function each now, because a plain float
  *is* an `Optf` that is always `Some`, so the None-left arm never fires for
  it. Same for the size-wise ones: nine mixed `SizeFOpt` / `SizeF` overloads
  became five. That is also the answer to "make it a distinct typedef so a
  mismatch is caught": a wrapper would put the three families back and, on
  MSVC x64, pass a one-float struct in an integer register. The two being
  interchangeable is the point of the tag.

  Two traps, both silent rather than loud. A `SizeFOpt` used to default to
  `{None, None}` and now defaults to `{0, 0}`, so every declaration that
  relied on that says `SizeFOptNone()`; and `None == None` is a NaN
  comparison, so the tests compare with `OptfEq` / `SizeFOptEq`. Everything
  else was compiler-driven the way the `SizeF` merge was — build, read the
  `C2039: 'width' is not a member of 'base::SizeF'` lines, rewrite exactly
  those sites, repeat — which is what separates a `SizeFOpt.width` from a
  `SizeDim.width` on the same line.

  Faster by more than the byte count suggests, because the shrink is in the
  values the hot loops copy: flexbox 20-30%, grid 15-20%, tree creation and
  the markdown control group unchanged.

  | `bun cmd/bench.ts -n=20`, median | before | after |     |
  | ------------------------------- | ------- | -------- | ---- |
  | flexbox/huge nested, 10000      | 9.02 ms | 6.04 ms  | -33% |
  | flexbox/wide tree, 10000        | 12.61 ms | 9.70 ms | -23% |
  | flexbox/deep auto, 10000        | 33.53 ms | 22.48 ms | -33% |
  | flexbox/deep random, 10000      | 13.05 ms | 9.61 ms | -26% |
  | flexbox/super deep, 100 levels  | 1.04 ms | 0.764 ms | -27% |
  | grid/wide 316x316               | 299.2 ms | 249.6 ms | -17% |
  | grid/deep 2x2, 16384            | 147.7 ms | 122.9 ms | -17% |
  | grid/deep 3x3, 6561             | 48.87 ms | 39.65 ms | -19% |
  | grid/superdeep, 1000 levels     | 7.98 ms | 7.48 ms  | -6%  |
  | tree creation, 100000           | 35.19 ms | 34.41 ms | ~0   |
  | markdown/parse prose            | 8.80 ms | 8.93 ms  | ~0   |

  Verified: `bun cmd/test.ts -rel` (6880 checks, taffy's own suite among
  them), `bun cmd/build.ts -rel -all`, and 17 example captures against a build
  of the commit before it — 15 pixel-identical, and the two that differed
  (`system_monitor`, `fps_monitor`) differ the same way when the same binary
  is shot twice, because they draw live CPU and frame timings.

- 2026-08-22: The markdown edit map is no longer quadratic. `util/edit_map.rs`'s `add` scans the entries it holds for one at the same index and its `consume` sorts them by insertion sort; a GFM table adds one entry per cell, so a document of tables paid for its cell count twice over — 16 KB / 64 KB / 256 KB of tables read at 7.2 ms / 65 ms / 1.0 s, and a megabyte took 22 s a sample, which is why the bench capped that one shape at 256 KB. `add` now finds its entry through an open-addressed table keyed by `at`, and `consume` merge-sorts. Entries, order and events are unchanged (no two entries share an `at`, so a stable sort lands where `sort_unstable_by` would).

  One trap worth writing down: the first hash was `(at * 2654435761) >> 16` masked to the table, which varies in only 16 bits. Under 65 k entries it looked like a fix — 65 ms to 14 ms at 64 KB — and above that every entry piled onto one probe chain, so 256 KB stayed at 550 ms and the win read as "there must be a third quadratic". There was not; the fold (`h ^= h >> 15`) is the whole difference between 550 ms and 61 ms. The table shape now takes the same 1 MB `-large` size as every other shape.

  | `bun cmd/bench.ts -small -large markdown`, median | before | after |     |
  | ------------------------------------------------- | -------- | -------- | ---- |
  | tables, 16 KB                                     | 7.18 ms  | 3.57 ms  | -50% |
  | tables, 64 KB                                     | 65.13 ms | 13.66 ms | -79% |
  | tables, 256 KB (the old `-large` size)             | 1010 ms  | 60.8 ms  | -94% |
  | tables, 1 MB                                      | 20.4 s   | 249 ms   | -99% |
  | prose, 1 MB                                       | 265.6 ms | 141.6 ms | -47% |
  | nested quotes and lists, 1 MB                     | 640.8 ms | 161.2 ms | -75% |
  | character references, 1 MB                        | 128.2 ms | 99.6 ms  | -22% |
  | prose, 64 KB                                      | 9.06 ms  | 8.68 ms  | -4%  |

  The 1 MB rows are the merge sort, not the index: the map is one entry per resolver edit for those shapes, and the sort was what grew. Verified with `bun cmd/test.ts` and `-dbg` (6880 checks each).

- 2026-08-22: `ArenaVec` is a list of segments, not one block that reallocates. An arena hands back only the allocation on top of it, so the old flat grow — `VecRealloc` into the arena, double the capacity, copy — abandoned every block it outgrew: an n-element build cost about 2n element copies and left about 2n elements' worth of arena behind it. Elements now live in `ArenaVecSegment`s of 4, 16, 64, then doubling, linked oldest to newest, with `first`/`last`/`len` in the handle; nothing is ever copied or abandoned, and a `T*` or a `T&` taken from the vec stays good across an append, which the flat version could not promise. Parsing 64 KB of markdown takes 1589 KB of scratch arena where it took 3257 KB, in 6727 allocations where it took 7713.

  Two things the design has to answer for. Segments are not contiguous, so `.els` is gone — the three callers that hand the elements to something taking a `const T*` (two `EditMapAdd`s, the Sankey generator) call `Flatten`, which returns the segment's own array when there is only one, which is nearly always. And indexing has to walk, so the first segment carries a `cache` pointer to the segment an index last landed in: walking i, i+1, i+2 stays inside one segment and costs the compare a flat array's bounds check would. `.len -= 1` is `Pop()`, which hands the room back to an earlier segment and keeps the emptied ones linked, so a pop and a push at a segment boundary allocate nothing.

  What the benchmark decided, twice. The progression started at 16/64/256 and the cache lived in the handle, and that lost to the flat version by 8-10% on the shapes with the most vecs. Both halves of that were the handle's size, not the walking: an `EditMap::Entry` is two ints and one `ArenaVec`, and a table adds one entry per cell, which the map then sorts and walks. Padding the *old* flat handle from 16 to 32 bytes and changing nothing else reproduced the whole regression — 14.65 ms to 16.30 ms on `gfm tables`. So the handle went back to three words with the cache moved into the segment header, and the first segment to 4 elements, which is what most of these vecs ever hold. Both are marked in `src/base.h` as benchmark numbers to be revisited the same way.

  | `bun cmd/bench.ts markdown -n=15`, median of 3 interleaved runs | before | after |     |
  | --------------------------------------------------------------- | -------- | -------- | ---- |
  | parse prose, 64 KB                                              | 8.79 ms  | 8.63 ms  | -2%  |
  | parse gfm tables, 64 KB                                         | 14.11 ms | 13.67 ms | -3%  |
  | parse character references, 64 KB                               | 6.30 ms  | 6.01 ms  | -5%  |
  | parse nested quotes and lists, 64 KB                            | 9.78 ms  | 9.81 ms  | ~0   |
  | tokenize, 64 KB                                                 | 7.82 ms  | 7.72 ms  | -1%  |
  | to_mdast, 64 KB                                                 | 0.378 ms | 0.414 ms | +10% |
  | parse prose, 1 MB                                               | 141.5 ms | 134.8 ms | -5%  |
  | parse gfm tables, 1 MB                                          | 254.8 ms | 241.1 ms | -5%  |
  | scratch arena, 64 KB prose                                      | 3257 KB  | 1589 KB  | -51% |

  `to_mdast` is the one row that went the other way: it is `DelveMut` walking `node->children[stack[i]]` per event, two indexed reads that were a load and an index each, and an mdast `Node` is 8 bytes bigger for the wider handle. It is 4% of a parse and the parse it is part of got faster.

  Taffy is the control — it uses `Vec`, not `ArenaVec`, and none of its rows moved: counterbalanced old/new/new/old runs of `bun cmd/bench.ts flexbox` land within 2% in both directions with no consistent sign.

  `tests/ArenaVecTests.cpp` is new, because everything the tree already had only ever filled vecs small enough to sit in one segment: the boundary, both directions of iteration, `Pop`/`Truncate` handing an earlier segment back its room without spending arena, `Reserve`, `AppendMany`, `Flatten` both ways, and a copy of the handle reading the same elements. Verified with `bun cmd/test.ts` and `-dbg` (9316 checks each) and `bun cmd/build.ts -rel -all` / `-dbg -all`.

- 2026-08-22: `ArenaVecSegment` lost its `els` pointer. The elements were already behind the header in the same arena block, so the field was storing an address that is `this` plus a constant — `Els()` computes it instead, and the header goes from 40 bytes to 32. Interleaved 12-and-12 runs of `bun cmd/bench.ts markdown -n=15` put `gfm tables` at 14.02 ms -> 13.69 ms (-2.4%) and everything else inside ±1%, which is the noise on this machine; the scratch arena for 64 KB of prose goes 1589 KB -> 1538 KB in the same 6727 allocations. Small, and in the right direction on both counts. `HeaderSize()` is a function rather than a constant because a class is only complete inside a member function body, and `sizeof` needs it complete.

- 2026-08-22: `ArenaVec` walks with an iterator instead of a per-vec cache. The segment header carried a `cache` pointer so that `v[i]` in a loop did not restart the segment walk each time; a cursor holding `{segment, index-within-segment}` is the same thing without the state, so the cache is gone, the header is 24 bytes instead of 32, and reading in order is `for (const Event& e : entry.add)`. Stepping is `++idx` and a compare, and only the step that leaves a segment touches a pointer. `Truncate` can leave empty segments linked behind the last one — and `Reserve` can leave an empty one in the middle — so both ends of the walk normalize past anything empty and `end()` is a null segment.

  Every sequential read of an `ArenaVec` in the tree is rewritten on it: the edit map's merge and consume, the document exits, the attention stack clone and `StackEq` (two cursors in step), `DelveMut`'s stack, the mdast children walks in `mdast.cpp` and `src/ui/text.cpp`, and the builders in menu, native_menu, resizable, root, setting, tab and chart. The loops that are not a sequential read of one vec keep their index: several of the builders walk parallel vecs in lockstep (`sizes`/`mins`/`maxs`/`grows`, or an `ArenaVec` beside a raw array indexed by the same number), where a cursor each would be worse to read and the vecs are one segment anyway.

  What the benchmark had to say about the fast path in `operator[]`: dropping it along with the cache cost `to_mdast` 5.5%, because `DelveMut` indexes `children` by what the stack says, which is not in order and cannot walk. `first == last` answers "this vec never left its first segment" from two fields of the handle, and putting it back took that to 1.8%.

  | `bun cmd/bench.ts markdown -n=15`, median of 8 interleaved runs | before | after |     |
  | ---------------------------------------------------------------- | -------- | -------- | ---- |
  | tokenize, 64 KB                                                  | 8.35 ms  | 8.22 ms  | -1.6% |
  | parse nested quotes and lists, 64 KB                             | 10.73 ms | 10.64 ms | -0.8% |
  | parse gfm tables, 64 KB                                          | 14.88 ms | 14.81 ms | -0.5% |
  | parse prose, 64 KB                                               | 9.30 ms  | 9.31 ms  | ~0    |
  | parse character references, 64 KB                                | 6.45 ms  | 6.49 ms  | +0.6% |
  | to_mdast, 64 KB                                                  | 0.439 ms | 0.447 ms | +1.8% |
  | scratch arena, 64 KB prose                                       | 1538 KB  | 1488 KB  | -3%   |

  A wash on time — every row but `to_mdast` is inside this machine's noise — 50 KB of arena, and one less piece of mutable state in a container that is copied by value all over the parser. Verified with `bun cmd/test.ts` and `-dbg` (9628 checks each, the iterator's own cases added to `tests/ArenaVecTests.cpp`), and with nine screenshots — the story pages for tabs, resizable, chart, settings, menu and native-menu, plus `markdown_table`, `rich_text` and `dialog_overlay` — captured against a build of the commit before it and pixel-identical.

- 2026-08-22: icons are byte code, not code and not files. `src/gpui/drawops.h`
  defines a small stream format — a `u16` opcode and its `f32` arguments,
  coordinates in the drawing's own viewBox — and `ExecuteDrawOps` is the whole
  machine: it maps the viewBox onto the box it is given, applies the rotation
  `Transformation::rotate` asks for, and builds one path per Fill/Stroke op.
  Two things feed it. `cmd/svg-to-bytecode.ts` converts `assets/icons` at build
  time into `src/gpui/asset_icons.cpp` — 73 icons, 21,800 bytes, one array with
  an offset and a length per icon and a binary search over the names — so
  nothing under `icons/` is read or parsed while the app runs. `SvgToDrawOps`
  in `src/gpui/svg.cpp` converts any other `.svg` the first time it is asked
  for, and the result is cached as the same bytes.

  What went away: the 640-line `DrawIcon` in `gpui.cpp`, a hand-drawn
  approximation of each lucide icon in `CanvasLine` calls, which existed only
  as the fallback for an app with no assets folder. There is no such app now —
  the icons are in the binary — and the fallbacks were the worse drawing of the
  two (`Moon` was a plain disc, `Github` a filled silhouette, and the stroke
  was a flat 1.6 px at every size instead of the authored 2 viewBox units).

  Two fixes fell out of making the reader and the generator agree.
  `GetAttr` copied an attribute into a 2 KB buffer, so `window-maximize.svg`
  and `window-restore.svg` — traced by a design tool, 2.4 KB of `d` — had been
  drawing about five sixths of their path; attribute values are read as a slice
  of the tag now. And a shape drawn by `<line>` or `<polyline>` was dropped
  entirely from a file whose other shapes named their own colours, because only
  the tags that called `EndShape` were on the per-colour list. Neither is in the
  icon set, which is why neither showed.

  `tests/DrawOpsTests.cpp` is not a port: it is the check that keeps the
  TypeScript generator and the C++ reader saying the same thing. Every icon in
  the generated table is reconverted from its file and compared op for op,
  floats to a thousandth of a viewBox unit — the generator works in doubles and
  rounds once, the reader is float throughout, and an arc lands a couple of ulps
  apart. That test is what found both bugs above.

  Verified with `bun cmd/build.ts -rel -all` and `-dbg -all`, `bun cmd/test.ts`
  (9930 checks), and screenshots of `sidebar`, `showcase`, `app_assets`,
  `story`, `markdown_table` and `rich_text` against a build of the commit
  before, pixel-identical in every one.

  Which of the two answers an asset path is the asset roots' call, not the
  table's: `SvgDrawOpsFor` asks the roots first and only falls through to the
  compiled bytes when no root has the file, which is what Rust's `AssetSource`
  means and what keeps `examples/app_assets` demonstrating something — its own
  `assets/app_assets/icons/{bot,inbox}.svg` still win. Either answer is cached
  under the path, so a name the table serves is not a directory walk every
  frame, and the cache is 128 slots — the icon set and then some — rather than
  the 24 the old one had.

- 2026-08-22: the stories build their layouts the way the Rust does. Four
  things gpui's `Styled` has and `gpui::Style` did not, all of which
  `src/taffy` already implements: **flex-basis** (`Flex1()`, `FlexNone()`,
  `Basis()`), a **gap per axis** (`GapX()` / `GapY()`, `Gap()` still sets
  both), **`FlexRowReverse()` / `FlexColReverse()`**, and
  **`JustifyAround()`** (plus an explicit `ItemsStretch()`).
  `ToTaffyStyle` had been hard-coding `flexBasis = Auto()`.

  The one that mattered is flex-basis. `crates/ui` and `crates/story` never
  write `.flex_grow()` — not once — so every grow in the Rust is `.flex_1()`,
  which is grow 1 **and shrink 1 and basis 0**. All 107 `->Grow()` in this tree
  came from one of those and had been leaving the basis at `auto`, so an item
  kept its content's width and siblings split only the slack. Converting them
  is not cosmetic:

  - `theme-colors` split its two panes by their contents, so the left tree was
    too narrow and the swatch pane fell into two columns; it is now one, as in
    Rust.
  - `settings` ran its right pane past the window edge and clipped every
    control on it.
  - the showcase's component grid had ragged columns; they are even now.
  - `Tab::new().flex_1()` in the tabs story's "Filling Space" gave "About" and
    "Profile" different widths.

  Then the story pages themselves, read against `crates/story` one at a time.
  `section()` in Rust is `h_flex().flex_wrap().justify_center().items_center()
  .gap_4()` and a page styles *it* — `.w_128()`, `.items_stretch()`,
  `.gap_x_2()`, `.v_flex()` — and hands it the widgets directly. Seventeen
  pages had been wrapping the widgets in a row or column of their own and
  styling that instead, which mostly landed in the same place and sometimes
  did not. They now style the section and add the children to it: spinner
  (`gap_x_2` on all five), switch (`items_stretch`, and the two loose switches
  as the section's own children), tabs, table and resizable (`w_full`, which
  is why the tab bars were not filling their pane), slider's colour picker
  (`justify_around`), and the widths on input, menu, number-input, otp-input,
  popover, progress, rating, select, sheet, textarea and toggle. Also
  `flex_row_reverse()` for the sidebar story's right-hand side — it had been
  appending the two panes in the other order — `gap_y_*` / `gap_x_*` in
  separator, and `flex_none()` in accordion.

  Verified against the Rust gallery running beside ours
  (`bun cmd/compare-story.ts -nobuild -rel <page>`) over all 62 pages, plus
  `-rel -all` / `-dbg -all` and `bun cmd/test.ts` in both (9931 checks). Of the
  62 pages only 13 moved at all, and every one of those moved toward the Rust
  shot.

  What is left, and is a different job: `crates/ui` has `flex_1()` in
  `color_picker` (11), `inspector` (7) and `scroll/scrollable` (5) that this
  tree has no counterpart for, because those parts of those components are not
  ported yet. The count mismatch is missing feature, not different layout.

- 2026-08-22: the three `flex_1()` counts the last entry left open, resolved
  one at a time. They were not the same kind of gap.

  **`scroll/scrollable.rs` (5) is not a gap at all.** All five are inside
  `#[cfg(test)] mod tests` — GPUI test views built to pin auto-height and
  max-height parents. There is nothing there to port, and the previous entry
  was wrong to list it.

  **`inspector.rs` (7): three ported, four blocked.** The JSON pane is a
  `v_flex().flex_1().gap_y_3()` whose header is `h_flex().gap_x_2()` with the
  label as the `flex_1` child and the Reset button after it, and whose body is
  a `v_flex().flex_1()` holding an editor at `h(relative(1.))`. Ours had a
  `JustifyBetween` header and a 180px editor; it fills the panel now, and
  `Textarea::H` learned `kFill` so `h(relative(1.))` has a spelling. The
  description list takes upstream's `.label_width(px(110.)).bordered(false)`
  and the Reset button its `.small()`. The other four are the source-location
  Link and the "Rust Styles" pane, and both are out for a reason rather than
  for lack of time: GPUI stamps a caller location on every element and nothing
  here records where an `El` was built, and the Rust pane is a code editor
  with an LSP completion provider behind it — an LSP client is a standing
  non-goal. Said so in the file, where the next reader will look.

  **`color_picker.rs` (11): the whole popover was missing.** What this tree
  had was five hard-coded swatches. It now has what upstream draws: a
  segmented Palette/HSLA tab bar, a featured row of the theme's six base hues
  and their light halves, the 9x11 palette grid, and an HSLA panel of four
  sliders lying on their own gradient tracks with a `min_w_16` label and a
  `w_10` readout either side. The hovered colour and its hex field sit under
  a separator, as they do in Rust. `ColorPickerState` grew the four
  `SliderState`s and the hex `InputState` that Rust keeps on it — the panel is
  rebuilt every frame and the thumb being dragged has to outlive that — plus
  `select_color` / `update_color` / `sync_pending_value` and the handlers the
  widget binds. The state is a keyed entity (`ColorPickerStateFor`), which is
  how this tree spells `Entity<ColorPickerState>`.

  Four things had to exist first. The theme gained `base.<hue>.light` for all
  six hues — the featured row is those twelve, and the resolver had been
  dropping them with a comment saying nothing wanted them. `theme_data.cpp`
  gained `kShadcnStone`, the one palette hue no `ColorName` can reach.
  `component::Slider` gained `WFill()` — the parts are placed by
  `left(relative(..))` instead of by pixels, which is what lets a slider be
  the `flex_1` child of a row, as every Rust slider is — and `Bg()`, the
  `bar_color` the picker sets to transparent so only the gradient shows.
  And `component::Tabs` gained `W()` / `WFill()`, because `TabBar` has no
  width of its own in Rust and every caller says `.w_full()` or `.w_64()`.

  One upstream quirk reproduced rather than fixed: `Tab::flex_1()` does
  nothing on a Segmented, Pill or Underline bar, because `TabBar` wraps each
  of those in a `div().flex_shrink_0().on_prepaint(..)` to measure it and the
  wrapper is what the bar lays out. That is why the picker's two tabs sit at
  their labels' width in upstream and why the tabs story's "Filling Space"
  does not fill — both now render the way upstream renders, and `tab.cpp`
  says why.

  `-gpui-inspector` opens the inspector on the first frame. The panel is
  otherwise only reachable through ctrl-shift-i, which the screenshot harness
  cannot send: it posts messages, and the chord is read from the real keyboard
  state.

  Verified against the Rust gallery beside ours — the picker's palette and
  HSLA panels click-for-click, and the tabs story's "Filling Space" — plus
  `-rel -all` / `-dbg -all` and `bun cmd/test.ts` in both (9948 checks; the
  colour-picker suite gained upstream's hex-parsing, hex-formatting and
  `default_value_reaches_the_hex_field_and_sliders` cases). The 62-page sweep
  moved two pages: `spinner`, which animates, and `form`, by four pixels.
  The inspector is the one thing not screenshot-compared: upstream has no way
  in that the harness can drive either.

- 2026-08-22: the font sizes, where a Rust node names one. GPUI's text scale
  is Tailwind's — `text_xs` 12, `text_sm` 14, nothing 16, `text_lg` 18,
  `text_2xl` 24 — and an element that names none inherits, which for a story
  page is the 16 px base. This tree has to write a number, and where the Rust
  said nothing the port had often written 13, which is not on the scale at
  all. Every one of those with a Rust node behind it is now what that node
  resolves to:

  - `src/ui`: the dock's tab label (`Tab`'s own size, 14), its
    "not registered" panel and its drag preview (both inherit), the dialog
    description (`text_sm`), a table cell (`Table` is `text_sm`), a tiles
    panel title (inherits), and the inspector's two notices (`DivInspector`
    is `text_sm`).
  - the stories: hover-card's trigger and card body (`text_sm`), the tabs
    story's filling-space labels (a tab's size), the gallery's sidebar menu
    label (`SidebarMenuItem` is `text_sm`), the slider story's channel
    captions and the alert-dialog story's body (all inherit), the toggle
    story's chip labels (`Toggle` names no size at Medium) and its
    Ghost/Outline headings (`text_sm().font_medium()`, not 13 semibold), and
    the progress story's two card titles (`font_medium`, so 16) beside their
    `text_sm` status lines.

  What is left at 13 is on the four pages with no upstream counterpart — the
  dock and tiles stories, and one line of the tooltip story — where there is
  no Rust node to resolve against and inventing one would be guessing.

  This was a pass over the sizes that are off the scale, not a node-by-node
  audit of every string in the gallery. A count of Rust text classes against
  ours per page is too noisy to drive more than triage: our port puts text
  inside components where Rust writes it inline, and vice versa, so the two
  distributions differ on 37 pages for reasons that are almost all structure
  rather than size.

  `-rel -all` / `-dbg -all` and `bun cmd/test.ts` in both (9948 checks). The
  62-page sweep moved 62 pages, because the sidebar menu label is on all of
  them; the three that moved by more than a fraction of a percent —
  hover-card, progress and toggle — were compared against the Rust gallery
  and each lines up with it now.

- 2026-08-22: the second half of the layout pass — the element trees, not
  just the styles on them. `section()` in Rust is a wrapping row that the
  page hands widgets to directly; this tree had often built a row or a column
  of its own and put the widgets in that instead. Where upstream really does
  wrap — and it usually does — the port was already right: breadcrumb,
  button's Progress, checkbox's Disabled, clipboard, combobox's Values,
  hover-card's three, icon's Icon Buttons, kbd, label's three, resizable's
  four, select's Values, separator's three, skeleton's two, slider's cards,
  status-bar, table, textarea's first, tree and collapsible all match the
  `h_flex`/`v_flex` upstream puts there. Sixteen that did not, now do:
  eight sections of the input story, four of notification, and one each in
  otp-input, sheet, toggle and tabs. The input story also loses its
  `Centered()` helper — the section body already justifies to the centre, so
  the wrapper existed only to undo the column it was inside.

  Taking the wrapper off the input story turned up a real bug in
  `component::Input`. Its root was a column for the optional label, and the
  *field* inside carried `W(width)` — 100% of a box that is only as wide as
  its content unless a parent stretches it. A column parent did stretch it,
  which is why nobody noticed; in a row every field collapsed to its text.
  The root carries the width now, and there is no column at all unless a
  label was asked for — which is also what Rust's `Input` is: `.flex()
  .size_full()`, with the label a div of the caller's own.

  The popover story's Anchor section is the other shape change: two
  `div().absolute().top_0()` / `.bottom_0()` bands over a `min_h(360)`
  section, rather than a column that spaced them apart. It matters for what
  it is demonstrating — a popover that opens upward needs the room above it
  to be real.

  `-rel -all` / `-dbg -all` and `bun cmd/test.ts` in both (9948 checks). Of
  the 62 pages, the ones that moved are input (now pixel-for-pixel with the
  Rust shot, where before every field was shrink-wrapped), notification (the
  buttons take the section's gap_4 rather than a row's gap_2, as upstream
  does) and tabs (five pixels of scrollbar). Nothing else moved at all.

- 2026-08-22: `to_mdast` stopped storing indexes. `TreeFrame::stack` held the
  child index at each open level and every `tail_mut` walked the tree from the
  root, indexing `children` once per level — which is what the entry above
  called "not in order and cannot walk", and why the `first == last` fast path
  in `ArenaVec::operator[]` had to stay. markdown-rs keeps indexes because
  Rust cannot hold a `&mut Node` into a tree it is still building; C++ has no
  such rule, and an arena allocation does not move, so the stack holds the
  nodes themselves. `DelveMut` is gone: the innermost open node is
  `stack[stack.len - 1]`, one read of a vec four deep, instead of a walk that
  indexed a `children` vec — often a long one — once per level.

  `TailPushAgain` was the other `[]`: it re-entered the tail's last child by
  index. Its one caller already has that child in hand (`OnEnterData` compares
  its kind), so it takes it as an argument now and looks nothing up.

  | `bun cmd/bench.ts -n=15 markdown`, counterbalanced new/old/old/new | before | after |     |
  | ------------------------------------------------------------------- | -------- | -------- | ----- |
  | to_mdast, 64 KB                                                     | 0.417 ms | 0.365 ms | -12%  |
  | parse prose, 64 KB                                                  | 8.30 ms  | 8.32 ms  | ~0    |
  | parse gfm tables, 64 KB                                             | 13.13 ms | 13.19 ms | ~0    |

  The parse rows do not move because `to_mdast` is 4% of one. What this
  recovers is the 1.8% the cursor change cost it, and then some.

  The fast path stays. Dropping it now costs to_mdast 6% (0.365 -> 0.386) and
  `parse prose` 5% (8.32 -> 8.74) — the tokenizer's own edit-map vecs index
  too, and they are what it was really for.

  Verified with `bun cmd/test.ts` and `-dbg` (9948 checks, the markdown suite
  among them) and with `markdown_table` and `rich_text` screenshots against a
  build of the commit before, pixel-identical.

- 2026-08-22: `SeqStrings`, ported from SumatraPDF's `src/base/Str.{h,cpp}`
  into `src/base.{h,cpp}`. A run of NUL-terminated strings laid end to end and
  ended by an empty one — `"red\0green\0blue\0"`, which as a C literal already
  carries the final NUL — with `SeqStrAt`, `SeqStrAdvance`, `SeqStrIndex`,
  `SeqStrIndexIS` and `SeqStrByIndex`. A string here is a `Str` rather than a
  `char*`, so a caller comparing one does not walk it again for its length.
  `SeqStrCount` is not one of Sumatra's: a run that parallels a table has to
  be as long as the table, and something has to be able to say so.

  `AssetIcon` uses it. The struct was `{const char* name, int offset, int
  len}`; it is `{int offset, int len}` now, and the names are one
  `kAssetIconNames` run in the same order — 73 pointers and 73 relocations
  fewer, though the exe is the same size, because six hundred bytes of
  `.rdata` disappear into the section padding. `cmd/svg-to-bytecode.ts` writes
  both, and the row comments still name the icon so the generated file's diff
  stays readable.

  `AssetIconFind` is a `SeqStrIndex` scan rather than a binary search now.
  That is a linear pass over about nine hundred bytes instead of seven
  compares, which costs nothing that matters: an asset path is looked up once
  and `SvgDrawOpsFor` remembers the answer. The drawops test gained the
  invariant the two arrays now depend on — `SeqStrCount(kAssetIconNames) ==
  kAssetIconsCount` — and checks that the name at each index finds that
  index's row.

  `-rel -all` / `-dbg -all` and `bun cmd/test.ts` in both (10168 checks), and
  `sidebar` and `app_assets` are pixel-identical to a build from before, which
  is the icons still coming out of the table.

- 2026-08-22: the markdown tables are `SeqStrings` runs. `constant.cpp` held
  the crate's three lists as arrays of pointers: `Str kHtmlBlockNames[62]`,
  `Str kHtmlRawNames[4]`, and `CharacterReference kCharacterReferences[2125]`
  of `{const char* name, const char* value}` — 2125 entries of two pointers
  each, which is 34 KB of `.rdata` and 4250 relocations for a table whose
  bytes were in the binary anyway.

  The two tag-name lists are scanned end to end for a match, so they are runs
  and nothing else; `NamesContainI` walks one with `SeqStrAdvance` and keeps
  this module's own `StrEqAsciiI`, which lowercases A-Z and nothing else,
  rather than taking `SeqStrIndexIS` and a locale's opinion of what a letter
  is. The references keep a table beside their two runs, but of byte offsets
  rather than pointers, so `DecodeNamed` is still the binary search the sort
  exists for.

  `markdown_table.exe` goes 919,040 -> 888,832 bytes, which is the 17 KB of
  pointers and the relocations that pointed them.

  | `bun cmd/bench.ts -n=15 markdown`, counterbalanced new/old/old/new | before | after |     |
  | ------------------------------------------------------------------- | -------- | -------- | --- |
  | parse prose, 64 KB                                                  | 8.43 ms  | 8.42 ms  | ~0  |
  | parse nested quotes and lists, 64 KB                                | 9.46 ms  | 9.51 ms  | +0.5% |
  | parse gfm tables, 64 KB                                             | 13.21 ms | 13.13 ms | -0.6% |
  | parse character references, 64 KB                                   | 5.97 ms  | 5.93 ms  | -0.7% |
  | tokenize, 64 KB                                                     | 7.67 ms  | 7.65 ms  | ~0  |
  | to_mdast, 64 KB                                                     | 0.374 ms | 0.375 ms | ~0  |

  Every row is inside this machine's noise, `character references` — the one
  that leans on `DecodeNamed` — included, which is the point: the search still
  indexes, only what it indexes into changed.

  `tests/MarkdownTests.cpp` gained the invariant the split created: the two
  runs are walked in step with the table and every entry's two offsets have to
  be where the walk is, so an offset that landed inside a name — which would
  still read as a string, just the wrong one — fails. That is 6379 of the
  16547 checks.

- 2026-08-22: two defects the compare harness had been showing all along.

  **The data table lost its scrolling columns.** Rows 13-15 of the data-table
  story rendered `ID / Market / Name / Symbol` and then nothing, and the
  horizontal scrollbar cut across row 13 instead of sitting under the last
  row. The two panes disagreed about their height: `headWrap` and `gsWrap`
  are rows *inside* the scrolling pane, which is a column, and the flex_1
  sweep had given them a flex basis of zero. A zero-basis item contributes
  nothing to a column's intrinsic height, so the pane measured shorter than
  its heads by exactly their height, and the body — which does not refuse to
  shrink the way the fixed pane's does — gave that height back. Both are
  `W(kFill)` now, which is what a row inside a column means by "fill". Mine,
  from `d8c2f79`: the sweep was right about `flex_1` everywhere it came from
  a Rust `flex_1`, and these three wrappers are this tree's own.

  **The stacked area chart drew one series.** The story built two `AreaChart`
  elements and absolutely overlaid the second, which is not what Rust does
  and did not draw. Rust hands *one* chart two `y` accessors —
  `.y(..).stroke(..).fill(..).name(..)`, once per series — and they share the
  domain, the grid and the tooltip. `ChartSeries` carries a
  `ChartSeriesExtra` list now, `DrawChart` draws the run once per series (so a
  later one paints over an earlier one, as Rust's do), the domain spans all of
  them, and the crosshair drops a dot on each with a line per series in the
  tooltip box. `component::AreaChart::Y()` starts a series and the `Stroke`,
  `Fill` and `Tooltip` after it belong to it, which is the Rust chain read
  left to right. Four series is the cap; the day something wants a fifth the
  array becomes an `ArenaVec`.

  Of the 62 pages only those two moved.

- 2026-08-22: `ui/dock`, enumerated against ground truth rather than against a
  line count. The gallery has no dock page upstream, which is why this module
  had never been compared — but `crates/story/examples/dock.rs` exists and
  builds:

      cd .work/gpui-component
      cargo build --release -p gpui-component-story --example dock

  (from PowerShell, not Git Bash, whose `link` shadows MSVC's). Its window is
  the reference this entry is written against.

  What the comparison says: the port matches in shape. The three docks around
  a centre, the per-group tab strip that scrolls under pinned buttons, the
  drop targets and their placeholder, zoom, the ⋯ menu, the dock toggle
  buttons on the left-most centre group — `update_toggle_button_tab_panels`,
  which we already place the same way — the saved layout and the panel
  registry behind it are all here. The 4143-vs-461 gap is mostly Rust keeping
  a `DockArea`, a `StackPanel` and a `TabPanel` as three entity types with
  `insert_panel_before/after/at` between them; this tree's flat node array
  says the same thing with `DockSplitAdd` and `DockTabsInsert`.

  One hook was genuinely missing and it is what the Rust example's tab bars
  show: `Panel::title_suffix`, the panel's own content in its group's tab bar,
  between the tabs and the group's buttons. `DockPanelDef::titleSuffix` is
  that, and the dock story gives every panel the pair of icon buttons Rust's
  example does — which is also what proves the bar leaves room for one.

  Not a gap, for the record: `Panel::menu_visible` / `toolbar_visible` are
  flags on a trait we express as fields, and the ⓘ/🔍 in every Rust pane are
  the example's own `title_suffix`, not a dock feature.

- 2026-08-22: `text/window_selection.rs` is 2079 lines and **every one of them
  is a test**: line 1 is `#[cfg(test)]`. The entry that put it on the "biggest
  remaining Rust file" list was wrong — that number came from a raw `wc -l`
  while the ranking beside it stripped test modules. There is nothing there to
  port; `src/base/text_selection.{h,cpp}` already has what those tests drive.

  What the file is good for is the list of behaviours upstream guarantees, so
  it was read as one. Most of the fifty cases are about a *virtualized* text
  view — a selection spanning blocks scrolled out of the painted set, and
  exporting them anyway — which does not arise here: a `TextView` builds every
  block and the story shell scrolls it. What did apply and was unpinned is the
  multi-click: `points_for_multi_click` says two clicks take the word and
  three the run, and that the unit asked for is not extended by a drag after
  it. Both are in `WindowSelectionPress` and neither had a test; they do now,
  along with the multi-click that lands on no run at all.

  Still genuinely missing from `ui/text`, and not something these tests cover:
  `TextView::selection_format(Source)` — copying a selection as its Markdown
  source rather than as rendered text. Select-all could return the source
  verbatim cheaply, but a partial selection has to be rebuilt from the parsed
  nodes, and `MdNode` carries no source offsets to rebuild it from. Half of it
  would be worse than none, so it is written down rather than started.

- 2026-08-22: `StrL` for every string literal that becomes a `Str`. `Str` is a
  pointer and a length, and `Str("http")` walks the literal at run time to
  find a length the compiler already knows; `StrL("http")` is
  `sizeof(lit) - 1`. Eleven sites had the constructor — most of them the
  two-argument form with the length counted by hand (`Str("mailto:", 7)`),
  which is the same bytes and one typo away from being wrong.

  How the audit was done, since "grep for `Str("`" only finds the explicit
  ones: `Str(const char*)` was made `explicit` for one build. That turns every
  implicit conversion into a compiler error, and the whole tree — every
  example, test and benchmark — produced exactly **one**: `log("open
  /base/primitives/link")` in the link showcase. The other ten errors were
  inside `base.cpp` itself and were `char*` and `char[256]` variables, where
  the walk is real work and not avoidable.

  The `explicit` was then reverted. `src/base.h` is a vendored subset of
  SumatraPDF's `src/base`, whose `Str(const char*)` is implicit, and diverging
  there costs more than the enforcement is worth — the audit is cheap to
  repeat, and this entry says how.

- 2026-08-22: A remote image is a picture now, not its alt text. The rule that
  said it could not be — "a socket and a TLS stack this tree does not have" —
  is gone from `gpui/image.h` and from AGENTS, and in its place is
  `src/sys/http.h`: one GET, made with whatever client the OS already ships.
  WinHTTP on Windows (`winhttp.lib`, the only library the link line gained),
  NSURLSession on macOS (Foundation was already linked for the window, so it
  cost nothing), libcurl on Linux through `pkg-config`. libcurl is the tree's
  one **soft** dependency, unlike x11/cairo/pango: `cmd/build-linux.ts` probes
  for it, defines `GPUI_HAVE_CURL` when it answers and says so when it does
  not, so a machine without `libcurl4-openssl-dev` still builds and only loses
  remote images. Verified both ways in WSL.

  Nothing blocks. `HttpGet` is the blocking call and only a worker thread ever
  makes it; `HttpFetch` is what a paint asks — a table of 24 slots, at most 4
  transfers at once, answering Pending / Done / Failed and never waiting. A
  Pending answer is the one thing `image.cpp` does not write into its cache,
  since it is not final; a failure is remembered, or a page of unreachable
  pictures would retry every one of them every frame. 16 MB and 15 s are the
  caps, and a body over the cap is refused rather than truncated.

  The repaint took two edits, and the second was the one that mattered.
  `WindowTimerTick` keeps the window awake while `HttpFetchPending()` is
  non-zero and `WindowTimerMs` paces that at 20 Hz rather than 60 — but a tick
  only happens if a timer is already armed, and an idle window has none. So
  `WindowDrawFrame` arms the clock itself when a fetch is outstanding. Without
  that, the story gallery worked (something there is always animating) and
  `rich_text` did not: the badge arrived in 407 ms and sat unclaimed until
  shutdown. Worth remembering as the shape of the bug rather than the bug.

  A remote src still prefers a shipped asset. `ImageAssetFor` looks the URL's
  last path segment up in the asset roots first, so the story's own
  `assets/story/logo.svg` answers upstream's README `<img src="https://...">`
  and no request is made — offline, deterministic, and unchanged from before.
  Only a URL nothing local answers is fetched.

  Three fixes came out of pointing this at real files, each useful on its own:

  - An SVG with no `viewBox` was drawn at 24x24. Every lucide icon has one, so
    nothing had ever hit it; a shields.io badge has `width="86" height="20"`
    and nothing else, and got scaled by three and a half. The viewport is the
    fallback the spec already names.
  - Shapes inside `<defs>` / `<clipPath>` / `<mask>` / `<filter>` / the
    gradients were painted. A badge names a clip rect it never uses, which is
    how a 20-pixel badge grew a stripe down the page. `IsHiddenContainer` plus
    a depth counter in the tag scanner, mirrored in `cmd/svg-to-bytecode.ts` —
    regenerating `asset_icons.cpp` after the change gives byte-identical
    output, which is the proof the icons are untouched.
  - A vector picture was drawn into a square of `min(w, h)` inside its box and
    measured as its alt text. `ImageNaturalSize` answers the viewBox when
    there is no bitmap, so an image element is laid out at the picture's own
    aspect, and `SvgDrawOps` takes a `w` and an `h` — `ExecuteDrawOps` already
    scaled the two axes independently. `SvgDraw` still passes a size twice,
    because an icon is square.

  What is still missing is `<text>`: the three badges on the Introduction page
  now render as the right rounded rectangles in the right colours with no
  words in them. That is the vector renderer's limit, not the fetch's, and it
  is the same limit a shipped `.svg` has always had.

  `HttpSetEnabled(false)` is the first line of `TestsMain`, so no suite ever
  touches the network; `tests/HttpTests.cpp` checks the scheme rules and that
  the switch is honoured. The live path was checked by hand on all three
  platforms with a throwaway probe — 200, 1277 bytes, `image/svg+xml` from
  WinHTTP, NSURLSession and libcurl alike. 16567 checks on each.

- 2026-08-22: Three more in the SVG reader, all of them older than the fetch
  that turned them up and none of them reachable from `assets/icons`, which is
  why the icon table never noticed.

  `transform=` was read off nothing at all. A `<g transform="translate(..)">`
  moved its contents nowhere, and the only reason the window-chrome icons
  looked right is that `window-close.svg` and its two siblings wrap their path
  in `translate(-120 0)` and `translate(120 0)`, which cancel. `SvgMatrix` is
  the 2x3 affine, `ParseTransform` reads translate / scale / rotate / matrix
  (skewX and skewY are skipped rather than guessed at), a 16-deep stack
  follows `<g>` nesting, and `EndShape` applies the product to the points it
  just emitted — so the byte stream stays a flat list of coordinates and
  `ExecuteDrawOps` never learns a matrix exists. A shape's own `transform`
  applies after the groups'.

  `<g>` fill and stroke are deliberately **not** inherited, and that is the
  trap worth writing down: every window-chrome icon wraps its path in
  `<g fill="#000000">`, so honouring it would make the file a picture with a
  colour of its own and pin the title bar's buttons black instead of letting
  the theme colour them. Correct SVG, wrong result. Left alone on purpose.

  `<ellipse>` was on none of the reader's lists and drew nothing. It cannot go
  through `AddRoundRect` either — one corner radius for two axes makes a
  stadium of anything wider than it is tall — so `AddEllipse` is four cubics
  with a kappa per axis. `<circle>` still goes through `AddRoundRect`, where
  its bytes are what the compiled table holds; the asymmetry is deliberate.

  Only `fill=` was read off a shape, so a picture that named the colour it is
  *drawn* with came out in the caller's. `SvgShape` gained a stroke colour,
  and a shape that names both is two passes over the same points, since one op
  carries one colour and `kOpFillStrokePath` is the single-colour case.

  The proof is in two halves. `cmd/svg-to-bytecode.ts` mirrors all three, and
  regenerating `asset_icons.cpp` gives byte-identical output — 21800 bytes,
  73 icons — so nothing in the shipped set moved. And the new tests in
  `DrawOpsTests.cpp` fail 12 checks against the reader as it was, which is
  what says they are testing the fix rather than agreeing with it.

  Visible result: the GitHub CI badge on the Introduction page. Its label and
  status halves both drew at x=0 and overlapped; now they sit at 0..40 and
  40..90 the way the file says. Correcting an earlier note in this log — the
  badges do *not* all render in their own colours. The two shields.io ones do,
  because they fill with plain hex; the GitHub one fills with
  `url(#workflow-fill)`, a gradient reference this reader cannot resolve, so
  its shapes take the caller's colour and come out as outlines. Resolving a
  `url(#..)` to its first `<stop stop-color>` is the obvious next one and was
  not done here.

- 2026-08-22: Actions and key bindings, seven commits, measured against what
  gpui-component actually uses rather than against gpui's whole surface.

  **An action carries what its binding said.** Rust puts fields on the action
  type — `Confirm { secondary }`, `Enter { secondary, shift }`,
  `SelectScrollbarMode(mode)`, `MenuClick(name)`, and the enum actions the
  data-table story declares — and matches the whole value. An action here is
  the hash of its name, so the port had been spelling one action's two
  payloads as two actions: a `ui::ConfirmSecondary` that exists nowhere
  upstream. `KeyBinding::arg` and `ActionEvent::arg` are the payload; a
  number, a bool or an enum is itself and anything larger is a pointer.

  **A click can dispatch one.** `window.dispatch_action` is called 17 times
  upstream and is how a dialog is wired: the Cancel button dispatches what its
  escape key resolves to, so the dialog declares `on_action` once. The seam is
  `El::OnClickAction`, and it walks the *hit chain* outwards rather than
  reading the element under the pointer — `AlertDialogCancel` wraps a Button,
  so the wrapper that names the action is the parent of what was hit. Without
  that the click reached nothing, which is how it was found. Escape closes the
  showcase alert now, which it never did.

  **Command is not Control.** window_mac.cpp folded them onto one flag, which
  works until a keymap names them apart — and state.rs does: on macOS it binds
  `ctrl-backspace` and `cmd-backspace` to different actions in the same
  context, `ctrl-shift-a` beside `cmd-a`, and `ctrl-cmd-space`, which needs
  both at once and could not be written here at all. `KeyChord::platform` is
  GPUI's `Modifiers::platform`; `cmd-` is that key, `ctrl-` is control, and
  `secondary-` is the shortcut modifier — the platform key on macOS, control
  elsewhere. All three platform layers report it.

  **The field's keyboard is a keymap.** `state.rs::init` is 74 bindings in an
  `Input` key context; here they were a `switch` over the key code with
  `word = ctrl || alt` standing in for the two `#[cfg]` sets. `input_keys.cpp`
  is that function, 71 of its bindings (all but ShowCharacterPalette and
  ToggleCodeActions, which have no counterpart), and `El::BindInput` declares
  the context. Five chords are ours and not upstream's, commented where they
  are bound: ctrl-home / ctrl-end / the two shifted, because state.rs spells
  the document ends cmd-up / cmd-down and binds them on macOS only; and
  ctrl-shift-z, because upstream's only non-macOS redo is ctrl-y.

  The window resolves a chord **once** and hands the answer on. The matcher
  holds a half-finished sequence on itself, so asking twice for one keystroke
  would append that chord twice — `WindowResolveKeyAction` is the front half
  alone.

  **The chord an action is reached by.** `KeymapBindingForAction` runs the
  matcher's own search backwards, so the answer is the binding that chord
  would really fire. `Kbd::ForAction` is the themed half, and a menu row that
  names an action shows whatever is bound to it in the menu's
  `ActionContext` — `menu.action_context(handle)`.

  **The story's Edit menu is real.** It was an inert label with a hover
  background. It is now app_menus.rs's eleven rows, every one naming an input
  action and carrying no handler: choosing one dispatches to whatever field
  has the keyboard, and the shortcut beside it comes from the keymap.

  One visible oddity, kept because it is faithful: the Delete row shows
  Shift+Delete. Both `delete` and `shift-delete` are bound to `input::Delete`
  and the later binding has the higher precedence, which is the rule the
  matcher applies and the rule
  `highest_precedence_binding_for_action` applies upstream. Preferring the
  chord with fewer modifiers would be a display heuristic of our own.

  The macOS run earned its keep twice. It caught the bracket indent pair in
  the modifier split, and then four places where the old switch differed from
  upstream: ctrl-a and ctrl-e are the emacs bindings state.rs adds there
  rather than nothing, cmd-backspace is DeleteToBeginningOfLine where
  ctrl-backspace is Backspace, cmd-y is not a chord, and the document ends are
  cmd-up / cmd-down. The Windows run was green through all of it.

  Deliberately not ported, because gpui-component uses none of them:
  capture-phase action dispatch, `on_key_up`, `NoAction` / unbinding, an
  action registry or command palette, and JSON keymap files — the last is
  Zed's, not this library's. `ShowCharacterPalette` and `ToggleCodeActions`
  have no counterpart here and are the only two bindings of the input's 74
  left out.

  16622 -> 16687 checks on Windows and Linux, 16695 on macOS.

- 2026-08-22: A markdown `Node`'s eight strings are `ArenaStr`, which is an
  offset and a length in one 64-bit word rather than a pointer and a length in
  two. A Node carries all eight whichever kind it is, so that was half the
  struct spent pointing into the arena the node is already in: **232 bytes ->
  168**, and a parse allocates them by the thousand.

  Measured at 64 KB of source, `bun cmd/bench.ts markdown`, with the
  BenchMem row the previous commit added:

    shape         before                after                 arena
    prose         1646.1 KB  25.65x     1285.9 KB  20.04x     -21.9%
    nested        1067.9 KB  16.67x      867.5 KB  13.54x     -18.8%
    gfm tables    2926.0 KB  45.66x     2269.7 KB  35.41x     -22.4%
    entities       660.2 KB  10.28x      626.2 KB   9.75x      -5.2%

  Speed is unchanged, which is the result to want rather than a
  disappointment: reading a string back is now an add rather than a
  dereference, and the block the offset lands in is found by walking the
  chain from the newest — one comparison while the arena is one block, which
  is every parse here. prose 8.44 -> 8.61 ms, nested 9.75 -> 9.53, tables
  12.99 -> 12.73, entities 5.89 -> 5.96, all inside the ~1% the runs move by
  anyway. `to_mdast` alone is 0.38 ms either way.

  The entities shape barely moves because its arena is mostly decoded
  character data, not nodes; the table shape moves most because a table is
  nodes almost all the way down.

  `Keep(c, s)` in to_mdast.cpp is the one thing worth knowing about the
  conversion. StrCat, NodeToString, IdentifierFrom and CharacterReferenceDecode
  all allocate in the parse arena, so storing what they return is a lookup and
  no second copy; a slice of the *source* bytes is not the arena's and has to
  be copied in. Which of the two it is cannot be got wrong at a call site
  because `Keep` asks the arena rather than the caller — `ArenaStrRef` first,
  `ArenaStrDup` when that finds nothing.

  What did not shrink, and is the next thing to look at if this matters more:
  `OnExitData` is `node->value = StrCat(a, node->value, value)` once per data
  event, so a text node of N events leaves N partial concatenations behind in
  the arena. That is where prose's remaining 20x lives, and it is markdown-rs's
  shape as much as ours.

  Verified beyond the suite: `rich_text` renders pixel-identical, which
  exercises every one of the eight strings — value, url, title, alt,
  identifier, label, lang and meta — through TextView.

- 2026-08-22: The accumulating strings in `to_mdast` grow in place, and the
  character-reference decode stopped allocating. Two changes, and a correction
  to what the entry above said.

  **The correction first.** That entry called `OnExitData`'s repeated
  `StrCat` the place "where prose's remaining 20x lives". It is not. Counting
  the appends in a 64 KB parse says so:

    shape       fresh   grew   copied
    prose        3012      0        0
    nested        610   1830        0
    tables       4725      0        0
    entities      272   1904     2176

  For prose and tables *every* append starts a string that was empty — a Text
  node there gets one Data event, because emphasis, code spans and links break
  the runs — so there was never anything to concatenate and nothing quadratic
  to fix. Their memory did not move by a byte, and saying it would have was a
  guess dressed up as a finding.

  **`ArenaStrAppend`** is the in-place growth: if the string ends where the
  arena's next allocation would begin, the new bytes are pushed straight onto
  it. The terminator is what makes it fit — the byte holding it is already the
  string's, so `more.len` new bytes are room for `more.len` characters *and* a
  new terminator, and the next append finds the same invariant. When something
  else has allocated in between it copies both halves, which is exactly what
  concatenating always did, so it is never wrong.

  That alone fixed nested (867.5 -> 729.2 KB) and half of entities, and left
  2176 copies in the entity shape — one per reference. `Grow` cannot append in
  place when something allocated between the node's value and the text being
  appended, and `CharacterReferenceDecode(c->a, ..)` was that something.

  **`CharacterReferenceDecodeInto`** takes a four-byte buffer instead of an
  arena, which is all either half of it ever needed: a named reference's text
  is a NUL-terminated run in the static table and a numeric one is one encoded
  codepoint. The arena version copied that into the arena for a caller about
  to copy it somewhere else. With the decode out of the way every append in
  all four shapes grows in place — **copied is 0 everywhere**.

  Where the four shapes ended up, against the Str-per-node baseline two
  entries ago:

    shape        original    ArenaStr      + append + decode
    prose        1646.1 KB   1285.9 KB     1285.9 KB   -21.9%
    nested       1067.9 KB    867.5 KB      729.2 KB   -31.7%
    gfm tables   2926.0 KB   2269.7 KB     2269.7 KB   -22.4%
    entities      660.2 KB    626.2 KB      163.8 KB   -75.2%

  Entities went 10.28x its source to 2.55x. Speed is unchanged again — prose
  8.44-8.48 ms, nested 9.47-9.63, tables 13.00-13.30, entities 5.82-5.98,
  across three runs, all where they were.

  One measurement lesson worth keeping: a single run had prose at 9.29 ms and
  `tokenize` at 8.86 against 7.65 before. `tokenize` is `Parse` alone and runs
  none of this code, which is what said the machine had drifted rather than
  the change had cost anything. Re-running put everything back. A number that
  moves in a case the change cannot reach is the cheapest drift detector there
  is.

  What is left in prose's 20x is nodes and events, not strings — 3012 Text
  nodes at 168 bytes is 500 KB of the 1286, and the rest is the event list and
  the ArenaVec segments the children live in.

- 2026-08-22: `Node` packs. The fields were in the order mdast.rs declares
  them, which put a `bool` after a 24-byte member twice and left the six of
  them one to a byte at the end: 24 bytes of the struct's 168 were padding and
  slack. Ordered largest first — three 24-byte members, the eight 8-byte
  strings, the one 4-byte number, four single bytes — it is **144 with no
  padding at all**, and a `static_assert` on the arithmetic says so, since a
  field added in the wrong place costs eight bytes a node and would otherwise
  go unnoticed.

  The six bools are one `uint8_t flags` and a `NodeFlag` enum:
  NodeHasPosition, NodeHasStart, NodeOrdered, NodeSpread, NodeChecked,
  NodeHasChecked, read with `Has(..)` and written with `Set(.., bool)`. The
  last two are Rust's `Option<bool>` on a task list item, which is the pair
  that has to stay together.

  At 64 KB of source:

    shape         before                after                arena
    prose         1285.9 KB  20.04x     1150.9 KB  17.93x    -10.5%
    nested         729.2 KB  11.38x      654.0 KB  10.21x    -10.3%
    gfm tables    2269.7 KB  35.41x     2023.6 KB  31.57x    -10.8%
    entities       163.8 KB   2.55x      151.0 KB   2.35x     -7.8%

  Three runs either side, and the times are where they were: prose
  8.46/8.75/8.49 -> 8.48/8.38/8.35 ms, nested 9.58/9.53/9.43 ->
  9.62/9.57/9.92, tables 13.15/12.80/12.92 -> 12.95/13.13/13.85, entities
  5.78/5.86/5.99 -> 6.02/5.89/6.04. `tokenize`, which allocates no Nodes at
  all, wandered 7.58 -> 7.93 -> 7.65 by itself, which is the width of the
  noise band on this machine and the reason none of the above reads as a
  change.

  The saving is 24 bytes per node, so the shapes rank by how many nodes they
  build rather than by how much text they hold: tables saved 246 KB, prose
  135 KB, entities 13 KB.

  Where a markdown parse's memory now goes, at 64 KB of prose: 3012 Text nodes
  at 144 bytes is 434 KB of the 1151, and the rest is the event list and the
  ArenaVec segments the children sit in. Since the three entries above it
  started, prose is 1646 -> 1151 KB and the entity shape is 660 -> 151.

- 2026-08-22: `ArenaPtr<T>`, and a node's children are four bytes each. The
  same trade as `ArenaStr` one level down: an offset into the arena's
  position space instead of an address, in four bytes instead of eight,
  `ArenaOffsetOf`/`ArenaAtOffset` doing the walk and the template wrapper
  keeping a node offset from being read back as an event offset. Zero is
  null, which costs nothing because a block's header sits at its own offset
  zero and no allocation ever lands there.

  `Node::children` is `ArenaVec<ArenaNode>` now. The Node itself is the same
  144 bytes — an ArenaVec handle is two pointers and a length whatever it
  holds — so all of the saving is in the segments the children sit in, which
  is exactly the half of a link that an address spends on information the
  arena already has.

  The walk is `NodeKids(a, n)`, a range whose iterator resolves one offset on
  the dereference and is otherwise the ArenaVec iterator, so the twelve
  `for (const Node* child : n->children)` loops read the way they did. Where
  the arena was not already in hand it was one parameter: `NodeToStringLen`
  now takes it, and the tests hang it off the same `gParsedInto` the
  ArenaStr change introduced.

  At 64 KB of source:

    shape         before                after                arena
    prose         1150.9 KB  17.93x     1091.9 KB  17.01x     -5.1%
    nested         654.0 KB  10.21x      611.6 KB   9.55x     -6.5%
    gfm tables    2023.6 KB  31.57x     1866.9 KB  29.13x     -7.7%
    entities       151.0 KB   2.35x      144.9 KB   2.26x     -4.0%

  Three runs either side, medians: prose 8.64/8.71/8.86 -> 8.66/9.02/8.46 ms,
  nested 9.66/9.76/9.84 -> 9.52/10.46/9.65, tables 13.66/13.46/14.20 ->
  13.25/13.69/13.67, entities 6.03/5.96/5.96 -> 5.94/5.97/5.92. `tokenize`,
  which builds no nodes, sat at 7.81/7.96/8.04 -> 7.75/7.88/7.82 through all
  of it. The extra lookup per child dereference does not show: the block walk
  is one comparison against `current` for a tree that never outgrew its first
  block, and the segments being half the size buys back whatever it costs.

  The shapes rank by children-per-node, not by node count — tables saved
  157 KB, prose 59 KB, entities 6 KB.

- 2026-08-22: An ArenaStr is four bytes, and the length rides with the
  characters. The word was an offset and a length packed into eight; now it
  is the offset alone, and the length is varint-encoded ahead of the bytes it
  measures:

      [varint len][len bytes][NUL]

  Under 128 characters — which is nearly every string a parse stores — that
  is one byte. So a string costs one byte more in the arena and four bytes
  less in every field that holds it, and a Node holds eight of them: **112
  bytes, down from 144**, with the `static_assert` moved along to say so.

  `ArenaStrLen` takes the arena now, since the length is in it. That is one
  byte read and decoded, and every caller that wants the length is about to
  touch the bytes anyway.

  `ArenaStrRef` is gone. A length-prefixed string cannot alias a slice of
  something else, so `Keep` copies always. That reads like a loss and is not:
  what `NodeToString` and `IdentifierFrom` build ahead of it was already a
  permanent allocation in the parse arena, so the ref saved a copy of a
  string that stayed there regardless. The measurement below is the whole
  change including those copies.

  `ArenaStrAppend` still grows in place. Crossing 128 characters is the one
  new case: the prefix needs a second byte, so the append asks for one more
  and shifts the characters over. The string does not move, and the next
  append finds the same invariant.

  At 64 KB of source:

    shape         before                after                arena
    prose         1091.9 KB  17.01x      918.0 KB  14.30x    -15.9%
    nested         611.6 KB   9.55x      512.3 KB   8.00x    -16.2%
    gfm tables    1866.9 KB  29.13x     1543.6 KB  24.09x    -17.3%
    entities       144.9 KB   2.26x      128.5 KB   2.00x    -11.3%

  Three runs either side, medians: prose 8.66/9.02/8.46 -> 8.55/8.74/8.52 ms,
  nested 9.52/10.46/9.65 -> 9.72/9.87/9.63, tables 13.25/13.69/13.67 ->
  13.20/12.90/12.97, entities 5.94/5.97/5.92 -> 5.93/5.88/5.93, with
  `tokenize` at 7.75/7.88/7.82 -> 7.62/7.78/7.77. The varint decode on every
  read does not show, and neither do the copies `Keep` no longer avoids.

  Since the Str-per-node baseline four entries ago: prose 1646 -> 918 KB,
  nested 1068 -> 512, tables 2926 -> 1544, entities 660 -> 128. The entity
  shape is 2.00x its source, where it was 10.28x.

- 2026-08-22: `start` and `depth` are one field. A List's `start` and a
  Heading's `depth` are set by different constructs on different kinds and
  read by different branches; no node is ever both, so they are one
  `uint32_t startOrDepth` and `kind` says which of the two a given node
  means.

  **It does not make the Node smaller, and the arithmetic says why.** Three
  24-byte members and eight 4-byte strings is 104; the number makes 108; the
  three remaining single bytes make 111; and the ArenaVecs align the struct
  to 8, so it is 112 either way. The byte the fusing freed went into the tail
  padding rather than off the end. `sizeof(Node)` is 112 before and after,
  the four shapes' arena figures are identical to the byte — 918.0 / 512.3 /
  1543.6 / 128.5 KB — and the times are where they were: prose
  8.55/8.74/8.52 -> 8.77/9.09/8.63 ms, nested 9.72/9.87/9.63 ->
  9.55/9.91/9.50, tables 13.20/12.90/12.97 -> 13.27/13.41/13.41, entities
  5.93/5.88/5.93 -> 5.90/6.10/5.83, `tokenize` 7.62/7.78/7.77 ->
  7.71/8.09/7.65.

  What it buys is the invariant written down instead of assumed, and one
  single-byte field's worth of room for free: the next flag or small enum
  fits in the padding, where before it would have cost eight bytes a node.

- 2026-08-22: A node's position is two offsets, and the lines are counted
  back out of the source. Rust's unist Position is a line, a column and an
  offset at each end — twenty-four bytes on every node in the tree for
  something nothing on any path here reads. The parse works in offsets; the
  lines and columns were carried along for a diagnostic that never came.

  So a Node keeps `srcStart` and `srcEnd`, eight bytes, and `GetUnistPosition
  (md, srcStart, srcEnd)` counts the rest out of the source when something
  asks. It counts by the tokenizer's own rules, which is the part that has to
  be right: a CR an LF follows is not a character, a tab runs to the next
  stop four columns apart, and every other byte is a column of its own — so a
  multi-byte character is as many columns as it is bytes, which is what the
  tokenizer says and not what a reader would. Two positions it cannot
  recover, neither of which a node ever starts at: part-way through a tab's
  expansion comes back as the tab's own column, and an `md` that is not the
  bytes the parse saw is a fiction.

  **96 bytes, down from 112.** Two 24-byte members, eight 4-byte strings,
  three 4-byte numbers and three single bytes, with one of padding.

  At 64 KB of source:

    shape         before                after                arena
    prose          918.0 KB  14.30x     828.0 KB  12.90x     -9.8%
    nested         512.3 KB   8.00x     462.2 KB   7.21x     -9.8%
    gfm tables    1543.6 KB  24.09x    1379.6 KB  21.53x    -10.6%
    entities       128.5 KB   2.00x     120.0 KB   1.87x     -6.6%

  Three runs either side, medians: prose 8.70/8.51/8.82 -> 9.07/8.68/8.55 ms,
  nested 9.76/9.65/10.14 -> 9.91/9.63/9.80, tables 13.19/14.04/13.20 ->
  12.92/13.21/12.98, entities 6.01/6.09/5.98 -> 5.98/5.85/5.97, with
  `tokenize` at 7.83/7.94/7.67 -> 7.76/7.86/7.69. Building a position is now
  two stores where it was six, and reading one costs a scan nobody makes.

  The one place that read a position back is the GFM task list, which strips
  the checkbox off the front of a paragraph and shifts the text's start past
  it. That was three fields to keep in step — offset, line, column — and is
  one now; the line the checkbox may have left behind falls out of the count
  on its own.

- 2026-08-22: A node's end is a 16-bit length. `srcEnd` was a second offset;
  it is `srcLen` now, saturating at 65535 — a node spanning more than that
  reports the cap, and nothing marks that it was cut. Everything reading a
  span here is a diagnostic, and no construct whose extent is acted on comes
  near 64 KB, so the cap costs the Root of a large document its end and
  nothing else. `NodeSrcEnd(n)` is one past the last byte; `NodeSetSrcEnd(n,
  end)` writes the length; `NodeMoveSrcStart(n, to)` moves the start forward
  and brings the length down by as much, which is the one thing an end offset
  did for free — the GFM task list, stripping the checkbox off the front of a
  paragraph, is the only caller.

  **`sizeof(Node)` is 96 before and after.** Two 24-byte members, eight
  4-byte strings and two 4-byte numbers is 88; the 2-byte length and the
  three single bytes make 93, and `alignof(Node)` is 8 because an ArenaVec
  holds pointers, so it rounds to 96 either way. The two bytes went into the
  tail padding, which is three bytes now where it was one. The arena figures
  are identical to the byte: prose 828.0 KB (12.90x), nested 462.2 KB
  (7.21x), gfm tables 1379.6 KB (21.53x), entities 120.0 KB (1.87x).

  Three runs either side, medians: prose 8.66/8.99/8.61 -> 8.81/8.77/9.08 ms,
  nested 9.50/9.88/9.76 -> 9.67/9.73/9.66, tables 12.99/13.19/12.85 ->
  13.00/13.05/13.52, entities 5.87/6.05/6.02 -> 6.04/6.05/6.14, with
  `tokenize` at 7.73/7.99/7.68 -> 8.00/7.94/7.84.

  What it buys, then, is not bytes today: it is a 2-byte field and three
  bytes of padding where there was one 4-byte field and one, so the next
  small field on a Node is free — and the field after that. What it costs is
  the exact end of anything longer than 64 KB.

- 2026-08-22: A node's children are a ring, not a vector. `ArenaVec<ArenaNode>
  children` was 24 bytes in every node plus an arena segment in every node
  that had any. It is two offsets now: `lastKid` in the parent names the last
  child, each child's `sibling` names the next, and the last child's wraps
  back to the first.

  The ring is what makes it two offsets and not three. Appending is all the
  parse ever does to a child list, and appending to a ring is three stores
  with no walk; `NodeLastChild` — "the child still being built", which the
  compiler asks for constantly — is the offset the parent already holds. A
  plain first/next list would have wanted the first and the last in the
  parent, 12 bytes and an 88-byte Node.

  Not the single tagged offset that was asked for. A node with children and a
  next sibling — a paragraph among paragraphs, an item among items, most of
  any tree — needs two distinct references, and one field holds one; the tag
  chooses which of the two it means rather than letting it mean both.
  Threading the last child's link back up does not recover it either, because
  nothing then says how many levels to pop.

  **80 bytes, down from 96.** One 24-byte member, eight 4-byte strings, four
  4-byte offsets, the 2-byte length and three single bytes, with three of
  padding.

  At 64 KB of source:

    shape         before                after                arena
    prose          828.0 KB  12.90x     619.0 KB   9.64x    -25.2%
    nested         462.2 KB   7.21x     308.8 KB   4.82x    -33.2%
    gfm tables    1379.6 KB  21.53x     898.7 KB  14.02x    -34.9%
    entities       120.0 KB   1.87x      98.9 KB   1.54x    -17.6%

  Three runs either side, medians: prose 8.66/8.99/8.61 -> 8.43/8.60/8.59 ms,
  nested 9.50/9.88/9.76 -> 9.49/9.44/9.52, tables 12.99/13.19/12.85 ->
  13.19/12.74/13.12, entities 5.87/6.05/6.02 -> 5.85/5.86/5.98, with
  `tokenize` at 7.73/7.99/7.68 -> 7.96/7.95/7.94. The `to_mdast` phase on its
  own, which is where the segments were being allocated, went 0.389/0.403/
  0.445 -> 0.342/0.311/0.343.

  What moved at the call sites: four `NodeChild(a, n, len - 1)` became
  `NodeLastChild`, two `NodeChild(a, n, 0)` became `NodeFirstChild`, the link
  fragment's steal is one offset crossing over, and dropping the task list's
  emptied text node is one link rewrite where it was a vector shifted down.
  `NodeChildCount` is a walk and exists for the tests; nothing in the parse
  or in TextView counts children.

  The cost to know about: `NodeChild(a, n, i)` walks where it used to index.
  Every caller asks for the first or the last, so nothing here regressed, but
  indexing in a loop would now be quadratic.

  Since the Str-per-node baseline: prose 1646 -> 619 KB, nested 1068 -> 309,
  tables 2926 -> 899, entities 660 -> 99, and a Node, measured then
  and now, 144 -> 80 bytes.

- 2026-08-22: A Table's alignments are a count and two bits a column.
  `ArenaVec<AlignKind> align` was the last 24-byte member of a Node, carried
  by every node in the tree for something only Tables have, with an arena
  segment and a byte a column behind it. It is a four-byte offset now, at a
  block of

      [varint count][2 bits a column, four to a byte, lowest bits first]

  which for the eight-column table that is already wide is three bytes. The
  block is pushed byte-aligned rather than through `Alloc`, which rounds to
  eight and would have handed back exactly what the varint saved. The varint
  helpers moved out of ArenaStr's part of base.cpp into base.h to do it —
  same encoding, now with a name.

  The whole list is known when the table is entered, so `GfmTableAlign` reads
  the delimiter row twice: once to count the columns, once to fill the one
  allocation. Nothing allocates in between, and a vector's growth was never
  needed.

  **60 bytes, down from 80** — more than the 20 the field gave up, because
  with the last ArenaVec gone nothing in a Node is wider than four bytes and
  `alignof(Node)` falls from 8 to 4. Any pointer-holding member put back
  costs that again.

  At 64 KB of source:

    shape         before                after                arena
    prose          619.0 KB   9.64x     519.3 KB   8.09x    -16.1%
    nested         308.8 KB   4.82x     256.3 KB   4.00x    -17.0%
    gfm tables     898.7 KB  14.02x     710.3 KB  11.08x    -21.0%
    entities        98.9 KB   1.54x      89.2 KB   1.39x     -9.8%

  The varint's own share of that is 0.8 KB on the tables shape and nothing on
  the other three, which have no tables; the rest is the smaller node and the
  segment that is no longer allocated.

  Three runs either side, medians: prose 8.43/8.60/8.59 -> 8.94/8.65/8.41 ms,
  nested 9.49/9.44/9.52 -> 9.64/9.58/9.46, tables 13.19/12.74/13.12 ->
  12.70/12.52/12.64, entities 5.85/5.86/5.98 -> 5.87/5.80/5.85, with
  `tokenize` at 7.96/7.95/7.94 -> 7.62/7.62/7.74 and `to_mdast` at
  0.342/0.311/0.343 -> 0.319/0.305/0.310. Reading an alignment is a block
  lookup and a varint decode where it was an indexed load, once a cell at
  render time, and it does not show.

  Since the Str-per-node baseline: prose 1646 -> 519 KB, nested 1068 -> 256,
  tables 2926 -> 710, entities 660 -> 89, and a Node 144 -> 60 bytes.

- 2026-08-22: A List's start, a Heading's depth and a Table's alignments are
  one word. `startOrDepth` already held two of the three; the alignment
  offset is the third, and a node is one of those kinds or none of them, so
  no two are ever live together. The field is `perKind` now, and the rule it
  is under is written where it lives: `kind` says which of the three a given
  node means, the compiler cannot check it, and every reader looks at `kind`
  first.

  **56 bytes, down from 60.** Eight 4-byte strings, four 4-byte offsets, the
  2-byte length and three single bytes, with three of padding.

  At 64 KB of source:

    shape         before                after                arena
    prose          519.3 KB   8.09x     483.9 KB   7.54x     -6.8%
    nested         256.3 KB   4.00x     233.7 KB   3.65x     -8.8%
    gfm tables     710.3 KB  11.08x     646.1 KB  10.08x     -9.0%
    entities        89.2 KB   1.39x      86.1 KB   1.34x     -3.5%

  Fastest of three runs either side: prose 8.39 -> 8.37 ms, nested 9.41 ->
  9.52, tables 12.47 -> 12.66, entities 5.78 -> 5.92, `tokenize` 7.54 ->
  7.65, `to_mdast` 0.303 -> 0.307. The control moved as much as the shapes
  did, which is the machine and not the change: the same word is read the
  same number of times, under one name.

  Since the Str-per-node baseline: prose 1646 -> 484 KB, nested 1068 -> 234,
  tables 2926 -> 646, entities 660 -> 86, and a Node 144 -> 56 bytes.

- 2026-08-22: A node's strings are a list in the arena, not eight fields.
  Eight offsets was 32 of a 56-byte Node, and seven of them were unset on
  nearly every node in a tree: a Text node carries a value and nothing else,
  and most nodes carry nothing at all. One offset now, at a list of records:

      [u32 next][u8 kind][varint len][len bytes][NUL]

  `next` goes through memcpy — the records are byte-aligned, because rounding
  each to four would give back what the varint and the packing saved, and a
  misaligned load is the compiler's business to know about rather than ours
  to risk. New records go on the head, so storing is O(1); the walk that
  finds a kind is at most eight long and is nearly always one or none.

  `NodeGetStr(a, n, kind)` reads, `NodeSetStr` stores, `NodeClearStr` takes
  away, `NodeGrowStr` appends. Growing still happens in place while the
  record is the newest thing in the arena, which is what a text node's value
  arriving one Data event at a time depends on; it rebuilds at the end
  otherwise, as ArenaStrAppend did.

  **28 bytes, down from 56.** A stored string costs five bytes more than
  before — the `next` and the `kind` — and a node that stores none saves 28.
  Most store none, and none ever stored more than three.

  At 64 KB of source:

    shape         before                after                arena
    prose          483.9 KB   7.54x     358.3 KB   5.58x    -26.0%
    nested         233.7 KB   3.65x     159.1 KB   2.48x    -31.9%
    gfm tables     646.1 KB  10.08x     402.9 KB   6.29x    -37.6%
    entities        86.1 KB   1.34x      73.7 KB   1.15x    -14.4%

  Fastest of three runs either side: prose 8.37 -> 8.25 ms, nested 9.52 ->
  9.28, tables 12.66 -> 12.58, entities 5.92 -> 5.79, `tokenize` 7.65 ->
  7.62, `to_mdast` 0.307 -> 0.296. The list walk costs less than the smaller
  nodes save.

  One bug worth writing down, because the refactor looked mechanical and was
  not. `TailMut(c)->url = Keep(c, NodeToString(c->a, Resume(c)))` became
  `Keep(c, TailMut(c), Url, NodeToString(c->a, Resume(c)))` — and an
  assignment sequences its right side first where the order two arguments are
  evaluated in is the compiler's to choose. `Resume` pops the stack `TailMut`
  reads. MSVC and Linux clang both went right to left and passed; Apple clang
  on arm64 went left to right and the tests took a SIGSEGV with no output.
  Seven sites, each now a statement of its own with a comment saying why.
  Turning a field assignment into a setter call is where this lives.

  Since the Str-per-node baseline: prose 1646 -> 358 KB, nested 1068 -> 159,
  tables 2926 -> 403, entities 660 -> 74, and a Node 144 -> 28 bytes.

- 2026-08-22: The reference kind rides in `flags`, and the length moves next
  to the bytes. Three values need two bits and the flags used six of the
  eight, so `referenceKind` gives up its byte — and a byte here was worth
  four, because 25 bytes round to 28 where 24 round to themselves.
  `NodeRefKind` and `NodeSetRefKind` reach it; `kind` stays a plain field, so
  the switches on it are untouched.

  That alone left 26, which is still 28: `srcLen` sat between two four-byte
  fields and cost two bytes of padding. Beside `kind` and `flags` it is free.

  **24 bytes, down from 28** — and worth eight a node rather than four,
  because `NodeNew` pushes through `Alloc`, which aligns to eight, so a
  28-byte node was handed 32. There is no padding left to hide a field in:
  the next one costs four bytes a node, and there are no spare flag bits
  either.

  At 64 KB of source:

    shape         before                after                arena
    prose          358.3 KB   5.58x     321.2 KB   5.00x    -10.4%
    nested         159.1 KB   2.48x     136.5 KB   2.13x    -14.2%
    gfm tables     402.9 KB   6.29x     341.9 KB   5.33x    -15.1%
    entities        73.7 KB   1.15x      70.4 KB   1.10x     -4.5%

  Fastest of three runs either side: prose 8.25 -> 8.14 ms, nested 9.28 ->
  9.35, tables 12.58 -> 12.51, entities 5.79 -> 5.83, `tokenize` 7.62 ->
  7.64, `to_mdast` 0.296 -> 0.301.

  Those two eight-byte steps pin the node count: prose is about 4,600 nodes,
  so nodes are roughly 110 KB of its 321. The next boundary is 16 bytes and
  only one combination reaches it — `srcStart`, `srcLen` and `perKind` all
  going the way the strings went, into records only the nodes that need them
  pay for. Partial moves buy nothing: without all three the struct still
  rounds back up to 24.

- 2026-08-22: A Node is 16 bytes, and where it came from is not kept. Two
  fields left the struct and one field's worth of alignment left with them.

  `srcStart` and `srcLen` are gone. Rust keeps a line, a column and an offset
  at each end — 24 bytes on every node in a tree — and this port had it down
  to the two offsets, which was 8; nothing in the parse read either, and
  nothing outside it did. `GetUnistPosition(md, start, end)` stays, because
  it is a function of a source and two numbers, but no node hands it one any
  more: a caller wanting a position needs an offset of its own, an event's
  `point.index` being the one the parse works in. That is the one real thing
  this change spends, and it is written down here rather than buried in the
  diff.

  `perKind` — a List's start, a Heading's depth, a Table's alignments —
  became a record in the list the strings already live in, varint-encoded,
  read by `NodePerKind` and written by `NodeSetPerKind`. Those three kinds
  pay about eight bytes for it; every other node pays nothing, where the
  field cost four on all of them, and most nodes in a tree are a Text or a
  Paragraph that has no such word.

  `NodeNew` pushes at `alignof(Node)` rather than the eight `Alloc` uses,
  which is the other half of what 16 means: the struct needs 4, and the
  string records around it are byte-aligned, so the padding the arena used
  to insert ahead of each node mostly goes as well.

  **16 bytes, down from 24.** At 64 KB of source:

    shape         before                after                arena
    prose          321.2 KB   5.00x     272.0 KB   4.24x    -15.3%
    nested         136.5 KB   2.13x     110.2 KB   1.72x    -19.3%
    gfm tables     341.9 KB   5.33x     250.5 KB   3.91x    -26.7%
    entities        70.4 KB   1.10x      65.6 KB   1.02x     -6.8%

  Prose saved 49.2 KB where eight bytes times its roughly 4,600 nodes is 37;
  the other twelve is the alignment padding that is no longer there.

  Fastest of three runs either side: prose 8.38 -> 8.22 ms, nested 9.46 ->
  9.26, tables 12.76 -> 12.92, entities 5.90 -> 5.85, `tokenize` 7.63 ->
  7.57, `to_mdast` 0.300 -> 0.302. All of that is inside the spread — the
  tables column moved 12.76 to 13.07 across the three baseline runs alone.
  The record lookups cost nothing measurable and the smaller node buys
  nothing measurable; this one is memory and not time.

  Since the Str-per-node baseline (`b935eaa`): prose 1646 -> 272 KB, nested
  1068 -> 110, tables 2926 -> 250, entities 660 -> 66, and a Node 232 -> 16
  bytes. What is left in the struct is two child links, the record list and
  two bytes of tag — there is no field left to take that anything still
  reads.

- 2026-08-23: `Str(const char*)` is explicit, the way SumatraPDF's is, so a
  `const char*` cannot become a Str by accident: a literal goes through
  `StrL()`, which knows its length at compile time, and a runtime pointer
  says `Str(p)` where it means "walk this to the NUL".

  It caught nothing that should have been `StrL()`. Nine sites stopped
  compiling, all of them in the formatter and all of them genuine runtime
  buffers; a grep for `Str("..")`, which the compiler cannot flag, came back
  empty on all three platforms. The discipline was already kept — the point
  is that it is now kept by the compiler.

  `bufFmt` answers the Str it wrote rather than nothing, so the nine sites
  append the result directly instead of walking the buffer again with
  strlen. The length comes from vsnprintf's return, falling back to strlen
  when it truncated, because MSVC's `_vsnprintf_l` answers -1 there where
  C99 answers the length it wanted.

  `fmt()` had no tests. `tests/FmtTests.cpp` is 58 checks against the doc
  block: the directives, flags and width and precision reaching snprintf
  unchanged, the length modifier normalized so `%lld`, `%I64d`, `%jd` and
  `%zu` agree on every platform, `%s` padding done by hand because a Str
  need not be terminated, and a format that does not hold up answering an
  empty Str rather than a partial one.

  The doc block itself was SumatraPDF's, and described an API this tree does
  not have — `Fmt fmt("%d = %s"); fmt.i(5).s("5").Get()`. It now describes
  `fmt()` / `logf()` / `StrDup(a, fmt(..))`, and corrects the spelling of a
  positional: it is `%{0}`, not `%{$0}`, which the parser rejects. Every
  claim in it is now an assertion in the suite that passed.

- 2026-08-23: The executor. GPUI runs everything that is not this frame
  through two of them — a `ForegroundExecutor` on the main thread that may
  touch entities, and a `BackgroundExecutor` thread pool that may not — and
  hands back a `Task<T>` whose destructor is the cancel. There are no futures
  here, so `src/sys/executor.h` is that pair written as callbacks: `ExecPost`
  queues a `Func0` to run on the main thread and wakes the event loop,
  `ExecDrain` runs what is queued, and `ExecSpawn(work, done)` puts `work` on
  a pool thread and posts `done` back when it returns. The handle is an
  integer, cancelled with `ExecCancel`, the way `WindowSetInterval` hands one
  out — nothing in this tree is cancelled by leaving a scope. Rust's two rules
  are kept as they are: a worker never touches an entity, a window or the
  frame arena, and everything the UI owns is touched on the main thread.

  The main-thread queue is modelled on SumatraPDF's `uitask::Post` — a lock, a
  list, and a nudge — and the nudge is the part each platform owns, installed
  with `ExecSetWake` so nothing portable names an OS call. Windows gets a
  message-only window of its own rather than `PostThreadMessage`, because a
  thread message is dropped by every modal loop the OS runs for us
  (`TrackPopupMenu`, the resize loop) and a posted window message is not. X11
  gets a self-pipe, polled beside the display's fd. macOS gets
  `dispatch_async` on the main queue, which is the one thing there documented
  to be safe from any thread; the block drains the queue itself and then posts
  an `NSEventTypeApplicationDefined`, because servicing the dispatch queue
  produces no event and `nextEventMatchingMask` would otherwise sleep on to
  its deadline with the work already sitting there.

  `WindowPost(win, listener, ev)` is the entity-bound half — `cx.spawn` plus
  `this.update(cx, ..).ok()` — and drops the call if the window closed or the
  entity went away before the queue was drained, which is the lifetime a
  `Task` in a view field gets in Rust.

  Timers stay where they were. GPUI has no timer list because it spawns a task
  per timer that sleeps; `WindowSetInterval` / `WindowSetTimeout` already are
  that, cancelled with the entity, and folding them into an app-level executor
  would have made them outlive the window they belong to.

  The pool is elastic: no threads until there is work, one more whenever a job
  arrives with nobody idle, up to `kExecMaxWorkers` (8), and they live until
  `ExecShutdown`. Eight rather than the core count because a job here may
  block on the network for fifteen seconds, and the fetcher's does.

  `sys/http.cpp` was the only thread user in the tree and is now the pool's
  first caller: `ThreadRunDetached` moved into base as `PlatThreadRun` (with
  `PlatThreadId`, `PlatSleepMs` and a `CondVar` beside `Mutex`), and
  `src/sys/http_posix.cpp`, which was nothing but those two calls, is gone. A
  fetch now reports itself instead of being polled: `HttpSetOnFetchDone` is a
  completion the window installs, so `WindowTimerMs` no longer wakes an idle
  window at 20 Hz for as long as a picture is on its way. Verified end to end
  on Windows — a failing fetch in `rich_text` came back through the pool, the
  message-only window and the main-thread queue.

  `base.h`'s `Func0` and `Func1<T>` grew the two things Sumatra's have and
  ours did not: a no-argument function (`MkFunc0Void`, with the `kFuncNoArg`
  sentinel) and the low-bit flag that lets a `Func0` stand in for a `Func1`,
  which is what makes one queue take both. `tests/ExecutorTests.cpp` is 40-odd
  checks — a post runs only when drained and in the order made, a post made
  during a drain waits for the next pass, a spawned job really is on another
  thread, every job of 32 runs, and a job that has not started is cancellable,
  which is made deterministic by filling all eight workers with jobs that hold
  until the test lets go.

- 2026-08-23: How `Vec` and `ArenaVec` grow. Both had a policy nobody had
  measured — `max(cap * 2, wanted)` from zero, which hands back **1**, and
  three segment sizes in elements, 4/16/64. `cmd/vec-log.ts` is the
  measurement: a debug-only instrument in `src/base.h` / `src/base.cpp`
  (`#if defined(DEBUG)`, and silent unless `GPUI_VEC_LOG` names a file) gives
  every vec a serial number and the source location it was declared at, and
  logs its births, growths and death. The analysis half replays a run against
  other policies without rebuilding, which works because the log records what
  each caller *asked for* (`needed`, `want`) and not only what the policy
  handed out.

  The location comes from `__builtin_FILE()` / `__builtin_LINE()` /
  `__builtin_FUNCTION()` as **default arguments**. As default member
  initializers MSVC evaluates them where they are written, so every vec in the
  tree would have said `base.h`; as default arguments all three compilers
  evaluate them at the call site. A member vec lands on the closing brace of
  the struct that holds it — the implicit constructor is what runs them — so
  the report keys on file:line plus the owning type plus the element size.
  The constructors take those parameters through `GPUI_VEC_DBG_ARGS` /
  `GPUI_VEC_DBG_INIT` macros rather than an `#if` around each signature:
  clang-format cannot count braces through a signature split across `#if` /
  `#else` with one body under it, and de-indents the rest of the struct.

  What the logs said, over the test suite, both benchmark suites and a
  showcase frame: six vecs in ten never allocate at all, and of the ones that
  do, the great majority stop at one to three elements. So what the *first*
  allocation costs is the only thing being traded, and starting it at 1 was
  paying three reallocs and three memcpys to reach four elements.

  `Vec` now starts at Rust `RawVec`'s byte-aware floor — 8 elements for a
  1-byte element, 4 up to 1 KB, 1 above (`VecNextCap`) — and doubles as
  before. A floor of 8, and a growth factor of 1.5, were both worse on one
  axis or the other in every run.

  `ArenaVec` keeps 4/16/64 elements but caps them at 64/256/1024 **bytes**,
  which binds only on the wide elements the counts were over-serving. Sizing
  the whole progression in bytes reached the same total arena but by making
  the narrow vecs bigger as well as the wide ones smaller, and that grew the
  mdast *output* arena on the prose benchmark by 23% — `to_mdast`'s two
  per-tree stacks live in it. Caps only, therefore: it can spend less than the
  counts did and never more.

  Four sites reserve instead of growing, three of them because the count was
  already in hand: `GenerateAnonymousFlexItems` (the child count — the hottest
  vec in the tree, 72,007 of them per flexbox benchmark, 192 bytes an
  element), `Parse`'s top-level tokenizer events (about one event per two
  bytes of source, and it is the one tokenizer whose span is the whole
  document), `EditMapConsume`'s jumps (one per entry), and the two icon
  builders, `DrawOpsBuilder::ViewBox` and `ParseSvg`, off the measured
  percentiles.

  Measured, not replayed: flexbox growth events 280,715 → 179,166 and bytes
  memcpy'd 17.9 MB → 0.79 MB; markdown 268,498 → 170,423 events, 43.9 MB →
  37.7 MB copied, and 21.2 MB → 17.1 MB of arena. In release, `grid/deep`
  16384 leaves 120.8 → 104.3 ms, `grid/superdeep` 1000 levels −14%, tree
  creation −5 to −15%, `flexbox/wide` −7%, markdown parse −4 to −12%; nothing
  regressed outside run-to-run noise, the mdast output arena is byte-identical
  and a `showcase` shot is pixel-identical to the build before.

  Two `ArenaVecTests` cases wrote the first segment size out as 4; they ask
  `ArenaVec<int>::CapFor` for it now, since it is a count capped by a byte
  budget. What is left on the table is `ArenaVecSegment`'s 24-byte header,
  which is 27% overhead on a two-element segment of 32-byte events —
  shrinking it would beat any further capacity tuning.

- 2026-08-23: An inline buffer for taffy's flex line list, and borrowed
  storage in `Vec` to hold it. The previous entry left this on the table: a
  reserve cannot remove an allocation, only move it, and `ComputePreliminary`
  was still taking one 24-byte block and freeing it per container per layout
  pass. `VecUseInline(v, buf)` lends a vec an array on the caller's stack to
  start in, so a vec that never outgrows it costs nothing at all.

  The capacity rides in the sign of `cap` — negative means the elements are in
  storage the vec does not own, and the size is `-cap`. A field of its own
  would have been clearer and cost eight bytes on every `Vec` in the tree,
  which is a great many; `StrBuilder` pays that eight for the same feature,
  and it is the only other thing here with it. Only two places have to tell
  owned from borrowed: growing, where the block cannot be realloc'd so the
  live prefix is copied out into a fresh one and the array is left alone, and
  freeing, which must not. `Cap()` is what everything else reads. The one
  thing it is not safe with is the idiom that takes a vec's storage over by
  hand — `events.els = t->events.els; events.cap = t->events.cap;` — and none
  of the ten places that do that are handed a borrowed vec.

  Two lines of buffer, which is a number that was measured rather than
  picked. No container anywhere in this tree — the taffy suite, the story
  gallery, the showcase, the benchmarks — ever takes a second flex line, so
  even one would do; but the buffer sits on a frame that recurses once per
  node and a grid item is laid out through here too, so the frame's size is
  not free. At four lines `grid/wide 100x100` was a repeatable 2% slower over
  three runs each way. At two it is level with the build before it, the deep
  flexbox trees are 3-5% faster (`deep tree (auto size)` 8.92 -> 8.48 ms,
  `(random size)` 3.80 -> 3.61 ms), and two is the largest that is never
  *worse* than allocating: an empty vec takes a first block of four, so a
  three- or four-line container still allocates exactly once.

  `tests/VecTests.cpp` is new and is where the mechanism is pinned: the vec
  stays in the buffer, the append and the reserve past it carry the elements
  out, `Reset` and the destructor let go without freeing a stack address,
  a copy owns its own elements, and `Clear` zeroes the right number of bytes.
  The last one drives the real caller — a wrapping row twelve items wide,
  which makes six lines — because nothing else in the tree would ever push
  that list off the stack. The suite passes under ASan, which is the check
  that matters for this one.

  Grid and markdown numbers moved in both directions across these runs
  without either being touched; the noise floor on this machine is around 2%
  for the small benchmarks and more for `grid/superdeep`, which read +13.8%
  once and was level on three focused repeats. Anything at that scale needs
  repeating before it means something.

- 2026-08-23: **The browser is a fourth target.** `bun cmd/build.ts -wasm <example>` compiles the same amalgamated `gpui.cpp` with emscripten and `bun cmd/run-wasm.ts <example>` serves and opens it. WASM had been a standing non-goal in `AGENTS.md`; that line is gone, because the seams rule 7 already asks for turned out to be the whole job — the four new platform files are `src/base_wasm.cpp`, `src/gpui/paint_wasm.cpp` (Canvas2D), `src/gpui/window_wasm.cpp` (the canvas, the DOM events, an animation-frame loop) and `src/sys/sysinfo_wasm.cpp` / `http_wasm.cpp`, and *not one `#if` was added to a shared file*. Every existing `#if GPUI_OS_MAC` in `src/base/input_keys.cpp`, `src/ui/kbd.cpp` and `src/ui/title_bar.h` reads correctly for a target where `GPUI_OS_MAC` is 0.

  Four seams had to be widened, each a portable function rather than a guard:

  - `PlatArenaReserveSize()`. An arena reserved 64 MB of address space because on a hosted platform untouched address space is free. wasm has no reserve/commit split at all — emscripten's `mmap` hands out real pages for `PROT_NONE` and warns `unsupported syscall` on every `mprotect` — so six arenas would have cost the tab 400 MB before the first frame. wasm answers 4 MB and chains a second block when it has to; `base_win.cpp` and `base_mem_posix.cpp` still say 64.
  - `base_posix.cpp` split. Emscripten's libc answers for strings, directories, threads and the clock the way Linux does, so wasm compiles that file too; the mmap half moved to `base_mem_posix.cpp`, which only the two hosted targets take. `cmd/build-dist.ts` learned the suffix — and to test it *before* `_posix.cpp`, which it also ends with.
  - `ExecHasThreads()`. `pthread_create` links under emscripten and returns `EAGAIN`, so `PlatThreadRun` fails honestly. `ExecSpawn` used to drop the job on the floor when no thread could be started; now it queues it on the main thread's own queue, so the work still happens and `done` still lands where it always does. That fixes a latent bug on every platform — a host that refused a thread lost the job silently — and `ExecHasThreads()` is what `tests/ExecutorTests.cpp` asks before insisting the work ran somewhere else.
  - `TableCellPack` is `intptr_t`, so a 32-bit target has twenty bits over the column: half a million rows, not four billion. Written down in `data_table.h`, and `tests/DataTableTests.cpp` and `tests/ArenaStrTests.cpp` stopped assuming a 64-bit word.

  The full suite runs under node: **16635 checks, all passing**, which is taffy, markdown and the whole base layer verified on a second word size for the first time. All seventeen examples build; `hello_world`, `showcase`, `system_monitor`, `story`, `input`, `app_assets` and `rich_text` were driven in headless Chrome over CDP — clicking, typing, scrolling, opening the drawn fallback menu — and match the Windows build. One real bug came out of that: `TextLayoutNew` was reporting a run's width with its trailing space trimmed, which ran every inline markdown run into the next one ("UIcomponentsforbuilding"); a line only loses its trailing space when a *wrap* put it there, not when the text ends with one.

  What a tab cannot do, and is documented rather than faked: one window (one canvas); `AppRun` never returns, because `emscripten_set_main_loop` unwinds the stack; `HttpGet` answers false, since everything a page can fetch with is asynchronous and `HttpGet` blocks — a remote image shows its alt text exactly as a Linux build without libcurl does; `ImageDecode` answers before the browser has decoded, so the `Image` is sized zero for a frame and the load repaints (SVG never goes through it, which is most of what this tree draws); the clipboard is a mirror kept by the DOM `paste` event, with the paste chord driven by that event rather than by its keydown; and `sysinfo` reports the tab — heap, `hardwareConcurrency`, the Battery API — not the machine.

  Trap for the next session: an `EM_JS` body is stringified by the preprocessor, so it may hold no empty `''` (an empty char constant), no regex literal and no backslash outside a string literal. Write `""`, and build a `RegExp` out of a string if you ever need one.

- 2026-08-23: **A second Windows backend, the shape GPUI's own renderer is.** `paint_win.cpp` was already on the GPU — Direct2D on a D3D11 device over a flip-model swap chain — but not in GPUI's *shape*: D2D takes a call per primitive and batches by its own rules. `src/gpui/paintgpu_win.cpp` is what Blade and `directx_renderer.rs` do instead. A frame is one instance buffer, 96 bytes a primitive, and the pixel shader evaluates the rounding, the border and the content mask analytically; path fills are stencil-and-cover, which is how you fill an arbitrary outline on a GPU with no tessellator — draw each contour's fan into the stencil with INVERT (even-odd) or wrapping increment and decrement (nonzero), then cover the bounding box through a stencil test. `GPUI_PAINT=gpu` picks it, read once at startup, so the default build is untouched: a story screenshot is byte-identical to the one before this change.

  It shares everything device-independent with the D2D file rather than writing it twice. A `TextLayout*` on Windows *is* an `IDWriteTextLayout`, so shaping, measurement, hit-testing and range rects are literally the same code on both paths and only `TextLayoutDraw` differs — it runs the layout through an `IDWriteTextRenderer` that turns each glyph run into atlas quads. The WIC decode, the DirectWrite factory and the D3D11 device are shared the same way. `paint_win.cpp` hands over in one line per entry point, twenty-five of them.

  Measured with `GPUI_FRAME_BENCH`, which is new and times the three phases of `Window::draw` apart — building the tree, laying it out, painting it — because the paint phase is a third of a `story` frame and a whole-frame number would have hidden the result. Mean of 800 frames, release, paint phase only:

  | scene | D2D | GPU | |
  | --- | --- | --- | --- |
  | showcase | 0.21 ms | 0.09 ms | 2.3x |
  | system_monitor | 0.46 ms | 0.16 ms | 2.9x |
  | story | 1.86 ms | 0.99 ms | 1.9x |

  The charts win biggest, because D2D re-tessellates path geometry on the CPU every frame and stencil-and-cover does not. The honest framing is the whole frame, though: `story` spends 5.8 ms in layout, so halving the paint is 11% of it. Multisampling — `GPUI_PAINT_MSAA`, 4 by default, and where paths and strokes get their antialiasing since only quads and glyphs carry their own — is close to free on the two heavy scenes and about 0.05 ms on the light one.

  Two things measuring it turned up. The showcase was *slower* than D2D (0.41 ms against 0.20) while batching the entire scene into a **single draw call** — so the cost was never the drawing. It was the per-frame full-surface D24S8 stencil clear; the cover pass already leaves the buffer at zero, so the clear only has to happen once when the surface is made, and that took it to 0.09 ms. And pixel-diffing against D2D found a real bug: underlines and strikethroughs came out half-brightness, because a 0.8 px rule at true analytic coverage is fainter than DirectWrite's, which snaps thin rules to the pixel grid. Snapping them fixed it.

  What is left, and why this is not the default: no subpixel glyph positioning — the atlas holds one rasterization per glyph and x is snapped, where DirectWrite positions at a third of a pixel — dashes are expanded on the CPU for lines and ignored on rounded rects, and the shaders are compiled with `D3DCompile` at startup rather than built to bytecode by fxc. Against D2D, 2.7% of the story's pixels differ and 1.2% of the chart page's, all of it text antialiasing and that positioning; no geometry moves. `AGENTS.md`'s exclusion of "the full GPUI GPU scene graph" is narrowed rather than dropped — this is the renderer, not a scene graph: no layers, no batching across windows, no offscreen mask cache.

- 2026-08-23: the story's layout pass, 5.8 ms when the GPU renderer landed and
  4.3 ms on this machine at that commit, is 2.3 ms. Profiled with `winperf`
  (`record -i 2000 -write-agent`, 1500 frames under `GPUI_FRAME_BENCH`), which
  is the first time the frame has been looked at with a sampling profiler
  rather than the three phase timers.

  **The first thing it found was not layout at all: it was the filesystem.**
  `FLTMGR.SYS` and `Ntfs.sys` held 18% of every sample, all of it under
  `LayoutMeasure` → `ImageNaturalSize`. `AssetsExists` answered "is this file
  there" by *reading the whole file* through `AssetsLoad` into a throwaway
  buffer, and `ImageAssetFor` asked it up to three times (the bare name, then
  `story/` and `images/`) across up to twelve roots. `ImageVectorForSrc` calls
  `ImageAssetFor` before it can even reach the decode cache, so a page with a
  vector picture on it walked and opened those paths on every measure pass of
  every frame. Two fixes: `AssetsExists` is `PlatFileExists` — a new base
  entry point, `GetFileAttributesA` on Windows and `stat` elsewhere, next to
  `PlatDirExists` — and `ImageAssetFor` keeps what each src resolved to, the
  empty answer included, since the roots do not change while the app is up.
  That alone took layout from 4.30 ms to 2.57 ms, and the paint phase from
  1.91 to 1.41, because paint resolves the same srcs.

  What was left was taffy, and three things came out of it. `F32Min` / `F32Max`
  tested for NaN with `std::isnan`, which MSVC compiles to a **call** into the
  CRT's `_fdclass` — 0.9% of all samples in a function that is one bit test;
  `F32IsNan` is that bit test. `CompactLength::Value` and `FromVal` were out of
  line in `style.cpp` and are one bit-cast each. And
  `ComputeBlockChildLayout`, the hottest function in a layout, resolved the
  node three times and built the cache key twice — once in `CacheGet` and
  again in `CacheStore`; it now looks up `NodeData` and `CacheKey` once and
  calls `Cache::GetWithKey` / `StoreWithKey`, with the `LayoutInput` overloads
  kept for the public tree API. Together: layout 2.57 → 2.31 ms, and on
  taffy's own benchmarks, where no asset ever loads, 6-14% off every flexbox
  case (`deep tree (random size)` 14-level 9.83 → 8.44 ms).

  Three things were measured and thrown away, which is worth writing down so
  the next session does not try them again. **`/GL` + `/LTCG`** is level or
  fractionally worse and costs build time. An **inline buffer for
  `ComputePreliminary`'s `Vec<FlexItem>`**, the trick that paid for `FlexLine`,
  moves nothing at four items or at twelve — the allocation was never the cost
  there. And **moving the `resolve.rs` helpers into `style.h`** so they inline
  across translation units bought under 1%, which is not worth 120 lines in a
  header; `RectLpa::ResolveOrZero` stays out of line at 2% self.

  The frame is now build 0.20 ms, layout 2.31 ms, paint 1.40 ms. What the
  profile says is next, and none of it is one change: `ComputeLeafLayout`
  (5.4% self), `DetermineFlexBaseSize` (2.9%), `GenerateAnonymousFlexItems`
  (2.4%). The tree is rebuilt from nothing every frame, which is what GPUI
  does too, so the only structural win left is carrying layout across frames.


## A GPUI scene graph, prototyped behind `GPUI_SCENE`, and what it turned out
## to be worth

`src/gpui/scene.h` / `scene.cpp` is a scene between the element tree and
`paint.h`: a frame's drawing collected as a flat array of primitives, each one
carrying its own content mask and its layer, instead of being issued to a
backend as the tree walks. It is off unless `GPUI_SCENE` is set, and the levels
stack — `replay` collects and draws, `cache` keeps path geometry across frames,
`skip` does not draw a frame identical to the last, `damage` draws a partly
changed frame in part.

It answers the question it was written for. **Yes, Direct2D can consume a
scene**, and it takes no second implementation: the replay walks the primitives
and calls the same `paint.h` entry points the tree would have called, so the
D2D backend and the GPU backend draw the same scene through the same code and
neither can tell. `scene.cpp` names no OS type and no GPU type. What it needed
from `paint.h` was two entry points — `TextLayoutSize`, so a text primitive
knows what area it covers, and `PathRealize`, which is where D2D builds a
geometry realization — plus the `paintLayer` field on `PaintCtx`.

The numbers are in `scene.h`; four things they say, and the third is the one
worth carrying forward:

- **What pays is culling, not collecting.** `replay` alone takes 41% off the
  story gallery's D2D paint (1.46 → 0.86 ms), and none of that is ordering or
  batching. 799 of the story's 1071 primitives have a content mask that has
  already reduced them to nothing, and a tree walk hands every one of them to
  the backend to be clipped anyway. On the GPU backend the same cull goes from
  6219 instances in 243 draws to 1613 in 39.
- **Reordering buys nothing.** The quad path was already one batch, the two
  paint walks were already in layer order, and the replay changes the clip 26
  times where the tree pushed 17. A scene is not how this renderer starts
  batching, because it already did.
- **The path cache is how D2D gets what the GPU backend has by construction.**
  99% of path lookups hit; filling a geometry realization instead of a geometry
  takes another 12% off (0.86 → 0.76 ms). It costs 0.03% of the story's pixels,
  all on curve edges, because a realization is tessellated once at one
  tolerance.
- **Skipping is worth the most and means the least.** A benchmark redraws one
  frame 600 times, so 97% of them are identical and the paint phase falls to a
  tenth. Read 0.25 ms as what an idle window costs, not as what the story costs
  — an idle window was already not redrawing.

And the counterweight, which is why none of this is on by default:
`fps_monitor`, where 2443 primitives all change every frame, pays **20% on top**
of the D2D paint to collect and hash a scene nothing can cache or skip, and the
damage rectangle comes out at 96% of the view. This is a win on a scene that is
mostly still and a loss on one that is not.

Two things are implemented but not measured, and both for the same reason.
`damage` compares the two frames as multisets of primitive hashes — the first
version compared them position for position and gave up whenever the counts
differed, which is every frame that matters, since a hover *adds* a background
fill — but the two cases that would show it working, a hover and the system
monitor's 500 ms tick, need input or a timer that `GPUI_FRAME_BENCH`'s own 1 ms
timer displaces, and this session's desktop would not bring a window to the
foreground. What is measured is the mechanism, not the payoff.

Not done, in `scene.h`'s own words: layers are a field rather than a stacking
context per element, there is no offscreen mask cache (the cache is of
geometry, one level below what Blade caches), the path cache is keyed on
absolute coordinates so an icon that moves misses, a shaped run is identified
by its address, and only `paint_win.cpp` dispatches into the recorder — the
other three backends need the same one line per entry point.

The standing non-goal in `AGENTS.md` — "Zed's scene graph as a whole" — is
left as it is on purpose. This is a prototype behind a flag, and what it
measured is an argument for a *cull* pass and a path cache, which want neither
a scene nor a graph, rather than for the scene graph itself.

### And then it became the default

`GPUI_SCENE` defaults to `skip`, and `GPUI_SCENE=off` is how to get the old
behaviour back — the first thing to try if a frame ever comes out stale, since
the scene is the only thing in the tree that can decide not to draw. Whole
frames, 600 each, off against on:

    story             D2D  3.98 -> 2.68    GPU backend  3.06 -> 2.55
    showcase          D2D  0.31 -> 0.12    GPU backend  0.48 -> 0.12
    system_monitor    D2D  0.57 -> 0.12    GPU backend  0.24 -> 0.10
    fps_monitor       D2D  0.82 -> 0.91    GPU backend  0.26 -> 0.35

Three things had to be true first. **A resize invalidates the scene**:
`ResizeBuffers` hands out surfaces with nothing in them, so what the scene
remembers about the surface is no longer on it — `scene::Invalidate()` is that,
separate from `Reset()`, which also drops the path cache and is for a device
going away. **The primitive hash reads ten words rather than a hundred-odd
bytes**, which halved what the scene costs on a frame that cannot reuse
anything: `fps_monitor` went from +20% to +11%. And the **stale-text worry in
the first commit was overstated** — a shaped run is identified by its address,
but its own size is hashed beside its position and colour, so a false match
needs text that shapes to the same size in the same place in the same colour.

Verified by pixel against `GPUI_SCENE=off` on eight examples, four repeats:
seven identical, and the story gallery differs on 497 pixels — 0.03%, 45 of
them by more than a hair, all on the curve edges of icons, because a cached
path is filled from a geometry realization and an uncached one is not.

**The GPU backend stays behind `GPUI_PAINT=gpu`**, which is the other half of
"make the fastest the default" and the half not taken. With the scene on it is
0.13 ms a frame ahead of Direct2D on the story gallery — 5% of a frame that
spends 2.3 ms in layout — and it differs from Direct2D on 2.74% of that page's
pixels, every one of them text: the atlas is grayscale where DirectWrite draws
ClearType, and glyph x is snapped where DirectWrite positions at a third of a
pixel. That is the trade the `paintgpu.h` note already named, and 0.13 ms does
not buy it. Where it *is* worth reaching for is a page that is mostly paths and
mostly moving: `fps_monitor` is 0.35 ms against Direct2D's 0.91.

## ArenaVec's two links are four bytes each

`ArenaVecSegment::next`, and `ArenaVec::first` / `last`, are `ArenaPtr` rather
than pointers — an offset into the arena's position space. The handle is 12
bytes where it was 24 and a segment's header is 16 where it was 24. An offset
and deliberately not a delta from the object's own address, which would have
been cheaper to follow and would have needed no arena: two blocks of one arena
are two separate reservations, and address-space layout can put them further
apart than an int32 reaches.

What it costs is the arena at every dereference, so `operator[]`, `begin` and
`end` became `At(a, i)` and `In(a)` — `for (const Event& e : entry.add.In(a))`
— and `Truncate` and `Pop` take one too. That is about a hundred call sites
across `src/markdown` and `src/ui`; every structure that holds one already
carried an `Arena* a`, except `SettingGroup`, whose two matchers took the arena
as a parameter instead of the struct growing an eight-byte field to save
twelve. `ArenaAtOffset` moved into the header and grew a fast path for the
newest block, which is where all but a handful of offsets land — without it the
parse was 2% slower rather than level.

`bun cmd/bench.ts markdown`, release, best of three runs of twenty samples:

                    scratch arena          tree             parse (min)
  prose            1280.9 -> 1230.4 KB   272.0 -> 259.6 KB   7.98 -> 8.06 ms
  nested quotes     979.8 ->  925.0 KB   110.2 -> 110.2     8.90 -> 8.90
  gfm tables       4010.1 -> 3830.5 KB   250.5 -> 250.5    11.86 -> 11.60
  character refs    886.9 ->  878.4 KB    65.6 ->  65.6     5.86 -> 5.95

So 4% off the scratch arena and 4.6% off the tree, and the times are inside the
noise of the machine — which is the trade: the memory is the point and the
extra load per dereference does not show. `markdown/phases` puts what movement
there is in `to_mdast`, 0.298 -> 0.308 ms, which is `TailMut` translating an
offset on every event. **Layout does not move at all**, in either sense: the
taffy benchmarks are unchanged case for case, since taffy has no `ArenaVec` in
it, and the story gallery's frame is the same to three decimals — build 0.195,
layout 2.29, paint 0.21 — before and after.

The saving in the arena is the segment header and nothing else. A handle that
lives in a `Vec<Entry>` or a `Vec<TreeFrame>` shrinks the heap rather than the
arena, and there is no benchmark number for that: an edit map entry went from
32 bytes to 20 and a compile frame from 56 to 32.

`cmd/vec-log.ts`'s replay models the header, so its `kSegHeader` went to 16
with it; the ground truth is `ArenaUsed`, which `bun cmd/bench.ts markdown` now
prints beside the tree size as a `scratch` row — the scratch arena is where
every ArenaVec lives and it was the one number the markdown benchmark could not
see before.

Verified by 17,148 test checks and by seventeen story pages screenshotted under
both builds — menu, popup-menu, native-menu, tabs, resizable, tiles, setting,
the three charts, modal, text, sidebar, dropdown, list, table, accordion —
every one pixel-identical.

What is left on the table: the handle could be eight bytes rather than twelve.
`last` is there so an append does not walk, but the whole markdown run averages
1.02 segments to a vec, so what it saves is one compare. Dropping it needs
another way to find the empty segments `Truncate` leaves linked past the end.

### And the arena moved inside, so the API is the old one again

`ArenaVec` carries the `Arena*` its offsets are into. `operator[]`, `begin`,
`end`, `Truncate(n)` and `Pop()` are back to what they were, the ~100 call
sites across `src/markdown` and `src/ui` went back with them, and
`SettingGroupMatches` / `SettingPageMatches` lost the parameter they had
grown. Every mutator was being handed the arena already, so nothing else
changed shape: a vec still declares as `ArenaVec<T> v = {}`.

The handle is 24 bytes again — an arena, two offsets and a length — which is
what it was before any of this. What the ArenaPtrs buy at that size is that
carrying the arena is free: two real pointers beside it would have made the
handle 32.

**The memory result is unchanged**, to the byte, because all of it was the
segment header:

                    scratch arena              tree          parse (min)
  prose            1280.9 -> 1230.4 KB   272.0 -> 259.6 KB   7.99 -> 7.92 ms
  nested quotes     979.8 ->  925.0 KB   110.2 -> 110.2      8.88 -> 8.85
  gfm tables       4010.1 -> 3830.5 KB   250.5 -> 250.5     11.96 -> 11.87
  character refs    886.9 ->  878.4 KB    65.6 ->  65.6      5.92 -> 5.88

Best of four runs of twenty samples. The parse is now level with the original
or a shade under it, where the arena-in-the-signature version was a shade over
— not a speed-up worth claiming, but it does settle that putting the arena in
the handle costs nothing to read through. `to_mdast` is the one case that
moved, 0.298 -> 0.311 ms: `TailMut` reads the top of a stack once per event
and now translates an offset to do it. Layout is unchanged, both the taffy
benchmarks and the story gallery's frame.

One thing had to be got right and the compiler caught it: recording the arena
on **every append** cost 1% of the parse — an append that has room writes one
element and should not also write the handle — so it is recorded in
`NextSegment` and nowhere else, which is once per segment. That leaves `a`
null until the first segment exists, which is exactly what a vec with no
segment wants: `ArenaPtrGet` answers null for a null arena, so the first
append finds no segment and goes and asks for one. `Append`, `AppendMany` and
`Reserve` therefore have to pass their *own* parameter to `NextSegment` rather
than the member, and MSVC's unreferenced-parameter warning is what found the
version that did not.

What the revision gives up, and it is worth being plain about: the 12-byte
handle would have taken an edit map entry from 32 bytes to 20 and a compile
frame from 56 to 32. Both live on the heap, so neither shows up in any number
this tree measures — which is the argument for taking the simpler API and the
reason the saving was not worth a hundred call sites. If it comes back it
should come back for a structure that holds a handle *in* an arena.

Verified again by 17,148 test checks and by the same seventeen story pages
screenshotted against the build from before any of this: all seventeen
identical.


## Six things the story gallery got wrong, and what each of them was

Six reports off the running app, and none of the six turned out to be about
the page it was noticed on: five were the shared layer underneath, and the
sixth was a whole element the SVG reader had never had.

**The app menu did nothing.** `StoryTitleMenuItem(cx, "GPUI Component")` was a
div with a hover background and a click id, and nothing behind it — the other
three title-bar menus were `DropdownMenu`s and this one had never been given
its rows. It is `app_menus.rs`'s first menu now: About, the Appearance pair
(Light / Dark), the Theme submenu the registry fills, and Quit. Rust's also has
`Open...`, which wants a file dialog this tree does not have, and `Language`,
which wants rust_i18n; both are named here rather than quietly dropped.

**The submenu was there all along and eight pixels of it were showing.** The
first thing the new menu needed was a submenu, and it did not draw — nor did
the two in the `menu` story, which had been that way since the menu was ported.
`PopupMenu`'s box had `ClipY()` on it, the submenu hangs off its row at
`Left(menuW - 8)`, and a clip is a clip on both axes: what got through was the
eight pixels of overlap, which reads as a shadow. Rust clips the *item list*
and only when the menu scrolls, so that is where the clip went.

**An open menu did not go away.** Neither the title bar's `PopupMenu`s nor the
story toolbar's own dropdowns closed on a press outside them, and the toolbar's
did not close on a row either. Two fixes, because they are two mechanisms:
`PopupMenuState::OnPressOutside` is `on_mouse_down_out` — the release rather
than the press, since that is the outside event this tree reports, and it runs
before the click the same release makes, so a row still runs after the menu it
was in has gone. It carries Rust's two refusals: a press inside the parent
menu, and a press while a submenu of this one is up. The trigger is the third,
and is this tree's own: the trigger toggles the menu itself, so dismissing on
it as well would close what the toggle then reopens. The toolbar's dropdowns
own their open flag on the page, so they get the same behaviour from the Popup
root — which is the trigger's box, so a press on the trigger is inside it.

**The sidebar's search box was in the wrong font.** `gpui::Input` and
`component::Input` are the unstyled editor and the themed field around it, and
the gallery had been holding the first: it draws at `InputEditorStyle`'s own
12px default rather than at `input_text_size`, which for a medium field is 14.
`Input::new(&search).appearance(false).cleanable(true)` is what gallery.rs
holds, and now so does this — the X in the pill is the field's own clear button
rather than a hand-rolled one. Measured: the placeholder's ink is 51px wide,
which is Rust's to the pixel; it was 44.

**Weight did not inherit, and colour inside a blockquote did.** Two halves of
the same mistake. `PrepareEl` cascaded the font *family* to children and not
the weight, so `font_medium()` on an accordion's trigger row styled nothing —
GPUI inherits both, and an accordion title, a collapsible's `$18.08 / $20` row
and every other `->Medium()` on a box rather than on a string had been drawing
at normal. Going the other way, `TextView` painted every paragraph and heading
in `theme.foreground` explicitly, which no `node.rs` render does: they inherit,
which is how a blockquote greys everything inside it with one `text_color`.
`TextView::BlockFg()` is that inherited colour.

**A link inside selectable text showed the I-beam.** `DispatchMouseMove` asked
`TextHitOffsetAt` first and the element's own cursor second, so a `TextView`
that is `Selectable()` claimed every pixel of itself. An element that named a
shape wins now, and anything that named none leaves the text to answer — which
is the order GPUI's cursor stack resolves in anyway, the innermost push last.

### The badges at the top of the introduction, and `<text>`

The three build-status badges came out as empty plates: shields.io and GitHub
both write a badge as coloured rectangles with the label *set in the file*, and
`svg.cpp` had no `<text>` at all. So it has one now, and `drawops.h` has
`kOpText` to carry it — `x y size textLength`, a flags word for `text-anchor`
and `font-weight`, and the UTF-8 inline. Three things came with it:

- **Containers pass presentation down.** The `<g>` stack was a stack of
  matrices; it is a stack of `SvgCtx` now — matrix, font size, anchor, weight,
  fill — because a badge names all of those on the groups and none of them on
  the element holding the word. `<text>` and `<tspan>` push one too: GitHub
  writes `<text fill=..><tspan x=.. y=..>CI</tspan></text>`, and shields.io
  writes the word directly inside a `<text>` under `<g transform="scale(.1)">`,
  which is what turns a font-size of 110 into eleven pixels.
- **`filter=` means skip.** A shields.io badge draws its label three times, two
  of them blurred drop shadows. There is no blur here, so a filtered run is
  dropped rather than printed sharp on top of the real one.
- **`url(#id)` resolves to a gradient's first stop.** Not for text — for the
  plates under it, which are `fill="url(#workflow-fill)"` on the GitHub badge
  and were drawing in the caller's colour, i.e. as an outline on white. One
  colour per shape is what the byte stream can hold, and the first stop is the
  same reduction `try_parse_theme_color` makes of a `linear-gradient(..)` in a
  theme file. `stop-opacity` had to come with it: the sheen shields.io lays
  over the whole badge is `#bbb` at a tenth, and reading the colour without the
  opacity washes the plate out to grey.

What it is not: there is no horizontal scale in `paint.h`, so `textLength`
cannot stretch a run to a width the way SVG asks. A run too wide for the width
it was drawn to fit is set smaller until it fits and a narrower one is left
where its anchor puts it — a badge is authored against Verdana and this is not
Verdana, so the two were never going to agree to the pixel. Nothing under
`assets/icons` has a `<text>`, so `asset_icons.cpp` never holds a `kOpText` and
`cmd/svg-to-bytecode.ts` has nothing new to write.

One thing the scene caught: a text primitive is recorded by the `TextLayout*`
it was drawn with and replayed after the walk, so shaping a run and releasing
it on the spot handed the replay a freed layout — the glyphs appeared under
`GPUI_SCENE=off` and nowhere else. `DrawTextBaseline` goes through the frame's
measurement cache like every other run, and the cache is what holds it.

Verified by 17,169 test checks — four of them new, over `<text>`, `<tspan>`,
the filtered shadow and a `<text>` that only wraps another element — and by
every one of the 63 story pages screenshotted against the build from before any
of this. Three pages differ beyond the search box, and all three are the fixes:
the accordion's titles, the collapsible's usage row (`font_medium` on the row
in `collapsible_story.rs`, and now medium here too), and the introduction. The
spinner is the fourth and is not one of them — its spinners are mid-turn in
each shot, which is what an animation looks like to a screenshot.


## Seven more the gallery got wrong, and three of them were the layer under it

The sweep picked up where the last one left off: every one of the 62 story
pages shot beside the Rust original again, ranked by how much of the frame
differs, and worked down the list. Four of the seven were the page; three were
`src/ui/` or `src/gpui/` and showed up on the page that happened to name them.

**A compact button had the padding and not the width.** `button.rs` spells
compact as the tighter `px` *and* `min_w_5` / `min_w_6` / `min_w_8`, so a
labelled compact button is never narrower than it is tall. Ours had only half
of it, which left every pagination page number six pixels light and the whole
row twenty short. The same page's ellipsis was an icon button with nothing
behind it, where `pagination.rs` hangs a dropdown of the pages that window hid
off it — the only way into the middle of a long run without walking the arrows.
The menu reports the row that was taken rather than the page it stands for, so
a keyed `PaginationMenuState` carries the range's first page across.

**A DataTable row was 38 unrelated numbers.** Every column past Chg% came out
of a hash of the cell scaled by its kind, so Volume, Turnover, Market Cap and
TTM all read in the same millions. `random_stocks_exact` says in its own
comment why that is wrong: the fields of a row hang together the way a real
quote does. So they do here now — turnover is that volume at that price, the
market cap is the price over shares drawn from 1e6..3e9, which is what puts it
in the billions and trillions, TTM is 5..80, the rankings 0..1000, and the bid,
ask, open, high and low stay inside a day's range of the price. `compact()`
grew the `T` it needed and reads a negative by its magnitude. Two smaller arms
of the same `render_td`: a market that is not US writes in magenta, and the
price is `font_semibold`.

**Three quarters of the theme viewer was unreachable.** The right panel was a
640-tall box with `ClipY` on it: Global through Base showed and the fifteen
categories under them did not, with no way to scroll to them. Rust's is
`size_full()` around a `list()` with a vertical scrollbar, so it is that here.
Three tokens the registry had been resolving and throwing away are Theme fields
now, which is what lets the page list them where Rust does —
`primary.hover.background`, `primary.active.background` and
`accent.foreground` — and Base grew the six `.light` hues beside the six it
had. What still differs there: Rust hides a token the theme file did not name
unless *Inherited Colors* is on, and nothing here records which key came from
the file; and a hex reads a digit apart in places, because Rust shows the
colour after a round trip through HSLA `f32` where this keeps the eight bits
the file spelled.

**A radar plotted from its own smallest value.** Three things
`radar_chart.rs` settles that the painter had been doing its own way: the outer
ring is `bounds.height * 0.4` rather than half the smaller side less sixteen,
which on a wide card drew a web big enough to run under its own labels; the
scale chains a zero into its domain — "so non-negative data starts at the
center" — where ours ran from the smallest value, pinning that month to the hub
and stretching every other spoke; and a label is anchored ten past the ring and
aligned by the side it is on rather than centred in a fixed 48-wide box.

**A paragraph that names no colour is not a paragraph coloured `foreground`.**
The last pass gave `TextView` an inherited block colour and left its default at
the theme's foreground, which is not the same thing. Inside a red Alert the
body still painted itself black while the bullet beside it — a plain string
child, so a real inherit — came out red. `BlockFg`'s unset value is a sentinel
now that `Word` and `Inline` read as "set no colour at all", and the run takes
what the container above the view pushed.

**The code editor drew a point small.** `theme.mono_font_size` is 13 and the
highlighter drew at 12. A row that is narrower is a row whose long `use` line
fits where Rust's soft-wraps, so every line under it sat a row too high.

**A list row shrank, and two sections fell off the end.** The rows are laid in
a flex column with the viewport's height on it, and a column shrinks what
overflows it: every 44-tall slot came out at 34, so the fifteen rows the
visible range had worked out against `rowH` covered five sixths of the box and
the last two sections could not be reached at all — a band of empty list under
the last row and no scrollbar. The slots and the two spacers are `Shrink0` now,
the way `DataTable`'s rows already were. With the rows at the height they
claim, the story's own 44 was too tall: Rust measures the item it built, and
that item is 36.

Verified by 17,169 test checks and by all 62 pages shot again afterwards: the
pages this touched moved and nothing else did — `button`, which exercises every
compact variant, is pixel-identical to the run before the `min_w`.

What this sweep found and did not fix, both named where they are: a no-wrap
`Textarea` has no horizontal scrollbar (Rust builds one from `!soft_wrap`,
where the offset here lives inside the input engine rather than on the box, so
the element machinery never learns the content is wider than the view), and the
`table` story's Amount column is laid out a little wider than Rust's.

## The four things a language server tells an editor, and the last two the
## markdown example wanted

The editor example arrived with the wavy underlines and the popover a
diagnostic puts up, and a note saying its store's other four halves — the
completion menu, hover, code actions and document colours — were still to
come. They are here, and each one is the same shape: Rust's provider trait
answers a `Task`, and there is nothing to await in this tree, so a provider
here is a function pointer that answers where it stands.

**The completion menu.** A word character or a `.` asks the field's provider
and what it answers hangs under the caret: the labels on the left, each with
its LSP `detail` beside it, and the selected item's documentation rendered as
markdown in the pane next to the list. Up and down walk it, enter writes the
item's insert text over the word it was completing, escape puts it away, and
backspace asks again on what is left. Placing it wanted something the tree did
not have: only the painter knows where inside a shaped run an offset falls, so
the row that draws the caret now reports where it landed and the menu is put
there, one frame stale, which is how every other popover here is placed.

**Hover.** The pointer resting on a word asks what it is, once per word —
while it stays inside the word it was asked about, what the provider said
stands — and a diagnostic under the pointer wins, being the more specific of
the two. Rust delays the request 150 ms because it is about to talk to a
language server; the frame that notices the pointer moved is the one that
shows the answer here.

**Code actions.** Ctrl-. asks what can be done to the selection and offers the
answers in a column under the caret. A `CodeAction` here is a title, a range
and the text that replaces it: Rust carries a `WorkspaceEdit`, which is a map
of documents to edit lists, and a field is one document that every action
upstream writes makes one edit to. The provider writes its strings into an
arena that lives as long as the menu is up. The example answers with
`TextConvertor`'s five — upper, lower, titleize, capitalize, snake_case.

**Document colours.** What the document names in colour is painted behind the
text that names it, which is `layout_document_colors` — the same quad a search
match is washed with, so the two go into one wash list per row. An edit marks
the set stale and the next frame's row builder asks again, where Rust asks on
a timer and keeps the answer only when it differs. The example scans for a hex
literal in its four lengths and an `rgb()`/`rgba()` call, where upstream hands
the text to the `color-lsp` crate.

The keyboard half of all this is pinned by tests rather than by screenshots:
the harness posts `WM_KEYDOWN` and `SendInput` is dropped on this machine, so
a synthetic ctrl chord never reaches `GetKeyState` and a menu cannot be driven
from a screenshot. The state machine — what the provider is asked, what the
four keys do, what accepting writes, what an edit invalidates — is 17,261
checks' worth of `InputStateTests`, and the rendering was checked by opening
each menu from the example's own setup.

**A table can be as wide as its content.** `render_scroll_table`, and the
`Table: Scroll / Wrap` button the markdown example toggles it with. The
columns are as wide as the widest text in them — measured, since a character
count is a poor guess on a proportional font — and they grow to fill a frame
wider than the content. A narrower frame squeezes them, their text wrapping,
until each is down to a floor set by what it holds; below that the table keeps
the floors and scrolls sideways, so nothing it holds is out of reach. Each
table's offset is its own keyed state, named by which table in the view it is:
the parse is rebuilt every frame, so a node's address is not a name that
lasts, and its position in the document is.

**The desktop's own open dialog.** `cx.prompt_for_paths`, which is what the
markdown example's `Open` action wanted and what this tree had none of.
Windows uses `IFileOpenDialog` — COM is already up for the drag-and-drop
registration — macOS `NSOpenPanel` run modally, and X11, which has no dialog
of its own and no toolkit here to borrow one from, asks the desktop's: zenity
or kdialog, whichever is on the PATH. A page can do none of this without a
gesture and a callback nothing has asked for, so wasm says so rather than
pretending. One path, no task: the platform dialogs run their own loop until
the user is done either way.

What is left of the markdown example, and the last thing named in its header:
`Selection: Plain / Source`, which copies the markdown behind a selection
rather than the text of it. Upstream reconstructs each selected block's source
from the node itself — no offsets involved — but the selection here is a range
over the runs the window painted, and nothing maps a run back to the block
that produced it. That map is the work, not the serializing.

Two things the sweep turned up on the way: `clang-format` had been unpacking
the generated icon table into one byte a line, twenty-two thousand of them for
what the generator wrote in fourteen hundred, so the formatter now leaves
generated files alone; and the language table's new `markdown` flag needed a
default, since gcc counts a missing last initializer as an error where MSVC
does not — the Linux build had been broken since the scanner learned markdown.

**A bar down one axis only.** The VirtualList story's `Axis: Both / Vertical /
Horizontal` button, which had been changing its own label and nothing else.
Rust hangs the bars off the list with `.scrollbar(&self.scroll_handle,
self.axis)`, one bar layer per axis named — so the axis says which bars are
drawn, not what scrolls, and a list set to Vertical still slides sideways
under the wheel. Here the box paints its own pair rather than wearing a
sibling layer, so what it needed was the other half of `HideScrollbar`:
`HideScrollbarX` and `HideScrollbarY`, one axis at a time. A bar that is not
painted is not there to grab either, so the pair rides into `ScrollRect` as
`barX`/`barY` and `ScrollbarAt` skips the band of a bar the box does not show
— otherwise the bottom eight pixels of a Vertical list would still answer a
press with a drag of a thumb nobody can see.

**The theme registry read the whole `themes/` directory every frame.** The
story's title-bar `AppMenu` calls `ThemeRegistryLoadDir("themes")` as it
builds, so the twenty-odd theme files were re-read and re-parsed once per
frame into `gArena` — the arena a theme's name and colour object point into,
which nothing ever hands back. `ThemeRegistryLoadStr` dropped every one of
those parses on the floor (the name is in the registry already, so the config
is skipped) and the arena chained another 64 MB block every second frame:
`GPUI_FRAME_BENCH=3000 story.exe` peaked at 57 GB, and building a frame cost
5.1 ms of file I/O and JSON.

Two fixes, both in `src/ui/theme_registry.cpp`. `LoadDir` remembers the
directories it has already read — callers mean "make sure these themes are
loaded", and a theme is never dropped, so a second pass can only find what is
there — and `LoadStr` marks the arena before it parses and pops back to the
mark when the document added nothing, since a document no config points into
pins nothing. Story now sits at 96 MB flat over 3000 frames and a frame costs
2.75 ms, build 0.27 ms.

How it was found: the arena is the only allocator big enough to lose that
much, so the search was for the arena that grows and never resets — a counter
on `PlatMemCommit`/`PlatMemReserve` said the growth was arena regions rather
than the heap, and `_ReturnAddress()` captured in `ArenaNew` and symbolized
with `dbghelp` at the point a chain block is added named `ThemeRegistryInit`.
`GPUI_VEC_LOG` (`cmd/vec-log.ts`) was the first stop and ruled the heap out:
births with no matching death came to 2.3 MB over 120 frames, three orders of
magnitude short of what the process was holding.

- 2026-08-24: **a border takes room.** `ToTaffyStyle` translated padding, gap,
  flex and inset and never once set `t.border`, where GPUI hands
  `Style::border_widths` straight to taffy — so a border here was painted
  inside the box and reserved nothing, and every bordered box in the tree was
  its border's width shorter and narrower than upstream's. One line of
  translation, and the per-edge rule that goes with it: `border` is the
  all-round width and `borderT`/`B`/`L`/`R` are the per-edge ones, the two are
  independent, and paint draws both — so an edge reserves the *larger* of the
  two rather than their sum. `tests/MinSizeTests.cpp` pins all three: a
  content-sized box grows by its border, a box of a named size keeps it and
  insets its content instead (a 16px checkbox stays 16 and its tick is drawn
  in the 14 inside), and `Border(1) + BorderB(3)` is 4 tall, not 6.

  Measured against the Rust story page by page, in a column of the content
  pane that no text crosses, comparing the run of gaps between the horizontal
  rules — which is every card's height and every gap between cards:

  | | spans that match upstream exactly |
  | --- | --- |
  | before | 181 / 455 (40%) |
  | after | 312 / 435 (72%) |

  45 of the 62 pages improved, 16 did not move, and the one that scored worse
  — `data-table` — has the same 32px row pitch in both and lost two spans to a
  hovered row's edge crossing the sample column. The shapes this fixes are the
  ones the arithmetic predicts: a story card is 49 tall where it was 47, and a
  section repeats every 81 where it repeated every 79. Upstream's numbers.

  Whole-image pixel diffs get *worse* on most pages and are the wrong
  instrument here: what is left is a one-pixel offset in the page header — our
  24px title line rounds a hair differently from Rust's — and a single pixel
  of vertical shift relights every glyph on the page, which swamps a metric
  that cannot tell a shifted layout from a wrong one. The rhythm above is what
  says the geometry is right.

  One thing had been leaning on the bug. The story's toolbar draws its border
  once around the group; upstream has no container border at all
  (`story_toolbar_group()` is a bare `h_flex`) and puts it on each
  `Button::outline().small()`, whose explicit height already contains it. With
  a border costing nothing the two agreed; with it costing two pixels our
  toolbar row became two taller than every one of upstream's. The border is
  now an absolute child filling the group — the same ring
  `ListActiveOverlay` draws for a selected row — so it paints the joined pill
  it always did and costs no layout. That matters for more than the story
  shell: `resizable` and five other pages put full-size `Button::Outline()`
  children in that group, which carry their own borders, so upstream's group
  is bare for them too.

  It also closes the list row the entry below this one left at 34.
  `list_story.rs` refines its row with `.border_1()`, which this port could
  not write because the border would have cost nothing; `ListItem` now takes
  the `refine_style(&self.style)` upstream has, applied before the selection
  the way list_item.rs applies it, so a selected row's own fill and ring still
  win. The story's rows are 36 apart, which is upstream's number.

  17,525 checks pass; `bun cmd/build.ts -rel -all` and `-dbg -all` build all
  24 examples, and the taffy benchmarks are unmoved.

- 2026-08-24: **five reported depth gaps, and the two of them that were still
  open.** Checked one at a time against the Rust at the pinned SHA.

  Already done, and verified rather than redone: the **no-wrap Textarea's
  horizontal scrollbar** (`2b01c4c`; `!softWrap → box->ScrollX(..)` in
  `ui/input.cpp`, and `gpui.cpp` paints a real track and thumb once
  `contentW > w`), and **element opacity** (`Style::opacity` has been there
  since 2026-08-20, and both things named as waiting on it already use it —
  `dialog.cpp` fades the whole layer by the motion's delta, and
  `notification.cpp` fades a card past the third by its `"toast-visibility"`
  transition).

  The **theme viewer's Inherited Colors** turned out to be done too, and the
  reason it looked open is worth writing down: `ThemeConfig` keeps the
  file's parsed `colors` object rather than a struct of fields, so
  `ThemeConfigNames` *is* the record of which keys a file named — aliases
  included, since `FindColor` resolves them. What the page cannot do is match
  the rows whose mapper name and schema name differ, and that is upstream's
  own wart, not a gap here: `mapper.rs` has no `button*` arm at all, so
  `parse_theme_key("button_primary_hover")` falls to the default and its
  `canonical_key` is `button_primary_hover`, where `config_keys` holds
  `button.primary.hover.background`. They can never match, so every `button.*`
  row reads as inherited in the Rust story as well. Nothing pinned any of
  this, so `ThemeRegistryTests` now does.

  **Buttons read the `button.*` tokens instead of recomputing them.**
  `Button::IntoEl` had one computed mix per variant — Primary hovered to
  `RgbaMix(primary, foreground, 0.85)`, the status variants to their accent at
  0.3 — and a theme file naming `button.primary.hover.background` changed
  nothing. Those numbers were never wrong; they are the *fallbacks*
  `schema.rs` spells, and `ThemeFillDerived` already computed all of them onto
  the tokens. So the fix was to delete the second copy: every variant now
  takes `tokens.button*`, `tokens.button*Hover` and `tokens.button*Active`,
  and only Ghost still computes — it has no token family upstream either, and
  its formula is `secondary` lightened in dark and darkened in light at 0.8.

  What that exposed is that the port had no pressed state at all. `El` had
  `hoverBg` and nothing beside it, so `.active(|style| style.bg(..))` had
  nowhere to land. `Style::activeBg` is that, `PaintCtx::activeId` is GPUI's
  `clicked_state.element` — the id the press landed on, held until the button
  comes back up, so it does not follow the pointer the way the hover does —
  and `BoxFillFor` is the three-way choice, split out of the paint pass
  because the pointer cannot be driven from a test. Two more of button.rs
  fell out of it once there was a state to put them in: `selected` is the
  variant's own active fill rather than `secondaryActive` for everybody, and
  a selected or loading button now has no hover and no press, which is what
  `when(!disabled && !selected)` and `when(interactive)` say. Link and Text
  paint no fill in any state and move their *ink* instead, so the button
  passes its colour down and the label and icons inherit it rather than each
  naming it — the caret keeps naming its own, since Rust builds it from
  `normal_style.fg` and it does not follow the state.

  **The list measures its rows.** `prepare_items_if_needed` lays out the item
  at `item_to_measure_index` on its own, and the section header and footer
  beside it, and `RowsCache` turns the three into a size per flattened row;
  this tree pinned one `rowH` of 32 for all of them and the story hand-set 36
  over the top. `MeasureEl` is `layout_as_root(size(MinContent, MinContent))`
  — the same pass `LayoutEl` runs, with the space left to the caller — and
  `ListPrepareRowHeights` is `prepare_if_needed`, rebuilt only when the
  sections or the three measured heights move. The rows then go through the
  non-uniform half of the virtual list, which was already there:
  `VirtualListVisibleRange`, `VirtualListItemOrigin` and
  `VirtualListContentSize` over the sizes, and `ListScrollToItem` with them.
  The story's header and footer had drifted while every row was being forced
  to one height — `pb_1` written as a padding on *top*, and the `pb_5` that is
  the gap between sections missing altogether — and both are Rust's now.

  The table was not touched, and should not be: `table_row_height()` is a
  constant of the size in Rust too (26/30/32/40), which `ui/sizing.h` already
  matches, and `state.rs` reads it the same way this tree does. Only the list
  measures upstream.

  **One thing found on the way and left for its own pass: a border took no
  room.** `ToTaffyStyle` never set `t.border`, where GPUI hands
  `border_widths` straight to taffy, so every bordered box in this tree was
  its border.s width shorter and narrower than upstream.s. It is why the list
  row measured 34 where Rust.s is 36. The entry above this one is that pass.

  17,512 checks pass and `bun cmd/build.ts -rel -all` builds all 24 examples.

- 2026-08-24: **a story frame is 2.29 ms, from 2.62.** Profiled with `winperf`
  again (`record -i 2000 -write-agent`, 1500 frames under
  `GPUI_FRAME_BENCH`). Layout is still the frame — 66% of the samples inside
  `ComputeRootLayout` — and four things came out of the profile, all of them
  work the frame was doing twice or work it was doing out of line.

  **The measure callback asked the shaped-run cache the same question four
  times a node.** Taffy asks a text leaf for its size several times a pass —
  the min-content width, the max-content width, then the width the line
  settled on — and each of those went the whole way into `MeasureText`, which
  hashes the string, probes the table and `memcmp`s the run back. Font size,
  weight, wrap and line height are settled by `PrepareEl` before layout
  starts and the `El` is built afresh every frame, so the width is the entire
  key: `El` now carries four (width → size) answers and `LayoutMeasure`
  consults them first. Layout 1.99 → 1.85 ms.

  **`MeasureText` took a reference on the shaped run and gave it back one
  line later.** It only ever wanted the size, and an `IDWriteTextLayout`'s
  AddRef/Release pair was 3% of the profile — `ComObject<DWriteTextLayout>::
  AddRef` was the 14th hottest function in the process. On a cache hit the
  slot already holds the width and the height, so it answers from there and
  never touches the run.

  **`Cache{}` per node per frame.** The tree is rebuilt from nothing every
  frame, so `InsertNode` reset each recycled node's layout cache by assigning
  a fresh one — a few hundred bytes of zeroes for ten entries whose contents
  nothing may read while their `present` flag is false. The flag is now a
  `presentMask` on the `Cache` rather than a `bool` per entry, so emptying a
  cache is one store; `Clear()` leaves the contents alone. Dropping the bool
  also took the padded entry from 32 bytes to 24, which matters again on the
  lookup side, where a story's caches are far larger than L2 and every get
  and store is a miss. `Cache::StoreWithKey` 2.9% → 0.8% self, `GetWithKey`
  2.1% → 1.6%, `InsertNode` off the list.

  **`inline` is not a request.** The component-wise `MaybeMax`/`MaybeAdd` and
  their four siblings — two calls to the scalar overload and nothing else —
  were compiled out of line at `/O2` and held 3.5% of the frame's samples
  between them. `TAFFY_INLINE` (`__forceinline`, `always_inline` elsewhere)
  is the request; they vanish from the profile.

  Frame: build 0.25 ms, layout **1.85 ms** (was 2.20), paint 0.23 ms. Taffy's
  own benchmarks, where no text is measured and no `El` exists, keep the last
  two: flexbox 6-9% faster across every case, grid 8-20% (`wide 100x100`
  20.4 → 17.5 ms, `deep 2x2 16384` 102 → 89 ms). All 62 story pages shoot
  pixel-identical before and after except `skeleton` and `spinner`, which
  differ run to run against themselves because they animate. 17261 checks
  pass.

  Measured and thrown away, so the next session does not retry it: an
  **inline stack buffer for the `Vec<FlexItem>` and the `Vec<BlockItem>`**.
  The profile blames 2.5% of the frame on `RtlFreeHeap` and
  `RtlpLowFragHeapAllocFromContext` under `GenerateAnonymousFlexItems` and
  `GenerateItemList` — one malloc and free per container per layout pass —
  but twelve items of stack on each recursion level bought 1%, the same
  answer the session before this one got at four and at twelve. The low
  fragmentation heap is faster than the frame it costs.

  What the profile says is next, and none of it is one change:
  `ComputeLeafLayout` (8.3% self), `DetermineFlexBaseSize` (5.7%),
  `GenerateAnonymousFlexItems` (4.3%, half of it the 192-byte `FlexItem`
  built on the stack and then copied into the vector), `RectLpa::
  ResolveOrZero` (3.2%, deliberately out of line). The structural win is
  still carrying layout across frames rather than rebuilding the tree.

  One thing this turned up in `winperf` itself, fixed there rather than
  worked around here: `record -- out/rel/story.exe` failed with
  `CreateProcessW ... err 0x2` — CreateProcessW parses the command line
  itself when given no application name, and its parser does not take `/` as
  a path separator — and then went on to stop xperf, parse the trace and
  print a full summary of the two seconds of everything else the machine had
  been doing. It now translates the path, and a workload that never starts is
  an error rather than a profile of the wrong thing.

- 2026-08-24: **the macOS story now uses half the IOSurface memory.** Xcode
  16.4's Time Profiler, `vmmap`, `heap` and `leaks` were run on an M3 Pro,
  macOS 15.7.7, against release C++ and the pinned release Rust gallery. The
  default AppKit colour space put the C++ window on an RGBA-float16 backing
  store even though `paint.h` exposes only 8-bit sRGB colours. The profile
  showed the consequence twice: `RGBAf16_mark_inner` in the paint samples,
  and 93.1 MB of nonvolatile IOSurface pages. `WindowOpen` now names the
  window's actual `[NSColorSpace sRGBColorSpace]`; AppKit chooses its 8-bit
  SDR backing store and the nonvolatile IOSurface charge is 46.6 MB.

  Total physical footprint fell from 278.8 MB to 186.0-201.6 MB. Two matched
  Rust Spinner runs were 212.2-215.3 MB; C++ is 5-14% lower after the change.
  `heap` reports 6.6-6.8 MB live malloc allocations for C++ versus 13.6 MB for
  Rust. Raw RSS goes the other way (99-122 MB C++, 88-91 MB Rust), because it
  counts shared mappings differently; physical footprint is macOS's charged
  process-memory number and the IOSurface rows name where the change landed.
  `leaks` found no C++ leak, and a 120-frame ASan run is clean.

  There is no frame-time tradeoff. Three 3000-frame introduction runs average
  1.993 ms (build 0.133, layout 0.849, paint 1.007), against 1.991 ms before,
  inside run-to-run noise. On the naturally animated Spinner page, two
  identical 12-second Time Profiler captures average 15.3% of one CPU core
  for C++ and 33.5% for Rust after the first two seconds are discarded: C++
  uses 54% less CPU, or Rust uses 2.19x as much. The heavy C++ introduction
  profile is still split between Core Graphics text/shape drawing and Taffy;
  no second change survived measurement. A fill/stroke-state cache measured
  the same and was removed. One profiling trap: `xctrace record --template
  Allocations --time-limit ...` suspended this command-line app at the limit
  and never finalized the trace; `vmmap`/`heap`/`leaks` against a live PID are
  the reliable built-in command-line path on this Xcode. 17,269 checks pass.

## Nine things the list said were left, and what four of them turned out to be

The list at the top of this file had gone stale in places: three of the nine
were already ported by later sessions and one had been half-ported. What each
one actually came to, in the order they were asked for.

**A no-wrap textarea scrolls sideways, and shows the bar.** Rust builds the
editor's scrollbar from `!soft_wrap` — `Scrollbar::new` for a field that does
not wrap, `Scrollbar::vertical` for one that does — and the port had only the
vertical half, because the sideways offset lived inside the input engine:
`InputState::scrollX` was clamped against a `contentW` nothing ever wrote, and
no element carried it. The offset moves onto the box (`El::ScrollX`), and the
paint pass writes the box's measured content width back onto the state, which
is `scroll_size.width`. Grabbing the bar wanted one more seam: a drag reports
through a Listener bound to an entity and an `InputState` is not one, so
`ScrollRect` names the state the way `El::BindInput` does and the bar writes
the offset straight onto it. The wheel does both axes now.

**Home and End take the wrapped row.** Rust documents the pair as: with soft
wrap on, the first press goes to the visual line's start and a second — with
the caret already there — carries on to the logical line's. It reads the row
off `display_map.line(row).wrapped_lines[..]`; the wrap here belongs to the
shaped run, so the row is found the way the vertical walk already finds one,
by measuring where the caret landed with `TextPointAt` and asking the run what
sits at either end of that row. Gated on `soft_wrap && is_code_editor()`, as
Rust gates it: a plain textarea keeps the logical line. Without a window to
measure against — every field with no frame on screen, and every unit test —
the answer stays the logical line, which is the case the test pins.

**The table story's Amount column was 120 where Rust lands nearer 145**,
because Rust does not size that column at all. `crates/ui/src/table/table.rs`
— the stateless table beside DataTable — had never been ported, and the story
drew its two tables out of raw divs with the widths guessed by hand. It is
`component::Table` now: Table, TableHeader / TableBody / TableFooter,
TableRow, TableHead / TableCell and TableCaption, each themed and padded by
`Size::table_cell_padding`, so the page's Size toolbar sizes both tables where
before it changed nothing on them. The rule that fixes the column: a cell with
no width of its own is `flex_shrink_1()` plus `flex_basis(relative(col_span))`
and `min_w(100 * col_span)`, so every unsized cell starts from the whole row
and they shrink together. `El::BasisFrac` is `flex_basis(relative(f))`, which
the style vocabulary was missing. Two theme tokens came with the footer —
`table.foot.background` and `table.foot.foreground`. Against the Rust story
every column edge now lands where it does upstream; the rows are a DIP
shorter, which is line height and not this.

**The dock, read against `crates/ui/src/dock` rather than against a line
count.** The three gaps the older entry named — the drag preview, the tab
bar's scroll, `InvalidPanel` — were all done in 2026-08-20; seven others were
not. The big one is `PanelStyle::Auto`, the default: a group showing one panel
is a plain 30-DIP title row with no tab chrome at all, and the port always
drew a tab bar, so every single-panel dock read as tabbed. Then
`Panel::visible` (a hidden panel has no tab, is not the active panel, and does
not count towards the last one); the guards on closing and dragging
(`is_last_panel` walking the parent chain, `is_locked` adding the zoomed group
and a group that is the root of its own tree) — which is why each Dock's item
in the story is now a split holding its tab group, exactly as the Rust example
writes it; `PanelControl`, so Zoom In is in the ⋯ menu and the maximise icon
only on a panel that asked for it; the three dock toggles belonging to
particular groups (left and right off the centre's left-most and right-most
top groups, bottom off the bottom Dock's own first group) with Rust's icons,
three of which this tree did not have; a collapsed tab bar showing no active
tab and taking no drag; `tab_name`; the layout version, read and then dropped
on both sides of the round trip; and `set_collapsible(false)` opening the dock
so one shut beforehand cannot be left unreachable.

**The scrollbar's thumb was two DIPs thin.** Measured against the Rust story:
its thumb is eight wide and ours was six. `scrollbar.rs` keeps two widths and
the thin one is only where a fading `Scrolling` bar rests — every other mode
draws the wide one, and any bar grows to it under the pointer or in a drag.
The colour changes under the thumb itself:
`scrollbar.thumb.hover.background`, a token the palette was missing. With it:
the band a press counts in is Rust's WIDTH (16, not 14), the track behind the
thumb is painted from `scrollbar.background`, and the thumb's radius is the
theme's clamped to half the thumb rather than a constant 3.

**Motion was the one that had already been done** — the accordion, the tab
indicator, the dialog and sheet entrances, the skeleton, the spinner and the
indeterminate progress all went through `src/base/motion` in the sessions
after that line was written, and every `with_animation` in `crates/ui` has a
counterpart here. What was left is the one the code admitted to in its own
comment: the dock's drop placeholder, which snapped where Rust tweens it over
150 ms. Four transitions do it, and a transition restarts itself when its
target moves, so switching zones needs nothing; what a transition cannot work
out is where the *first* one starts, which is what `MotionSeed` is for — a
drag arriving at a group seeds it with the dragged tab's preview, and the
placeholder flies out from under the pointer.

**`theme_tokens.rs`**, written down as deliberately not ported because no
component reads it. Two of those clauses were true and the third stopped being
a reason: the token set is also a theme file format, and a theme written in it
could not be read at all. `SemanticThemeTokens` is the set — colours by role,
the radius ladder, the spacing scale, the six text roles, the three shadow
elevations — with `ThemeSemanticTokens` projecting a palette into it and
`ThemeApplySemanticTokens` writing the representable half back.
`ThemeApplySemanticConfigStr` reads a `{"tokens": {...}}` document over the
palette in force. `surface` needed somewhere to come from, and that turned up
a token the palette was missing outright: **popover.background /
popover.foreground**, which the menu, the notification card and the find bar
had been painting as the window's own background.

**`DoubleClickedCell`** has been reachable since cell selection landed; what
was missing was `row_header`, a flag this tree carried and nothing read.
`render_row_header_cell` is the narrow strip a cell-selecting table picks whole
rows with, and without one a single click on the already-selected cell
escalates to the row — never a double click, which is on its way to
`DoubleClickedCell`. `TableEscalatesToRow` is that rule by itself, so a test
can drive it.

**`<text>` in SVG** was ported two sessions ago, badges and all; the line
naming it as missing was older than the fix. The three badges at the top of
the Introduction page read `CI passing`, `docs passing` and `crates.io v0.5.1`
today.

Found and not fixed, written down rather than started: `tooltip.rs` paints a
popover with a border and a shadow where this tree's Tooltip is an inverted
chip — `bg(foreground)` with `text_color(background)` — which is a design
difference rather than a token one and moves every tooltip in the gallery. The
dock has more of its API surface left than its behaviour: panel lifecycle
callbacks (`set_active` / `on_added_to` / `on_removed` / `set_zoomed`), a
panel's own toolbar buttons and menu rows, focus following the active tab,
`DockEvent::DragDrop` for a host-owned drag, and `Panel::title_style` /
`inner_padding`.

17341 checks. The keyboard half of the Home/End change is pinned by tests
rather than by screenshots for the reason the LSP entry gives: a posted
`WM_KEYDOWN` does not reach a field on this machine, so `cmd/shot.ts -key`
cannot drive a caret.

## The editor half of a language server, all seven pieces

`crates/base/src/input/editor/lsp` is eight files and 1714 lines. Five of its
seven provider slots were here — completion, code actions, hover, document
colours, and the diagnostics that live next door — as function pointers where
Rust has traits answering `Task`s. What follows is the rest of it, and the
standing constraint first: **there is still no transport.** No JSON-RPC, no
child process over stdio, no `lsp_types`. An LSP client is one of the named
exclusions in AGENTS.md, and it would want process spawning and real async,
neither of which this tree has. Everything below is the *editor* side — what
an application that has a language server by other means can plug into.

**Go to definition** (`definitions.rs`). A `DefinitionFn` answers
`DefinitionLink`s — a flattened `LocationLink`. With the shortcut modifier
down, the pointer asks where the symbol under it is defined rather than what
it is (`handle_mouse_move` picks one of the two and never both), the answer
is underlined in the link colour, the symbol takes the hand cursor, and a
secondary-click follows it and keeps the press so the caret does not also
move. The `GoToDefinition` action goes by the *last* thing a hover found,
since the underline is gone by the time a menu row is picked.
`window/showDocument` is the host's first refusal; after it an `http(s)`
target goes to the browser and anything else moves the selection inside this
document. Two things came with it: `Window::mouseModifiers`, because a hover
is worked out by the frame builder here and it has no event to read them off,
and `El::RangeOut`, which reports where a named run of a text element was
painted — Rust inserts a hitbox over exactly that.

**Range semantic tokens** (`semantic_tokens.rs`). The provider answers tokens
*delta-encoded as a server sends them*, which is the point: the decoding is
the editor's. `SemanticTokensDecode` unpacks the deltas, skipping a type
outside the legend; `SemanticTokensForRange` binary-searches the window
touching the viewport out of a cache kept in document order. What is cached
is the type *name*, resolved to a colour at paint, so a theme change recolours
with nothing refetched. The names map onto this tree's scanner palette rather
than a tree-sitter capture vocabulary, with `registry.rs`'s own dotted
fallback. One deliberate difference: Rust composes the two layers with
`combine_highlights`, which folds the overlapping styles out of a `HashSet` —
so which wins where both speak is undefined there. Here the server's token
wins over the scanner's capture, which is what the protocol means by layering
semantic tokens over a lexer.

**Inline completion** (ghost text). The provider is asked once the typing has
stopped for 300 ms — a `dueAt` on the state and a frame asked for while it
runs, with the same two checks Rust makes on the far side of its timer: the
caret has not moved and no menu has opened. Tab accepts it before it indents,
escape declines it and consumes the key, Enter and any press drop it. Rust
makes room for a multi-line suggestion by shifting the rows below down; the
rows here are a virtualized flex column whose heights the layout owns, so
every line is drawn over what is under it, each on the editor's own surface.

**The rest of the completion surface.** `is_completion_trigger` — the
provider decides whether what was typed opens, carries or closes the menu,
with the old hardcoded rule underneath it; `completionItem/resolve`, asked
once about the item the selection is on when it arrived without
documentation, the answer written back into the item; and
`CompletionMenuOptions::max_width` — 320 rather than 420, clamped to what is
left of the window, with Rust's fallback layout when the list and the pane
beside it will not both fit (the documentation goes underneath, trimmed to
its first line).

**`apply_lsp_edits`.** `TextEditItem` is `lsp_types::TextEdit` in byte
offsets, a code action carries an edit list where it has one, and a completion
item carries `additionalTextEdits` — the import a name brings with it. Each
edit is its own undo step, which is what Rust's loop over
`replace_text_in_range_silent` records; written down rather than improved on.

**The overlay seam** (`overlay.rs`). Rust keeps only the menus' *state* in the
editor and hands the drawing to the host. This tree draws them itself and
still does; beside that there is now `InputPresentCompletionItems` /
`InputPresentCodeActions` / `InputPresentHover` / `InputPresentDiagnostic`, a
**revision** on each menu for a host renderer to diff against, an
`OverlayActionFn` asked before the editor's own menu handling — which
`InputPerform` now goes through — and `InputInsertCompletion`, with
`insert_text` going in *after* the query's range rather than over it. One real
fix came out of it: `completion_inserting` / `silent_replace_text`, so an edit
the editor makes on the reader's behalf is not treated as typing.

**The three small ones.** The 150 ms before a hover is asked for, and only
when nothing is showing (`should_delay = hover_popover.is_none()`); every
registered code action provider asked with its answers concatenated, each item
remembering which one it came from so a `perform` goes back to that provider;
and `Lsp::update` / `Lsp::reset` as one call each.

The editor example carries a provider for every one of them now — the Rust
example's own definitions (`Duration` in the document, four std names on
doc.rust-lang.org), `MarkerHighlighter` from the Rust markdown example for the
semantic tokens, a loop body after `for (`, items sent thin with their
documentation behind `resolve`, a two-edit "Wrap in Parentheses", and an item
that brings a `use` line with it.

17455 checks, of which nineteen are new — the four in Rust's own
`semantic_tokens.rs` tests among them. The keyboard half is pinned by tests
rather than by screenshots, for the reason the last LSP entry gives: a posted
`WM_KEYDOWN` does not reach a field on this machine.

## Five things a list said were still wrong, and what two of them turned out
## to be

Five items came in as one list. Two were already done and are recorded here so
the next reader does not go looking again; three were real.

**Soft wrap and the sideways bar were both already here.** The editor has
`softWrap` on `InputState`, the editor and large-text examples both carry the
toggle button their Rust originals do, and `Textarea`'s no-wrap horizontal
scrollbar was built two sessions ago — `El::ScrollX`, the measured content
width written back onto the state, and a bar that drags through `ScrollRect`.
Nothing to do.

**A multi-word link is underlined once.** A styled paragraph is a row of word
elements — that is what lets the row wrap where Rust wraps one `StyledText` —
and each word carried its own trailing space and its own `text_decoration`. A
shaper does not draw an underline under trailing whitespace, so `[link to the
port](..)` came out underlined word by word with a gap at every space. Rust
does not ask a shaper: `node.rs` hands the run an `UnderlineStyle { thickness:
px(1.) }` and GPUI draws a quad under the whole run. `TextView::Word` puts the
rule on as a `TextSpan` over the word's own bytes now, which is that quad —
`PaintTextUnderline` measures the run *including* its trailing space, so the
rules of two neighbouring words abut and read as one line. The colour goes on
with it: a span with no alpha is not drawn at all, so a run that named no
colour takes the theme's foreground, which is the colour the glyphs will take.
The link colour is `theme.link` rather than `theme.primary`, which is what
`node.rs` reads (the token falls back to primary, so nothing moved).
`~~struck out~~` still breaks at the space for the same reason and is not
fixed: a strikethrough has no span of its own to hang on, and the height a
shaper puts it at is a font metric this tree does not read back, so inventing
one would move every strikethrough in the tree to close a gap in one.

**The resizable story's panels start where Rust's do.** Two halves. The
declarations had drifted — the nested group's top row was a panel of 264 where
Rust hands the inner group to the outer one as a plain child (a panel with no
size of its own), the bottom panel had a 150 ceiling where `size_range` says
`Pixels::MAX`, the growing panel had lost its `200..400`, and the programmatic
group's centre was a fixed 300 where Rust grows it. Those are what
`resizable_story.rs` says now, `.visible()` on the panel the Hide Left button
toggles included.

The other half is what a panel's *first* size is. Rust's panel is a flex item:
`size_full`, `flex_grow: 1` unless the caller says `flex_none()`, the
`size_range` as the min and the max, and the declared size as the flex basis —
so a group of sized panels in a container narrower than their sum **shrinks**
them, and a panel with no size of its own starts from `width: 100%` and is the
one that gives way. `update_panel_size` then writes what the layout measured
back into the state, and the state is what every later frame and every drag
reads. The port had been doing arithmetic instead: the declared numbers, with
the leftover handed to the one growing panel. It declares the flex item now
and reads the measured size back on the next frame, which is the same two
steps in the same order — `Resizable::Flex()` is the panel Rust did not call
`flex_none()` on, `Visible(false)` is a slot that keeps its size and draws
nothing, and the drag arithmetic compacts the hidden slots out before it runs
rather than letting their numbers drift the way upstream's do. On the nested
group at the compare window's width Rust lands on 121 / 408 / 148 and this
lands on 120 / 389 / 170, where before it was 150 / 115 / 415: the left panel
is at the minimum both sides agree on, and the remaining ~20 DIPs are a
percentage basis resolving against something slightly wider on Rust's side,
which is a taffy question rather than a resizable one.

**The theme viewer lists `ThemeColor`, not a subset of it.** The page's table
was 78 rows picked by hand out of the tokens this tree's `Theme` happened to
carry. Rust's is every field of `ThemeColor` — 139 — serialized and split into
a category and a name by `mapper.rs`. The palette is that field set now:
`Theme` and `ThemeTokens` carry the four button families, the hover and active
halves of danger / success / info / warning, accordion, the drop target, the
link trio, the list surfaces, the slider and switch backgrounds, the tab and
segmented tab-bar surfaces, table hover, tiles and the window border, and
`ThemeFillDerived` in `gpui.cpp` is where every one of their fallbacks lives —
the `fallback =` of the matching `apply_color!`, in schema.rs's order — so a
palette written in code and a theme file that names nothing land on the same
numbers. `ThemeConfigResolve` calls it and then lets the file's own words
stand over the top, which is what the new keys are read as. The story's table
is generated out of `theme_color.rs` and `mapper.rs` rather than typed.

Two things on that page that had been ornaments work now. **Inherited Colors**
filters: a row is shown only where the theme file names that token itself,
which is Rust's `is_explicit`, and the menu item lifts the filter. Rust
decides it by comparing `mapper.rs`'s canonical key against the key
`ThemeConfigColors` serializes, and those two names only sometimes agree — a
token like `accordion` (`accordion.background` in the file) or `drag_border`
(`drag.border`) can never match and reads as inherited however the file is
written. That wart is kept rather than improved on, because with it the page
opens on the same list on both sides: Global, Primary, Secondary, Accent,
Base, Chart, Danger, Info, Input, List, Muted, Sidebar. And the **search
field** filters, on the category, the name, or the start of the hex with a
leading `#` taken off it first.

What this does *not* do is move the components onto the new tokens. A Button
still paints the expressions it was written with rather than
`theme.button_primary_hover`, and the same goes for the tab, the switch, the
slider bar and the list and table hovers. The token layer is the half that had
to come first — a theme file can name any of them now and the viewer shows
what it named — and the components are a page at a time behind it.

**A float becomes a byte by truncating.** `Colorize::to_hex` is
`(rgb.r * 255.) as u32`, and every other place Rust turns one of its float
colours into a byte truncates the same way. This tree holds bytes, so the
quantisation happens at every step of a fallback chain rather than once at the
end, and it was rounding: half the derived tokens came out one above the
number Rust prints for the same colour. `RgbaOpacity`, `RgbaHsla`, the blend,
the Oklab mix and the two alpha caps truncate now, `red-500/50` is 127 rather
than 128, and the five hardcoded palette constants that drifted from the file
by one — the two selections at their 30% cap, the two description-list labels
and the dark input background — are the truncated numbers, which is what the
`theme drift` check compares. The hex printers go through `RgbaToHex`, which
is `to_hex` in full: upstream holds an `Hsla` and converts back to bytes to
print it, so a colour that arrived as a hex prints one below itself wherever
the conversion does not land on a byte boundary, and the round trip is made
here for the same reason. It is not all the way there: the colour picker's
`#6366f1` still prints as itself where Rust prints `#6366f0`, because this
tree's HSL conversion is not bit-identical to GPUI's and the truncation falls
on the other side of the boundary for that colour. The rule is right; the last
bit of one channel is not, and closing it wants GPUI's own `Rgba`↔`Hsla`
rather than a textbook one.

17,455 checks pass. The pages this touched were shot against the Rust story
side by side (`bun cmd/compare-story.ts resizable`, `theme-colors`,
`color-picker`), and the theme viewer's default list, its inherited list and
its hex readouts line up with upstream's.

- 2026-08-24: **four files moved to the name the Rust has.** A sweep of every
  `src/base`, `src/ui` and `src/gpui` file against the module its header
  comment names turned up four places where the path had drifted from the
  crate the code came from, and they are now the Rust's:

  | was | is | Rust |
  | --- | --- | --- |
  | `src/gpui/positioner.{h,cpp}` | `src/base/positioner.{h,cpp}` | `crates/base/src/positioner.rs` |
  | `src/ui/history.{h,cpp}` | `src/base/history.{h,cpp}` | `crates/base/src/history.rs` |
  | `src/ui/theme_registry.{h,cpp}` | `src/ui/theme.{h,cpp}` | `crates/ui/src/theme/` |
  | `src/gpui/fps.{h,cpp}` | `src/fps/fps.{h,cpp}` | `crates/fps/` |

  `positioner` is a gpui-base module, not a runtime one — `gpui.cpp` calls it
  for the built-in tooltip and now includes `base/positioner.h`, which is the
  third gpui→base include and no new kind of edge. `history` is upstream's
  `crates/base/src/history.rs`; `crates/ui/src/history.rs` is a one-line
  re-export of it, so the file that had been `component::History` in `src/ui`
  is plain `gpui::History` in `src/base`, which is what a `crates/base` type is
  here. `theme_registry` was one file for what upstream keeps in a `theme/`
  directory, and `theme.h` is the name that directory has. `fps` is a crate of
  its own — `crates/fps`, which leans on gpui the way `crates/base` does — so
  it gets a directory of its own, the way `src/taffy` and `src/markdown` do.
  Nothing else moved: `src/base/{list,tiles,dock,dock_state,data_table,
  popup_menu,sankey}` hold the unstyled halves of `crates/ui` modules on
  purpose, and `src/taffy` and `src/markdown` map through the tables in their
  readmes.

  `src/base/lib.h` had drifted out of alphabetical order in five places; it is
  sorted again, with the two new headers in it. `src/ui/sizing.h` now names
  `crates/ui/src/sizing.rs`, which it always was.

  Still divergent, and not fixed here: `crates/base`'s `theme.rs`,
  `theme_tokens.rs`, `geometry.rs`, `measure.rs`, `text_boundary.rs`,
  `event.rs`, `styled.rs` and `global_state.rs` have no file of their own —
  they are folded into `src/gpui/gpui.h` and `gpui.cpp`, where the runtime
  they extend lives. Splitting them back out is surgery on a header the whole
  tree includes, and it wants its own session.

  17,455 checks pass and `bun cmd/build.ts -all` builds all 24 examples.

- 2026-08-24: **two CI failures that the Windows and Linux jobs could not
  see.** `bun cmd/test.ts` was green on both and red on macOS and wasm, for
  two unrelated reasons.

  macOS: `theme drift Default Dark.inputBg: file 2f2f2f4c, hardcoded
  2e2f2e4c`. `Theme::input_background()` on a dark theme is
  `mix_oklab(input.border, transparent, 0.3)`, and mixing toward transparent
  is defined to keep the colour — the transparent side is premultiplied by an
  alpha of zero, so it contributes nothing and the Oklab channels are the
  other side's exactly. `RgbaMixOklab` was computing that identity the long
  way, `l1 * aa * factor / alpha` and back through `cbrtf` and `powf`, which
  is `l1` in real arithmetic and `l1` ± an ulp in f32. Rust keeps its channels
  in f32 and never notices; an `Rgba` here is eight bits a channel, so the ulp
  landed a channel on the far side of a rounding boundary — on a platform's
  libm, not on the colour, which is why three jobs agreed on `2e2f2e` and the
  fourth said `2f2f2e`. Both were wrong: #2f2f2f is what the mix means. The
  two fully-transparent cases now return the other side's bytes and the
  interpolated alpha, and the hardcoded `ThemeDefaultDark().inputBg` is
  `2f2f2f4c`. Every other `mix_oklab(x, transparent, f)` in the resolver — the
  input backgrounds, the alert and callout tints off `success`, `info` and
  `warning` — is exact for the same reason now.

  wasm: `tests/InputStateTests.cpp:535: variable 'gCompleteCalls' set but not
  used`. Emscripten's clang has `-Wunused-but-set-global`, which MSVC, g++ and
  Apple clang do not, and `-Werror` made it fatal. The counter was not dead
  code that wanted deleting — it was an assertion nobody had written. It now
  pins what it was counting: the completion provider is asked once per
  keystroke that *opens* the menu and not at all for one that closes it, which
  is two calls across the four characters that test types.

  17,459 checks pass.

- 2026-08-24: **three gpui-base modules out of `gpui.h` and into files of
  their own.** The previous entry listed eight `crates/base` modules with no
  file in `src/base`, folded into `src/gpui/gpui.h` and `gpui.cpp` instead.
  Going through them one at a time, three of the eight were real blocks that
  had been put in the runtime header because the type they read lives there,
  and they are now where the Rust has them:

  | new | Rust | what moved |
  | --- | --- | --- |
  | `src/base/geometry.h` | `crates/base/src/geometry.rs` | `Placement` (out of `positioner.h`), `Side`, `SideIsLeft`, `AxisIsHorizontal` |
  | `src/base/theme_tokens.{h,cpp}` | `crates/base/src/theme_tokens.rs` | the whole `Semantic*` set, `kMonoFontSize`, `SemanticShadowElevations`, `ThemeSemanticTokens`, `ThemeApplySemanticTokens` |
  | `src/base/text_boundary.{h,cpp}` | `crates/base/src/text_boundary.rs` | `CharKindOf`, `Utf8ClipLeft`, `TextWordRangeAt`, `TextLineRangeAt` |

  None of it needed a new kind of edge. A `src/base` header includes
  `gpui/gpui.h` the way all forty-odd of them already do, which is the same
  direction `gpui_base` depends on `gpui` in Rust; `gpui.cpp` picks up
  `base/text_boundary.h` for `TextMultiClickRangeIn`, its fourth
  `gpui`→`base` include. `gpui.h` is 126 lines shorter and `gpui.cpp` 199.

  What stayed, and why it is not drift:

  - **`theme.rs`** is a three-field defaults holder — `SemanticThemeTokens`,
    a scrollbar mode and style, a resizable handle colour — hung on `App` as
    a Global. Its tokens now have their own file; `ScrollbarMode` is in
    `gpui.h` because the runtime's own scroll areas carry one; nothing here
    has a `ResizableTheme`. A `base/theme.h` holding the container would be a
    struct invented for the file name, so there isn't one.
  - **`styled.rs`** is `StyledExt`, a trait on `gpui::Styled`. Rust needs a
    module to hang `h_flex` and `v_flex` on another crate's type; here they
    are `El::Flex()` / `FlexRow()` / `FlexCol()`, methods on the class
    itself, which is in `gpui.h` because `El` is. Only `FOCUS_RING_WIDTH` and
    `FOCUS_RING_OPACITY` are free-standing, and `gpui.cpp`'s own painting is
    what reads them.
  - **`event.rs`** is two extension traits. `on_double_click` is
    `ClickEvent::clickCount` here, a field on gpui's own event struct.
    `lock_scroll_axis` works around `std::time::Instant` panicking on wasm32
    inside Zed's `OngoingScroll`; this runtime has no such call and nothing
    to switch off.
  - **`measure.rs`** and **`global_state.rs`** are not folded into `gpui.h` —
    they are not ported at all, which the previous entry got wrong.
    `measure.rs` is a scoped timer that logs through `tracing` behind
    `ZED_MEASUREMENTS`; `global_state.rs` is app-wide state with three
    members — the app menu list, the set of open deferred popovers, and a
    one-mouse-down text-selection suppression. The last two are behaviour
    with call sites, not a file move, so they are a port of their own rather
    than part of this one.

  17,459 checks pass and `bun cmd/build.ts -all` builds all 24 examples.

- 2026-08-24: **five reported dialog and textarea divergences, checked one at
  a time against the Rust story running side by side.** Two of them were
  already fixed; three were not, and one of those was a component bug rather
  than a story one.

  **The dialog's controls all shared one id.** `dialog-close-x`,
  `dialog-backdrop`, `dialog-cancel` and `dialog-ok` were constant strings, so
  every open dialog hashed to the same `clickId`. A hover is one number —
  `e->style.hasHoverBg && e->clickId == ctx->hoverId` — so pointing at the top
  dialog's close x lit the one behind it as well, and the two backdrops were
  one click target. GPUI does not have this problem because an `ElementId` is
  scoped by its ancestors' and the panel is `.id(layer_ix)`; `Dialog::LayerId`
  is that, appending the layer to each. With it, the story's two-dialog
  section closes only the top dialog when its backdrop is clicked.

  **No dialog animated, on this machine.** `MotionAppear` — the port of
  GPUI's `with_animation` — checked `MotionReduced()`, and
  `SPI_GETCLIENTAREAANIMATION` is false on any desktop with animation effects
  turned off, which is not rare. Rust has exactly one `cx.reduce_motion()` in
  the crate and it is inside `motion::transition`; every `with_animation` —
  the dialog's slide-down and fade, the sheet's slide — plays regardless. The
  check is gone from `MotionAppear` and stays in `MotionTransition`, where
  Rust has it. Caught by stretching `kDialogMotionMs` to 20 s and shooting
  200 ms in: before, the panel was already in place and fully opaque; after,
  it is high, faint and on its way down.

  **The scrollable dialog's bar and its footer.** The story built the whole
  surface with one `Pad(16)`, so the scroll box stopped 16px short of the
  panel and its bar sat there too. Rust pads the title, the scroll box's
  *contents* and the footer separately (`pl(paddings.left)` three times in
  `dialog.rs`), so the box itself reaches the panel edge. The footer's two
  buttons were compact and right-aligned where Rust wraps each in
  `DialogClose` / `DialogAction`, which is a `size_full` box — the pair share
  the row half and half, which is what `render_custom_buttons` already did
  here and this section did not. Both edges now land where upstream's do.

  **The table dialog showed 13 rows where Rust shows 15.** Rust hands the
  dialog a bare `DataTable::new(&table)` and the body gives it the rest of the
  panel; the story had `H(430)`, guessed. `DataTable::H` cannot be a fill —
  it is what decides how many rows are *built*, and a virtualized list cannot
  ask the layout what height it got — so the story spells out what is above it
  and takes the remainder of the 600, which is 476. Rows 0..14, as upstream.

  Already fixed, and verified rather than redone: a no-wrap `Textarea` gets
  its horizontal bar from `!softWrap → box->ScrollX(state->scrollX)` in
  `ui/input.cpp`, and the table story's Amount column comes from
  `component::Table`'s `BasisFrac`.

  Not touched, and worth a session of its own: every section of the dialog
  story builds a `Surface()` by hand where Rust passes `.title()`, `.child()`
  and `.footer()` and lets the component lay them out. That is why these three
  drifted independently, and why `Dialog::IntoEl` has no equivalent of Rust's
  `flex_1 / overflow_hidden / overflow_y_scrollbar` body wrapper.

  17,459 checks pass; `bun cmd/build.ts -all` builds all 24 examples.


## Selection: Plain / Source, and the four items already standing

Five things off a list, and four of them were already here: the three markdown
plugins (ticker, user card, math — the last on its own fallback path, since
KaTeX wants node), the `Table: Scroll / Wrap` toggle over `TextView::
TableScroll`, and the `Open...` action, which has had the desktop's own dialog
since `PromptForPath` landed. The fifth was real, and the reason recorded for
it was wrong.

**`Selection: Plain / Source` does not need source offsets.** The note in
`examples/markdown.cpp` said it would have to map a selection in the rendered
document back to the markdown that produced it, and that an `MdNode` carries
no offsets. Upstream does not map anything: `node.rs` *reconstructs* the
markdown from the `BlockNode` tree it rendered — `text_by_kind` with
`BlockTextKind::SelectedSource`, `reconstruct_markdown` over the mark ranges,
`list_selected_source`, `table_selected_source`. Nothing in that needs the
source text at all, so it ports.

What it ports onto is different, though, and that is the whole design. Rust's
document does its own copying, so the walk happens at copy time with the tree
in hand. Selection here is the *window's* — `base/text_selection.h`, a flat
list of painted `TextHit`s in document order — and it has no tree. So the walk
happens as the tree is built, and each run carries the piece of the
reconstruction it is responsible for:

- `SelSource` is `wrap_with_mark`'s two halves, in that function's nesting —
  code innermost, then italic, bold, strikethrough, underline, highlight, link
  outermost. One record is *shared* by every adjacent run with the same marks,
  and the copier closes a group only when the record changes. That is what
  `reconstruct_markdown` gets from walking mark ranges rather than words: a
  bold phrase split into three word elements has to copy as `**one two three**`
  and not as three separately wrapped words.
- `SelBlock` is what opens and closes a block — a heading's `#`s, a fence and
  its language, a table row's pipes and the alignment row behind the header —
  plus the `linePre` every further line of it carries, which is how a
  blockquote's `> ` reaches lines inside a run that holds its own breaks.
  Block identity is also what tells the copier a line has ended, and
  `SelBlock::join` is what says a table cell continues the row instead.
- `TextHit::join` is per-run and holds in **both** formats. A paragraph is one
  `InlineState.text` in Rust however it is copied; here it is a row of word
  elements, and without this a copy of a styled paragraph came out one word
  per line. That was wrong before this change and is the one behaviour it
  fixes rather than adds.

`TextView` accumulates the walk state — `srcLinePre`, the pending item marker,
the open block and whether the next run starts a line — and `SrcOpen` /
`SrcMark` / `SrcCell` / `SrcBreak` are where each block hands its runs their
piece. `CopyTextHitsIn` takes a `SelectionFormat` and puts the pieces back
together; `WindowSelection` carries the format, which `TextView::SelFormat`
pushes onto the window as it renders, since the copy is the window's.

The gap rule in the copier is worth naming, because it survived a rewrite: the
separator between two runs comes from whether the selection ran through the
*gap* between them, not from whether anything has been emitted yet. A drag
that ends exactly at the start of the next run has reached into it, and
`ADragAcrossTwoRunsCopiesBoth` is the test that says so.

Verified end to end rather than only in the unit tests: a fixture with a
heading, a paragraph mixing all five marks and a link, a two-line blockquote,
a bulleted list with a nested item, an ordered list, a fenced `rust` block and
an aligned table, selected whole and copied in both formats. Source comes back
as the markdown that produced it, alignment row and all; Plain comes back as
the rendered text, one block per line, with a row's cells joined by a space —
which is what `text_by_kind(All)` does.

Not carried over at the time, and both because the fold dropped them before
the renderer saw them: a task list's `[x]` (`MdNode` had no `checked`, so the
checkbox was not drawn either) and an inline image's `![alt](src)`, which is
an `Image` element and so registered no run to hang an affix on. Both are
carried now — the section below.

The one other thing this touched: the story gallery's app menu said `Open...`
was left out because the tree had no file dialog. It has had one for a while.
The row is there now, and the comment says what upstream's gallery handler
actually is — empty, because a component gallery has nothing to open a file
into. `on_action_open` in the markdown example is the one that does the work.

17,472 checks pass; `bun cmd/build.ts -rel -all` builds all 24 examples.


## The checkbox and the picture the fold used to drop

The two things the selection work left behind, and neither of them was really
about selection.

**`MdNode` now has the task list's `Option<bool>`.** mdast has carried it all
along — `NodeChecked` / `NodeHasChecked`, and `to_mdast` even takes the `[x] `
off the front of the item's first paragraph — and the fold in `text.cpp` was
simply not reading it. `MdNode::hasCheck` / `checked` is that pair, the way
`markdown.rs` carries mdast's `checked` onto the `BlockNode`. What it costs is
two bools on a node that was already padded.

Drawn where `render_list_item_row` draws it: a 14-DIP box (`rems(0.875)`) with
the primary colour as its border, filled with a `size_2` tick when the item is
ticked, and standing *instead of* the bullet or the number — `when(!todo &&
checked.is_none())` is what draws a prefix at all. The two margins Rust gives
it are padding on the cell that holds it, since there are no margins in this
tree. `options.todo` comes with it: everything under a task item is rendered
with `todo: checked.is_some()`, which is what takes the bullets off a plain
list nested inside a todo. That reads as an upstream quirk and it is one, but
it is the shape the row draws in, and a nested list under a checkbox does look
like part of it.

In `SelectionFormat::Source` the checkbox goes where `list_selected_source`
puts it: after the marker, on the first line only. The indent under the item
is the marker's width and not the marker plus the box (`" ".repeat(marker.
len())`), so `srcItemPad` is now what the item indents by and `srcItemMarker`
is what its first line carries. An ordered task item still spends its number.

**An inline image registers a run of its own.** This is the part that needed a
seam rather than a field. `node.rs` gives an `ImageNode` no selection —
`Paragraph::text` lays the children's text end to end and an image child has
none — and `selected_source` emits its `![alt](url)` when the selection *runs
into* it: the run before it ends at its end, the run after it starts at its
beginning. The window's selection here is a flat list of painted runs, so the
image takes a place in that list: `TextHit::atom`, a run with no text at all,
holding one offset of document order and carrying the whole `![alt](url)` as
its `SelSource::pre`. `Source` emits it when the selection has run past the
place it sits in; `Plain` skips it, which is the empty string Rust's `text()`
gets from an image child.

`AtomReached` is that rule, and it is Rust's three clauses rather than the
one it started as. The run before the image has to be selected *and* selected
to its end, which is `selected.emitted && selected.at_end` — so a drag that
stops at the last word before a trailing picture takes the picture, and one
that stops a character short does not. A paragraph that begins with an image
has no run on that side, and Rust counts that as reaching it, so selecting the
sentence after a leading picture takes it: what says so there is the run
*after*, selected and from its beginning (`at_start` is what flushes the
images the walk has queued). And a paragraph that is nothing but a picture
emits nothing of its own in Rust — its `source` is empty — so the enclosing
document walk is what takes it, which here is the selection having run past
the place it sits in.

The one thing still left: Rust writes the image's title after the url when it
has one, and `MdRun` keeps the url and the alt, which is what the parse fold
kept.

Checked with the unit tests — the fold reading `- [x] `, the copier over a
hand-built task list, and a picture in the middle of a paragraph, at either
end of one, and as a paragraph of its own — and with the markdown example
rendering a fixture of ticked, unticked, nested and numbered items beside an
inline badge. What is not driven end to end is the clipboard itself for these
two, since there is no headless input path; the marker and affix strings are
covered at the unit level only.

17,542 checks pass; `bun cmd/build.ts -rel -all` builds all 24 examples.


## gpui::Hsla, and the four places that were doing without it

The tree had the two conversions and no type: `RgbaHsla` took four floats and
`RgbaToHsla` handed three back through out-params, and everything that wanted
to work in HSL declared `float h, s, l` and passed their addresses. So the
conversions were there and the *shape* was not, and the places that should
have gone through HSL went through the bytes instead.

**The pair now is `color.rs`'s, clause for clause.** `Hsla` is gpui's four
floats; `HslaFromRgba` is `impl From<Rgba> for Hsla` — including the
`l == 0. || l == 1.` arm that reports no saturation rather than dividing by
nothing, and the `rem_euclid` on the red arm that brings a negative hue back
round; `HslaToRgba` is `impl From<Hsla> for Rgba`, with the branch on
`(h * 6.).floor()` (the `0 | 6` arm catches a hue of exactly 1) and the clamp
where Rust puts it — on the three channels coming *out*, not on the h/s/l
going in. The old entry point clamped its inputs and wrapped the hue instead;
`gpui::hsla()` is the one place Rust clamps, and it clamps rather than wraps,
so `HslaNew` is that and `RgbaHsla(h, s, l, a)` is now the two together.

**What it is used for is the point.** Three things this tree does in bytes are
things Rust does in HSL:

- `Colorize::mix` — the hue takes the *shorter* arc round the circle and the
  other three interpolate straight. `RgbaMixHsl` is that, `lerp_hue` and all,
  and it is a different function from `RgbaMix`, which is the plain channel
  blend `default_title_bar_background` writes out by hand. The plot's dashed
  crossline wanted the first (`border.mix(foreground, 0.8)`, tooltip.rs) and
  was painting the plain border.
- `impl Lerp for Hsla` — the four channels straight, hue included. The colour
  Lerp interpolated bytes, which is a different path through the middle for
  any two colours that are not near grey; it walks in HSL now. The ends are
  handed back as they came in rather than through the conversion, and that is
  deliberate: eight bits a channel cannot promise a round trip lands on the
  byte it started from, and a transition that settled a byte off its target
  would stay there.
- `Colorize::darken` — and this one was a real bug. The colour picker's three
  borders (a swatch, the trigger's square, the hovered row) are `darken(0.1)`,
  `darken(0.3)` and `darken(0.2)` upstream; here they were `RgbaMix(c, black,
  0.1)` and friends, which is not a tenth darker but a *tenth of the colour* —
  a border nine tenths of the way to black. One of them even had `darken(0.3)`
  written in the comment above it. They call `RgbaDarken` now, which has been
  the port of `Colorize::darken` all along.

The quantisation is worth naming, since the type makes it easy to chain: Rust
holds four floats from the theme file to the GPU and rounds once, at the end;
every step here goes back through eight bits a channel. A colour that goes out
through HSL and back comes back as itself or one byte under it — a grey always
exactly, a saturated colour sometimes a step low, because the float that
returns is a hair under the one that left and `ToByte` truncates. That is why
`to_hex` looks like it drifts and does not: `Colorize::to_hex` is written on
an Hsla, so the round trip is what it does.

`tests/ColorTests.cpp` is new and holds color.rs's own cases — `test_mix`'s
three hexes, `test_lighten` and `test_darken`'s factors, each sixth of the
hue circle, the clamps either side, and the round trip's one-byte bound over
the palette and all 256 greys. Where Rust asserts a float that a colour here
was quantised into on the way in, the assertion is the operation against the
byte's own value and the comment says so.

17,901 checks pass; `bun cmd/build.ts -rel -all` builds all 24 examples. The
colour picker's swatch and trigger borders were checked on the story page: a
darker shade of the colour, where they were nearly black.

## The webview, and the two crates Windows needed that we could not have

`crates/webview` — the `gpui-wry` crate — was the last of gpui-component's
crates with nothing here at all, and the reason was a line in the non-goals
list: "a webview" sat among the third-party C++ libraries hard rule 3 rules
out. That reading was wrong by one level. A webview on Windows is WebView2,
which ships with the OS's Edge; what wry needs from crates is not the browser
but the *bindings* to it — `webview2-com`, which generates the COM interfaces
from Microsoft's SDK, and `webview2-com-sys`, which links Microsoft's
`WebView2LoaderStatic.lib`. Both of those are the kind of thing this tree
writes out rather than vendors, the same call `src/sys/http.h` makes about an
HTTP client. So the line is gone and there are two new directories:

- **`src/wry/`** is a port of [wry](https://github.com/tauri-apps/wry) 0.53.3
  — the `lb-wry` fork `crates/webview/Cargo.toml` pins — and is isolated the
  way `src/taffy` and `src/markdown` are: `base.h`, its own header, no
  `gpui::` name anywhere, and `cmd/build-dist.ts` fails the build if that
  stops being true. The parent window arrives as a `void*`, which is Rust's
  `raw_window_handle`.
- **`src/webview/`** is the port of `crates/webview` itself: the view that
  gives a wry webview a box in the element tree and keeps the OS control on
  top of it.

### The two halves that had to be written out

**The ABI.** `wry_win.cpp` opens with a block of `ICoreWebView2*`
declarations transcribed from the WebView2 SDK header — same vtable order,
same IIDs, the MIDL comments dropped. It declares an ABI the way `<d2d1.h>`
does, and nothing of the SDK is compiled in. It was generated mechanically
rather than typed, which is the only way 44 interfaces come out right: an
interface that only appears in a signature we never call is forward-declared,
and the ones we implement — `ICoreWebView2EnvironmentOptions` and its 6 and 8,
every event handler, every completion handler — are there in full, because an
implementation has to answer for every slot. `ICoreWebView2` alone is 59
methods before its 21 versioned successors add theirs, and each of those
successors has to be declared to reach the one method on the last of them.

**The loader.** `CreateEnvironmentWithOptions` is about a hundred lines doing
what `WebView2Loader` does: read `WEBVIEW2_BROWSER_EXECUTABLE_FOLDER` or the
EdgeUpdate registry keys (`SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-…}`
— `pv` and `location`, per-machine through the WOW64 view and then per-user),
`LoadLibrary` the runtime's `EBWebView\<arch>\EmbeddedBrowserWebView.dll`, and
call the `CreateWebViewEnvironmentWithOptionsInternal` export it has always
had. The argument list is not guessed: `WebView2LoaderStatic.lib` is on this
machine, and `lib -extract` plus `dumpbin -disasm` of its `instance_shared.obj`
shows `CreateWebViewEnvironmentWithClientDll` forwarding everything but the
dll path, with its own mangled name spelling out the types —
`(wchar_t const*, bool, WebView2RunTimeType, wchar_t const*, IUnknown*,
ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*)`.

Two values the SDK's helper class supplies and the runtime will not do
without, both found the same way — by getting `0x80070002` until they were
right:

- a **user data folder**. Null is not "pick one for me"; it is
  ERROR_FILE_NOT_FOUND. The default is the exe's own path with `.WebView2`
  after it, which is what the loader passes.
- **`TargetCompatibleBrowserVersion`**, which is a *browser* version
  (`149.0.4022.49`, the SDK's `CORE_WEBVIEW_TARGET_PRODUCT_VERSION`) and not
  the SDK package's own version, which is the thing it looks like.

### What is ported, and what is not

The whole portable API of `lib.rs` and the WebView2 backend under it:
attributes and their defaults, a webview built into a window or as a child of
one, bounds / visibility / focus, `evaluate_script` with and without a
callback, url and html loading (with headers), reload, zoom, background
colour, theme, memory usage level, reparent, print, clear-all-browsing-data,
devtools, `webview_version`, the IPC channel, initialization scripts, custom
protocols with their `http://<scheme>.host` work-around and their
asynchronous responder, the navigation / page-load / document-title /
new-window handlers, the clipboard permission, incognito, the proxy switches,
and the Windows builder extensions.

Not ported, each with the reason in `src/wry/readme.md`: cookies (the API is
the `cookie` crate throughout), downloads, drag and drop (behind wry's own
feature flag), extension loading, the `NewWindowResponse::Create` arm (it
would put a COM type in the portable header), the `_async` constructors, and
Android and iOS.

**Only Windows has a backend.** The other three files are stubs that answer
"there is no webview here", and each says what a real one would take. macOS is
the one worth writing next — WKWebView is in the system SDK, so it takes no
ruled-out dependency — and it is not written because nothing here can build or
run it. Linux is the hard one and not for the reason it looks: wry's backend
is WebKitGTK and wants a GTK container, this tree's window is raw X11, and
bridging them is a GtkPlug/XEmbed project plus a second soft dependency. The
browser has no wry backend at all; an `<iframe>` over the canvas would answer
three calls of the twenty-odd and none of the interesting ones.

### The gpui side, and the one seam it needed

`src/webview/` is `crates/webview` clause for clause, with two deviations.
Rust builds the `wry::WebView` outside and hands it to `WebView::new`, because
only the caller has the `Window`; `Ctx` is the window here, so `WebViewNew`
takes the attributes and builds it. And `impl Render for WebView` is a view
whose whole body is one element — gpui needs an entity to be a view before it
can be a child — where an element here is a value, so `WebViewEl` hands one
back and the owner puts it where it likes. The entity is still an entity: it
is what the outside-click subscription binds to, and dropping it is what
closes the webview.

The seam is `PlatWindowHandle(Window*)` in `platform.h` — GPUI's
`Window::window_handle()`, which Rust answers as a `RawWindowHandle`. The
HWND on Windows, the X11 window id on Linux, the `NSView*` on macOS, null in
the browser. Nothing else in the tree has ever needed to name the OS window,
which is why it was not there.

Rust's element sets the bounds in `prepaint`; the first place an element here
knows its box is paint, so `El::customPaint` is where the OS control is moved,
and only when the box actually changed. One thing to know about making a
webview lazily from inside a Render: creation blocks by running the window's
message loop, exactly as `build_as_child` does, so a WM_PAINT can re-enter the
same Render before it returns. `examples/webview.cpp` sets its `started` flag
before the call rather than after, and the header says why.

### Checking it

`examples/webview` is upstream's example of the same name: an address bar over
the webview, Enter loads what is in it. The page renders, and it renders in
the box layout gave it — `bun cmd/shot.ts -rel webview` and the same with
`-half=right` are two window sizes with the control tracking the frame's
border either way.

`tests/WryTests.cpp` is the crate's own `checks_if_custom_protocol_uri` plus
the round trip through the two `replace` helpers beside it. Those three
functions moved out of `wry_win.cpp` into a portable `wry.cpp` to be testable
on any platform, which is the seam-rather-than-harness rule.

17,910 checks pass; `bun cmd/build.ts -rel -all` builds all 25 examples, and
the Linux build compiles the stubs and runs (an empty box and one log line,
which is what it promises).

## The second webview backend, and the one thing WKWebView will not do over ssh

`wry_mac.cpp` was a stub an hour ago and is `wkwebview/` now. It is the
backend that could always have been written — WKWebView is in the system SDK,
so it takes no dependency hard rule 3 rules out, and the only build change is
one more framework in `cmd/build-mac.ts` — and the reason it was not written
was that nothing here could compile it. That turned out to be wrong too:
`bun cmd/mac-build.ts` has been in this tree the whole time, and it compiles
on a Mac over ssh.

The shape of the port is the shape of the Rust, once one thing is accepted:
an Objective-C class cannot live inside a C++ namespace. Rust's delegates are
`define_class!` invocations with an ivars struct; here each is a real
`@interface` at file scope holding the `wry::WebView*` those ivars would have
held, and the file is laid out the way `window_mac.cpp` is — the namespace's
types, then the classes, then the API. There are five: the IPC message
handler (`WryWebViewDelegate`), the navigation delegate (the policy call, and
`didCommit` / `didFinish` for the two page-load events), the UI delegate (the
open panel, the media-capture grant and `window.open`), the title observer
(KVO on `title`), and one `WKURLSchemeHandler` where Rust builds a class per
scheme at runtime because ivars were the only place it had to put the index.

ARC does the work `Retained<T>` does in the Rust: the amalgam is compiled
`-x objective-c++ -fobjc-arc` on macOS, so the Objective-C pointers in the
`WebView` struct are strong references and the struct's destructor releases
them. The delegates are held there for the same reason Rust holds them — a
`WKWebView`'s delegate properties are weak, and nothing else would.

Three things in the crate's macOS half are not the Windows half, and each is
worth knowing:

- **`pending_scripts`.** An `eval` before the first navigation commits has no
  page to run in, so it is held and replayed from `didCommitNavigation`.
  Windows needs nothing like it because `ExecuteScript` is queued on the
  browser thread.
- **The custom protocol work-around is Windows' alone.** WKWebView takes a
  scheme handler for the real scheme, so `wry://` is `wry://` there and the
  `http://wry.host` tunnel does not exist. What macOS needs instead is the
  task-validity check: a stopped `WKURLSchemeTask` is a dangling pointer, and
  an asynchronous responder that answers late must not touch it. Rust keeps a
  UUID per task; this keeps the live tasks in an `NSMutableSet` and
  `stopURLSchemeTask` takes them out.
- **Three calls answer false.** `set_theme` and `set_memory_usage_level` are
  `WebViewExtWindows` with no counterpart, and `set_background_color` is the
  iOS half of the crate — on macOS the only knob is the `transparent`
  attribute, set on the configuration before the webview exists.

One deliberate difference from the Rust, and it is not a small one: **a
webview that is not a child is added as a subview**, where Rust replaces the
window's `contentView` with a `WryWebViewParent`. Rust does that so the page
gets key events; the content view here is the gpui view that draws everything
else and owns the window's input, so evicting it would take the application
with it. `reparent` is ours for a smaller reason — Rust has it on Windows
only, and on macOS it is `removeFromSuperview` plus `addSubview`.

Two compile errors were worth the trip. The first is a rule this tree has
never had to think about: `/**` inside a block comment is `-Wcomment`, and
`wry/src/wkwebview/**` was the first line of the file. The second is Cocoa
memory-management convention leaking into the compiler — a property called
`newWindows` "follows Cocoa naming convention for returning owned objects",
so ARC refuses it; it is `openedWindows` now. Nothing in the port itself was
wrong, which is the useful part of a first compile.

**It is compile-verified, not run-verified**, and the difference is worth
stating plainly. `bun cmd/mac-build.ts -rel -all` builds all 25 examples on
the Mac. Running one over ssh does not work and cannot: the process starts,
`pgrep` finds it, and no window appears, because a Cocoa application needs a
login session and `launchctl asuser` needs root to join one. AGENTS.md has
said so all along. Somebody at that Mac's keyboard running `bun cmd/run.ts
-rel webview` is what would put a page on the screen.

The Windows half is unchanged and still passes: 17,910 checks, all 25
examples, and the Linux stub still compiles and runs.

## Carrying layout across frames, and a notify that knows what it woke

Two entries above this one end with the same sentence: the structural win left
in layout is carrying it across frames rather than rebuilding the tree. It is
carried now, and a story frame is **0.70 ms, from 2.34** — layout **0.24 ms,
from 1.85** — with the other two phases where they were (build 0.25, paint
0.19). The four fixes before this one took 2.62 to 2.29 by making the work
cheaper; this one stops doing it.

### The tree is reconciled, not rebuilt

`gLayoutTree` was already kept between frames, but only so its node slots and
child arrays could be recycled: every frame called `Clear()` and built the
tree again from nothing. That threw away the thing worth keeping — **taffy's
own per-node cache**. A `PerformChildLayout` that hits that cache returns the
node's last answer and does not walk the subtree at all, so a page where
nothing moved should cost one cache hit at the root and nothing else. Instead
it cost `AddChild`, which marks a parent dirty, 1466 times.

A window now owns a `LayoutCache`, and each frame the element tree is
reconciled against it. The key is position — the nth child of the nth child is
the node it was last frame — because an `El` has no identity to key on: it is
built out of the frame arena every frame, and `El::Click(id)` names only the
handful of boxes that hit-test. Where the kinds no longer match, that subtree
is dropped and built again, which is what a page switch is.

A node is told something only when it has something to hear:

- **its style is not the one it carries.** `operator==` on `taffy::Style` is
  the `PartialEq` derive Rust has and this port had left out; it is in
  `src/taffy/style.h` now, field by field, with `Optf` compared by bits
  (Rust's `Option<f32>` is a NaN here, and `NaN == NaN` would report two
  Nones unequal) and the grid template slices compared by contents the way a
  `Vec` is. `tests/StyleEqTests.cpp` moves each field in turn, because the one
  way to get a stale frame out of this is a field the comparison forgot.
- **it is a measured leaf whose content moved.** The text bytes, the font it
  was prepared with, the weight, the line height, the wrap, and an image's
  natural size — which arrives after the load and changes the answer when it
  does. That hash is the node's `measKey`, and it is the only thing that marks
  a text leaf dirty.

Neither of those moves when a row is hovered, because a hover recolours a box
and a colour is not in `taffy::Style`. A hovered row costs no layout at all.

Two smaller things the shape forced:

- **The element pointer cannot go in the node's context.** `SetNodeContext`
  marks a node dirty, so handing taffy this frame's `El*` would undo the
  caching. The context is a `LayoutNode` record instead — allocated once,
  recycled with the node, and holding the `El*` and the `measKey` that are
  written in place. The measure function reads the element through it.
- **The root's `stretch_auto_size_to_fill` had to move inside the sync.** It
  used to be applied after the tree was built, which would have made the root
  differ from itself every frame and dirtied the whole page.

`MeasureEl` keeps a scratch cache of its own, reset per call: a measure builds
an element tree that has nothing to do with the last one, and a measure in the
middle of a frame must not disturb the window's tree. A `LayoutEl` with no
cache — a test, a one-shot — gets that same scratch, which is what every
caller had before.

`GPUI_LAYOUT_REUSE=off` resets the cache every frame, which is the old
behaviour exactly: 2.42 ms and layout 1.91, against the 2.34 / 1.85 this
started at. It is the first thing to try if a frame ever comes out laid out
stale, and it is what the two paths were compared with.
`GPUI_FRAME_BENCH` now prints what the frame had to tell taffy —
`nodes=1466 made=0 dropped=0 restyled=0 remeasured=0` is a page that did not
change.

**What is measured and what is not.** The bench draws the same frame back to
back, so what it shows is the cost of a repaint that changes nothing — which
is most repaints: a caret blink, a fade, a timer in one view. A frame that
changes *everything* does the work `reuse=off` measures plus one `operator==`
per node; that case is not separately benched, and the honest way to read
0.24 ms is "the floor is now the walk this tree does itself", not "layout is
free". The skeleton and progress pages, which animate colour and not size,
come out at 0.009 and 0.012 ms with `restyled=0` — the case this was built
for.

### Notify names the entity, and the windows that have it

`Notify(cx)` invalidated the window in hand, or every window when there was
none. It is `App::notify` now: it marks the entity, runs that entity's
**observers** — `Observe(cx, entity, &T::OnChanged)` is `cx.observe`, the
untyped half of the `Subscribe`/`Emit` pair that was already here — and
invalidates only the windows that rendered that entity in their last frame.
That set is `Window::rendered`, filled by `EntityRender` as the frame is
built, and it is GPUI's `Window::dirty_views` under another name.

The fallback is where the old behaviour lives on, and it has to: an entity
nothing has rendered — a state entity that is not a view, a view on its first
frame — still has to reach the screen, so it invalidates the window the notify
came from, and every window when it came from none. So this is a refinement
and never a regression: a notify that cannot be placed does what it always
did.

`tests/ObserverTests.cpp` pins the parts that are not the repaint: an observer
hears which entity notified, two observers both hear it, an observation can be
given up, an observer whose entity has gone is swept rather than called, and
`EntityRender` records the view against the window that asked for it.

**What is still coarser than GPUI, and why it stays that way.** A repaint
rebuilds the whole window's element tree. GPUI does the same — its per-observer
invalidation decides whether a window draws, not which subtrees rebuild — so
this is not the deviation it reads as. Retaining per-view subtrees *here*
would need dependency tracking this tree does not have: a view's render reads
hover, focus, the theme, the clock and whatever entities it feels like, and
none of that is declared. The build phase is 0.25 ms of the 0.70; that is what
it would be worth, and it would be bought with a class of bug — a view that
quietly stops updating — that nothing in the tree can currently catch.

### Checking it

All 65 story pages shot before and after: pixel-identical except `skeleton`
and `spinner`, which differ run to run against themselves because they
animate. Then the same 64 pages *touched* first — a scroll, a click and two
typed characters each — with the cache off and on: identical except the two
animating pages and three carets, each of which was re-shot on a quiet machine
and came out identical. Where a page differed only under load — `avatar`,
`collapsible`, `scrollbar` — the fresh shot matches the pre-change baseline
exactly, so what the sweep caught was a frame taken mid-fade while the machine
was building something else.

17,958 checks pass, `bun cmd/build.ts -rel -all` builds all 25 examples, and
the Linux and macOS builds compile.

## The command palette, and the field that was never focused

`gpui-component` cc89092f adds a `Command`: a search field over a filtered list
of commands, with groups, separators, keybinding hints and keyboard
navigation. It is the first component in this port that was ported whole from
upstream rather than being found there while chasing a fix, and one thing had
to be fixed underneath before its keyboard worked at all.

### The split, and where the model lives

Upstream splits it in two. `Command` owns the entries and the rendering policy
and is pushed into the state on every render; `CommandState` keeps the
interaction state — the query, the highlight, loading and scrolling. The same
split holds here, with one difference that runs through the whole tree: the
model is not copied into the state. The entries are the caller's array, the way
a `SearchableList`'s items are, so they have to outlive the frame.

`IndexPath` is the part worth being careful about. An item reports where it was
given, not where the filtering left it: an ungrouped item is section 0 and its
own position, a grouped one is its group and its position in the group, and a
model that mixes both puts the implicit ungrouped section first. That is what
lets an application answer `on_select` and `on_confirm` against its own model
while the palette shows a filtered list. `tests/CommandTests.cpp` pins it, along
with the heading that disappears with its group, the separator that is only
drawn where something follows it, and the highlight that skips the disabled
items and wraps.

Two things are done differently and knowingly. Rust measures every flattened row
with `layout_as_root` before handing the sizes to the virtual list, so a custom
row can be any height; there is no measure pass here, so the two standard rows
are the heights their padding and text come to and a custom row states its own
with `CommandItem::contentH`. And `input.set_loading` has no equivalent — an
`InputState` here has no spinner — so a loading palette puts one where the field
ends, which is the same information in the same place.

### The field that took the characters but not the chords

The palette would filter as you typed and would not answer an arrow, an escape
or an Enter. The cause was older than this component. `InputState::on_mouse_down`
upstream calls `focus_handle.focus(window, cx)`; the press here set `win->input`
— which is what WM_CHAR follows — and left `win->focusId` alone. Key *actions*
resolve against the contexts stacked over the focused element, so a clicked
field had no context over it, "Input" never matched, and every chord state.rs
binds resolved to nothing. Anything that focused a field some other way — a
dialog's focus trap, a select taking its content handle — worked, which is why
nothing had caught it.

`El::BindInput` is where a field is declared, so that is where the press-to-focus
opt-in went. The visible half of the fix is the focus ring: a clicked field now
shows one, and so does the Rust story's for the same click.

### Checking it

`bun cmd/compare-ui.ts input click:400,300 shot:focus` puts the two focused
fields side by side. All 62 story pages were shot before and after the focus
change: every difference is the sub-percent animation noise those sweeps always
carry, and the one page that changed on purpose — a clicked field — was checked
against Rust by hand. 18,167 checks pass and `bun cmd/build.ts -rel -all` builds
every example.


## The notification that leaves the window

gpui-component b80bb899 bridges a notification to the operating system's
notification center. Upstream builds it on gpui's `SystemNotification` — post
under a tag, retract that tag, hear that the user clicked one — which this port
does not have. So the checkin arrives in two halves: the component half, which
is a straight port, and a platform seam that had to be written first.

### The seam

`src/sys/notify.h` is the four calls, alongside `sys/http.h` and
`sys/sysinfo.h`, per-platform files and all. Only Windows has a backend.

It is not the WinRT toast API. That one wants an AppUserModelID registered
against a Start-menu shortcut, and — for a notification that outlives the
process — a COM activation server with its own CLSID under `HKCU\Software\
Classes`. What it buys over the alternative is a richer toast body and
retraction from the Action Center. The alternative is the notification area:
one hidden window owns one icon, and a balloon on that icon is what Windows 10
and 11 turn into a real toast and file in the Action Center, with the click
coming back as `NIN_BALLOONUSERCLICK` on the icon's callback message — on the
main thread, inside the loop that is already running. The cost is stated where
it is paid: one balloon at a time (a second post replaces the first, which is
what a tag asks for anyway), retraction takes the balloon off the screen but
not out of the Action Center, and the application has an icon in the
notification area from its first system notification until it exits.

macOS, Linux and wasm answer `SysNotifyAvailable() == false` and drop a post.
That is the same degrading Rust does on a mac that is not running from a
bundled `.app` and on a Linux with no notification daemon — the difference is
that here it is every run rather than some of them. `AppActivate` is new on all
four platforms: `window.activate_window()`, which a click on a notification
needs and nothing had needed before.

### The component half

`NotificationDelivery` is `InApp` (the default), `System` or `InAppAndSystem`;
`NotificationListState::delivery` is the global one and an item overrides it
with `hasDelivery`, which is how this tree spells `Option<T>` on a builder.
Only the title and the message cross over: an action, a content element and a
placement are the toast's alone, and a content-only notification — nothing
textual to show — is not posted at all. A message with no title becomes the
system notification's title, as upstream does.

The tag is `gpui-component/notification/<id>`, which does two things: a repeat
push with the same id replaces the notification in the center as it replaces
the card in the stack, and a response to a tag without that prefix belongs to
whoever posted it and is left alone. Rust derives the tag from a `TypeId`; the
ids here are the ints this port has always keyed notifications by.

`NotificationSystemEntry` is what a response needs — the window, the list, the
`onClick` — because a response arrives as a tag and nothing else. It is a
file-static array of 100, oldest pruned, where Rust keeps an `App` global of
the same size. A dismiss only retracts an entry posted from its own window: a
second window that pushed the same id owns the tag now. Retraction is on
explicit dismissal — the close button, `NotificationDismiss`,
`NotificationClear` — and not on autohide expiry, which leaves the notification
in the center, which is upstream's rule and the one a user would expect.

The response arrives from the platform's own event handler, so it is posted
back through `WindowPost` rather than acted on where it lands: the window comes
forward at once, and closing the card and firing `onClick` happen on the next
turn of the loop, where an entity may be touched. A post is dropped if the
window or the list has gone, which is what Rust's `WeakEntity` upgrade
swallows.

`NotificationPush` and its two siblings take a `Ctx*` now, as Rust's `push`
takes a `Window`. A null one is the in-app half on its own, which is what the
tests push.

### Checking it

The story grows the "System notification" section upstream added, and
`story.cpp` sets the app identity its `main.rs` now sets. Clicking "System
only" leaves the stack empty and "In-app and system" raises the card — both
shot and compared. A throwaway probe confirmed `Shell_NotifyIcon` accepts the
post on this machine. What is not automated is the part that needs a person:
that the toast is in the Action Center, and that clicking it brings the window
back. 18,207 checks pass and `bun cmd/build.ts -rel -all` builds every example.


## Twenty-six checkins, and what a port does with them

From `b80bb899` to `7885c416` — gpui-component's 19th to 24th of August. The
OS-notification bridge has its own section above; this is the rest, and mostly
it is about what *not* to write.

### The ones that landed

**A canonical dock tree.** `2cadad22` moves the whole dock foundation into
`gpui-base` and rebuilds the skin over it: the layout becomes pure data
addressed by NodeId, and two mutually recursive `remove_self_if_empty` methods
reaching through weak parent pointers inside `window.defer` become one
idempotent `normalize`. Nearly all of that is how this port was already
arranged — an array of nodes naming each other by index, a skin rebuilt from it
every frame — so what was actually missing was the canonical *shape*.
`DockPrune` collapses upward from an edit and knows about an empty node and a
split of one; it says nothing about a split nested in a split of the same axis,
or an active tab past the end, because no edit makes either. A file can, so
`DockNormalize` is the four rules run to a fixpoint and `DockLoad` ends with it.

**Springs.** `cc86f8d4` is the one substantial new mechanism. A transition
restarts its curve from the value sampled at the instant its target changes:
continuous in position, discontinuous in speed. A spring carries velocity
instead, so a switch toggled twice or an indicator chasing a click decelerates
and turns around. The integrator here is the closed form of the damped
oscillator rather than an Euler step — at these frame times an integrator makes
the motion depend on the frame rate — with the underdamped, critical and
overdamped cases spelled out. Sprung: the switch thumb, the checkbox tick, the
accordion panel, the slider's thumb ring, the tab indicator, the dock's drop
placeholder, the toast stack's geometry and fade. A response is not a duration,
so upstream's numbers replaced the durations rather than sitting beside them.

**Two real bugs, both about state outliving what it described.** A committed
IME composition never closed its undo transaction, because neither platform
sends an unmark after a confirmed candidate — so everything typed afterwards
merged into it and one undo took the lot. And a field removed from the tree
while focused left the window pointing at it, which here is a pointer that
outlives the state; the field now takes its registration with it, and a closing
window blurs what it had.

**Smaller.** A masked field keeps its value out of the clipboard and treats the
whole of itself as one word (there are no boundaries on screen to move by). The
title bar draws window controls only when the frame is actually ours, which on
X11 means the Motif hint *and* an empty `_NET_FRAME_EXTENTS`. A hidden dock slot
stops occupying a slot, and the growth goes to the last one that is drawn. The
command palette gained `Filterable(false)` for a source that answers the query
itself, and its rows' inset moved onto the virtual list, where it behaves as CSS
scroll-padding does. The editor's rows follow its font. The dark syntax palette
came from upstream's own JSON through `gen-theme-data.ts`.

### The ones that did not, and why

Ten checkins fixed things this port cannot have. Two are worth naming because
they say something about the shape of the port.

`e5b8a3f4` snaps highlight runs to character boundaries because gpui's shaper
*panics* when a run splits a multi-byte character, and a stale tree-sitter tree
can hand it one. Neither half exists here: a line is shaped whole and a span
only clips what is drawn, and the highlighter is a synchronous scan of the
current text with no background reparse to fall behind. The fix is real; the
failure is not reachable.

`16274ece` and `f478ff6b` are Rust ownership — wrapping without copying lines
out of the rope, not copying the value into a presentation struct. The text
here is a flat buffer and `InputValue` hands back a borrow of it. A port whose
data structures differ inherits neither the cost nor the fix.

The rest: two docs runs, a skills bundle, a Cargo feature, a Rust build config,
a scrollable that needs an explicit id because Rust derives one from the
caller's source location, a dock resize handle for the same reason, a combobox
whose selection is resolved against the filtered view, a single-line input
painting scrollbars, a menu icon read from the CWD, and a dock size animation
added and then removed two checkins later.

One was deferred rather than declined: `cb87f2cf` moves the story's app menus
into the macOS system menu bar. There was nowhere to move them to — `native_menu`
here is popup and context menus, and nothing set NSApp's main menu — and
hiding the in-window bar without that would have left a Mac with no menus at
all. The section below is that missing half, written and then the checkin
applied on top of it.


## The menus that are not in the window

`AppSetMenus` is the port of gpui's `App::set_menus`, and `cb87f2cf` — the
story's macOS menu bar — is what it was written for.

A Mac application's menus are not in its window. They are the bar at the top
of the screen, they belong to whichever application is in front, and they are
there whether or not that application has a window open at all. Nothing else
this tree targets works that way: Windows and X11 leave a menu bar to the
application, which is what `component::AppMenuBar` draws into the title bar,
and a browser tab has nowhere to put one. So the seam is one call and three
platforms that ignore it — `PlatSetAppMenu`, with the honest answer to
`PlatHasAppMenu()` beside each one.

**A row carries an action.** That is the whole of `MenuRow`: a label, an
action and what it carries, and either rows under it or not. It is what Rust's
`MenuItem::action` is, and it is what makes one table serve both bars —
choosing a row dispatches, and where the handler lives is the same question it
was for the keystroke. The alternative, a callback per row, would have made
the OS bar a second wiring of the same menu, which is exactly the drift the
Rust checkin does not have: `build_menus()` feeds `cx.set_menus` and the
in-window `AppMenuBar` from one value.

The story now does the same. Its four menus are one `MenuDef` table built each
frame; `StoryMenuBar` turns them into the PopupMenus the title bar opens, and
`StorySetSystemMenus` installs them when a row has moved — hashing the rows is
cheaper than rebuilding an NSMenu sixty times a second, and it is the same
"only when something changed" that Rust gets from observing the theme. The
handlers moved off the row indexes onto the root as `OnAction`, which is where
Rust's `cx.on_action` reaches from.

**The shortcut is not typed into the row.** `KeymapAnyBindingForAction` is
Rust's `bindings_for_action`: the contexts are ignored, because a menu row is
outside every element and the binding that names its action belongs to
whichever one it applies to — the Edit menu's rows are bound in `Input`. What
the OS is handed is the chord, and matching it is then the menu bar's job:
⌘C fires the row, the row dispatches `input::Copy`, and the field that has the
focus handles it. Which is the same place the keystroke would have arrived on
its own.

**What macOS gets that the drawn bar does not.** The first menu is the
application menu whatever it is called — AppKit titles that one after the
process — and a menu named `Window` is handed to `setWindowsMenu:`, which is
what adds Minimize, Zoom and the list of open windows to it. Neither has a
counterpart in the window, and neither is worth faking there.

The story's own half of `cb87f2cf`: `showAppMenuBar` is off on macOS, where
the menus are in the system bar already and a second copy of them would be
odd, and the freed up left side of the title bar names the window instead. It
stays switchable — the row is in the Appearance menu — because a component
gallery is where you would want to look at the component itself.

That menu carries actions now as well, which it did not: it was a table of
rows dispatched by their index, and the switch would have been the one row in
the story that worked some other way. `SelectFont(18)` is the action with the
value on it, and the three toggles carry nothing because what they flip is
what they read. Nothing in the story's menus reports an index any more.

**Where a row's chord actually comes from.** A menu row has no shortcut field,
here or in Rust: it shows what the keymap has bound to its action. So a Quit
row with no ⌘Q is an application that never bound one, and the story now binds
what upstream's story binds — `secondary-o` for Open, ⌘Q on macOS and alt-F4
elsewhere for Quit. Alt-F4 is upstream's too, and is a fallback rather than the
path: Windows and most window managers close the window before the keystroke
reaches the application. The two Window rows are the port's own, so their
chords are as well, and only on the Mac — ⌘N and ⌘W are what every application
there uses for them, and an unscoped ctrl-w elsewhere would close the window
out from under whatever was in the middle of something. `KeyName` learned to
spell f1..f12 on the way, which is the other half of a parse that has always
read them and the reason Alt+F4 showed as nothing beside Quit.

Quit also had to become quit. `AppQuit(win)` closes the window it names and the
loop ends when the last one has gone, which is the same thing while there is
one window and not the same thing at all once the Window menu can open a
second. `AppQuitAll` is `cx.quit()`: the list copied, then every window still
in it closed.

What gpui's `os_action` is, for the record, is not that: it maps six rows — cut,
copy, paste, select-all, undo, redo — onto AppKit's own selectors so the
responder chain handles them in a native control. Nothing in this port is a
native control; a field here is drawn by the port and already answers those
actions, and the menu dispatches them to it. There is nothing for the
responder chain to do.

One thing is still not verified. A process started over ssh never joins the
console session, so the macOS build compiles and runs but its window, its menu
bar and the ⌘Q on it need somebody at the machine. What was checked here is
the Windows and Linux side of the same change — the drawn bar, its submenus,
the chords beside Open and Quit, the actions arriving through the root, Quit
leaving nothing running, and the title-only title bar — plus the numbering,
the keymap lookup and the key names under test.


## Thirty-two frames a second, and where they went

`fps_monitor` looked twice as slow as it is, and was.

**The measurement first.** Three numbers that are not the same thing kept
getting compared: what a frame *costs*, how often a frame *happens*, and what
the HUD's FRAME row reads. The HUD is a mean over the last 120 frames, so for
the first seconds after launch it still has the warm-up in it — the swap
chain, the shaders, the glyph atlas — and reads high. And the cost itself
moves with the CPU's clock: the same build, same window, same six curves is
1.02 ms with a busy core beside it and 1.9–3.2 ms on an otherwise idle
machine, because a process that sleeps 25 ms between frames wakes on a core
that has clocked down. Every number below was taken with `-bench`, which runs
the window for real and reports the distribution of `Window::draw`, with a
spinner pinned on another core so two runs can be compared at all.

**No regression.** The same benchmark at `9787520`, `8e8b045` and `b0bd623` —
three weeks of commits — gives 1.076, 1.073 and 1.092 ms. Nothing in the
recent work touched it.

**What was actually wrong: the frame rate.** An animating window asks for a
16 ms timer, and `SetTimer` counts in system clock ticks — 15.625 ms — and
rounds a request *up* to the next whole one. So 16 ms landed on the second
tick, 31.25 ms, and every animated window in the tree ran at 32 FPS on a
60 Hz display no matter how little it spent drawing: a 1.0 ms frame arriving
every 29 ms. `PlatSetTimer` now rounds the ask *down* to a whole number of
ticks, so the timer fires at the last tick before the deadline instead of the
first one after it. Early is safe and late is not — `WindowTimerTick` skips
the timers that are not due and re-arms from what is left, so an early wake
costs one more pass through the loop, while a late one is a frame that did not
happen. `fps_monitor` goes 34.6 FPS -> 63.2 FPS at the same 1.0 ms a frame,
and the HUD reads 64 with the figure green rather than 39 in amber.

GPUI does not use a timer here at all: its Windows backend waits on the
compositor clock (`DCompositionWaitForCompositorClock`), which is the display's
own cadence rather than an approximation of it. This lands within a tick of the
same rate without a second thread; what it does not get is the phase, so a
frame can still be handed to DWM just after a vblank.

**Where the 1.0 ms goes.** `winperf record -i 2000 -print-agent` on a paced
run puts 59% of the process inside `CanvasLine` — D2D's `DrawLine`, and under
it `WidenLine`, `FillRectangleTessellator::SendGeometryStatic`,
`AddAntialiasedTriangleStripPoints`: the CPU tessellating ~2300 antialiased
hairlines into triangle strips, one command each. `PaintEl` — the port's own
drawing, which only records into the scene — is 0.25 ms of that frame, and
`EndDraw` plus `Present` together are 0.4 ms. This is the same wall an earlier
session hit, and batching a colour run into one stroked path does not move it,
because the tessellation is per segment either way.

One earlier claim here was wrong and is worth correcting: a bigger window is
*not* fill-bound in this demo. At 1600x1020 the frame costs 1.022 ms against
1.023 ms at 800x600 — the cost is per segment, not per pixel.

The backend that does not have this cost is already here: `GPUI_PAINT=gpu`
rasterizes the same frame in a shader and runs it in 0.364 ms, 2.8x faster
than Direct2D, at either size. That is what GPUI does on every platform. It is
still not the default, for the two reasons `src/gpui/paintgpu.h` gives.

`fps_monitor` gained `-size WxH` alongside `-bench` and `-curves`, since a
line-drawing number measured at one window size says nothing about another.


## The language the components speak

`rust_i18n` was the last thing in `crates/ui` with no counterpart here, and it
is why the app menu had no Language submenu and why a calendar read `Su Mo Tu`
whatever the application asked for.

**The catalogue is upstream's, not a transcription.** `crates/ui/locales/ui.yml`
is 55 keys — the weekday heads and month names, the placeholders, the dock's
tab menu, the colour picker's rows, the dialog's two buttons, the find bar's,
pagination's — each with up to five translations. `cmd/gen-locale-data.ts`
reads it and `crates/story/locales/ui.yml`, which is where the French comes
from, and writes `src/ui/locale_data.cpp`: one row per key, sorted, one string
per locale, null where the catalogue has nothing. It is the same arrangement
`gen-theme-data.ts` has for the palette, and for the same reason — a later
checkin's wording lands by re-running the generator rather than by somebody
retyping it. The parse is not a YAML parser and says so: both files are three
levels of `key:` lines, the story's nested one deeper under rust_i18n's crate
namespace, and anything else in them is a reason to look rather than to widen
the reader.

**`Tr("Dialog.ok")` is `t!("Dialog.ok")`.** A `Str` over a string literal, so
it outlives the frame and needs no copy; the locale in force is process-wide,
the way the theme and the scrollbar mode are, since an App here is not a
container for globals. A key with no value in the current locale falls back to
English, which every key has — that is how Chinese gets a calendar and French
gets `OK`. A key nothing has answers with itself, which is what rust_i18n does:
a missing translation reads as `Dialog.nope` on screen instead of as an empty
label nobody notices.

Thirteen components read from it now, which is every place the catalogue has a
key for and this tree has somewhere to put it. Two keys have no home yet and
are left alone rather than faked: `Input.Decrement` / `Input.Increment` are
accessibility labels on the number input's steppers, and the six under
`Input.Cut` and friends belong to a field's context menu, which is not ported.

**The story's Language menu** is `language_menu()`: English, 简体中文,
Français, checked against the locale in force and dispatching
`story::SelectLocale`. Rust reloads its menus from that action because its
menu bar is built once; ours is built every frame and installed when a row
moves, so the language reaches the macOS menu bar the same way it reaches the
drawn one. Switching refreshes every window — nothing observes the locale, and
every label was read while the frame was built.

`tests/I18nTests.cpp` pins the lookup: the fallback to English, an unknown key
answering with itself, a locale that does not exist being refused, and the two
invariants the generated table has to keep — sorted keys, because the lookup
is a binary search, and an English value on every row, because that is what
the fallback rests on.


## The frame that was drawn to the window's old size

Maximize the showcase and its overview comes back a single narrow column of
tiles instead of three, and it stays that way for as long as you leave it
alone. Click anything and it is suddenly right. The same thing on a smaller
scale is what a resize drag looks like, and what "the border around *All
components* is badly drawn for a moment" was.

The scene layer culls: a primitive whose bounds fall outside the view is not
drawn, which is where three quarters of the story's primitives go. The view it
culls against is `gViewW/gViewH`, taken in `scene::FrameBegin` from
`PaintCtx::viewW/viewH`. And `FrameBegin` runs inside `PaintTargetBegin`,
which `WindowDrawFrame` called **before** it set those two fields for this
frame. So every frame culled against the *previous* frame's view.

That is invisible while a window keeps its size, and wrong the moment it
changes: a window that has just grown draws one frame with everything outside
the old rectangle thrown away — the left 840×640 of a 1920×1129 window, which
for the overview is the first column of each row and nothing else. Nothing
asks for another frame after a resize, because nothing has changed as far as
the application is concerned, so that culled frame is what stays on screen
until the next click or keystroke. Seconds, if you are reading rather than
clicking.

The fix is the two assignments moved above `PaintTargetBegin`, where the
comment now says why the order matters.

Two things this cost, both worth writing down. The tooling could not see it:
`cmd/shot.ts` captures with `PrintWindow`, which makes the window render, so
every screenshot showed a correct frame no matter what was on screen — and a
screen grab is not available from a headless agent session (`BitBlt` from the
screen DC comes back black, `CopyFromScreen` throws). What found it was the
app's own log: the scene's `view=` printed 840×640 on the frame after a
`WM_SIZE` that said 1920×1129. And `GPUI_LAYOUT_REUSE=off` "fixing" it was a
red herring — rebuilding the layout tree every frame changes what is dirty,
not what the scene culls against, and the wrong frame it drew was simply a
different wrong frame.


## The page that was never attached to the tree

Click a component in the showcase and click *All components* to come back, and
the overview is drawn as nothing — every box at zero size — and stays that way
until the next click or keystroke redraws it. Seconds, if you are reading
rather than clicking. This is the bug the previous two entries were circling.

**What it was.** The taffy tree is carried across frames and reconciled
against the element tree by position. Where a child's *kind* changed, the node
cannot be reused: `LayoutSync` built a new one and the caller put it where the
old one sat —

```
taffy::NodeId now = LayoutSync(sc, c, old, true, false);   // drops `old`
if (now != old) {
    lc->tree.ReplaceChildAtIndex(prev, i, now);            // ...at index i
}
```

— except that the drop inside `LayoutSync` calls taffy's `Remove`, and Remove
takes the node **out of its parent's child list**. The list is then one
shorter, and the index the caller is holding points at the wrong child, or
past the end. Where the replaced child was the last one — a page's whole
content under a container that has exactly one child, which is every page in
the showcase — `ReplaceChildAtIndex` found index 0 in a list of length 0 and
did nothing at all. The new subtree was never attached to the tree, so taffy
never laid it out, and all 96 of its boxes wrote back the zero rectangle they
were made with.

The order is Rust's, and it is the other way round: `replace_child_at_index`
first, `remove` second. The drop moved out to the three callers, after the
swap.

**And a second one beside it.** taffy's `Remove` does not dirty the parent —
Rust's does not either, because Rust's callers reach for `set_children`, which
does. Our reconcile removes trailing children directly, so a parent that only
*lost* a child changed in no other way that taffy could see, and kept the
layout it had when the child was still there. It says so itself now.

**How it was found.** Not by screenshot: `cmd/shot.ts` captures with
`PrintWindow`, which makes the window render, so every capture came out of a
frame drawn after the fact and looked right. `GPUI_LAYOUT_DUMP=<path>` is what
found it — every frame's laid-out tree written to a file, with what became of
the frame in the header:

```
--- frame 7 t=4.837 view=840x640 prims=82 presented=1 nodes=99 made=96 dropped=21
0 id=0 x=0.0 y=0.0 w=840.0 h=640.0
  0 id=0 x=0.0 y=0.0 w=840.0 h=640.0
    0 id=0 x=0.0 y=0.0 w=840.0 h=640.0
      0 id=0 x=0.0 y=0.0 w=0.0 h=0.0        <- and everything under it
```

96 new nodes, 96 zero rectangles, and `presented=1`: that frame went to the
screen and nothing asked for another. It is worth keeping for the next time
something is laid out wrong, and `GPUI_LAYOUT_REUSE=off` beside it says in one
run whether the reconcile is to blame.

`tests/LayoutReuseTests.cpp` pins all three shapes — a child whose kind
changed, a page switch that replaces a whole subtree, and a parent that lost a
child — and each of them fails on the code as it was.


## The tree that could not be scrolled

A wheel over the showcase's tree scrolled the page behind it. The tree had
nothing to scroll with: the page built its own rows into a clipped box, and a
clipped box is not a scroll container — no offset, no `ScrollId`, no
`OnScroll`, so `DispatchScrollWheel` walked past it to the window's own
handler.

The reason it was hand-built is where this port had put the tree. Upstream
splits it the way it splits everything else: `crates/base/src/tree.rs` is the
*unstyled, virtualized element* — a `uniform_list` with `track_scroll`, whose
row comes from the caller's `item(..)` closure — and `crates/ui/src/tree.rs`
is a row drawn into it. This tree had the state in `src/base` and the element
in `src/ui`, so the base showcase, which may not touch `src/ui`, had no
scrollable tree to reach for and drew a box instead.

`TreeList::New(cx, id, state, h, row, user)` is that element, moved down where
Rust keeps it: the visible range, the two spacers standing in for the rest,
the scroll container, the press handlers around each row and the tree's key
context. The row is the caller's — a function and a user pointer, since an
element here holds no closures. `component::Tree` is now its themed row and
nothing else, and the story's tree page is pixel-identical to before.

The showcase's page is then upstream's: a `TreeState` on the app, the base
element, and an `item` that draws the indent, the chevron and the label. It
scrolls with the wheel, walks with the arrows, and its rows select and expand
on a press — all of which come with the element rather than being spelled out
again on the page.

One thing to know when building a `TreeState`: it keeps the `Str`s it is
given rather than copying them, and it outlives the frame, so the ids and
labels must not come from the frame arena. The page's first attempt did, and
drew a treeful of replacement characters.


## The thumb that could not be dragged

The showcase's scrollbar page drew its own thumb. It worked out `thumbH` and
`thumbY` from the offset and laid an absolutely-positioned grey box over the
content, and the offset itself came from a branch in the window's wheel
handler — `if (app->component == CompScrollbar) { app->exampleScroll -= delta; }`.
So the bar was a picture: pressing it did nothing, dragging it did nothing,
and the page it was demonstrating was the one page in the showcase where the
scrollbar was not real.

The element was not missing. `Scrollbar::New(cx)` in `src/base` returned a
bare div, and everything a scrollbar is — the thumb, the hover widening, the
track press, the drag, the fade — was already in the renderer under this
tree, where a scrolled box draws its own bars. What was missing was the way
in: a box becomes a scroll region only when it has **both** a `ScrollId` and
an `OnScroll`, because `DispatchScrollWheel` skips a region with no listener
and lets the wheel fall through to the window. A caller could set `ClipY` and
`ScrollY`, get something that looked scrolled, and never take the wheel. That
is the same half-wired box the showcase's tree had.

`Scrollbar::New(cx, id, scrollY, scrollX, onScroll, axis, mode)` is the whole
thing in one call — Rust's
`div().overflow_scroll().track_scroll(&h).child(Scrollbar::new(&h))` — so the
halves cannot be separated. `Scrollbar::Vertical(..)` is the common case.
`component::Scrollable` is now a themed wrapper over it and the story's
scrollbar and dialog pages are pixel-identical; `ScrollAxis` moved down to
`base/scrollbar.h` beside the box that owns it.

The page is then upstream's page: a list, a `Scrollbar::Vertical`, and no
arithmetic. The wheel scrolls it, the thumb tracks, and dragging the thumb to
the bottom brings up Activity 20 — none of which the page had before.


## The virtual list that was virtual only on the page

`ui/virtual_list.rs` upstream is twenty-two lines, and all of it is a
re-export plus a test asserting the ui type *is* the base type. The list
itself — the visible range, the spacers, the handle, the deferred
`scroll_to_item` — is `base/virtual_list.rs`, 879 lines of it. Here the
arithmetic was in `src/base` and the assembly was in `component::`, so the
base showcase, which cannot reach `src/ui`, worked out its own window of rows,
translated them with an absolute `Top(first * rowH - scroll)`, drew its own
thumb, and had the offset moved by a branch in the window's wheel handler.

`VirtualList::New(cx, id, opts)` is the assembly, moved down. `VirtualListOpts`
is what Rust passes to `v_virtual_list`: how many items, how long each is, the
handle or the offsets, the axis the bars follow, and the row builder — a
function and a user pointer, since an element here holds no closures.
`component::VirtualList` keeps its builder API and is now the theme and
nothing else: the `Item N` row a caller did not supply. The story's
virtual-list and list pages are pixel-identical.

The showcase page is then upstream's: a row function and an `Opts`. It has a
real thumb, the wheel reaches it because the list sets both the scroll id and
the listener, and the window's wheel handler no longer needs to know which
page is open.


## The resizable group Rust never gave a themed half

Upstream has no `ui/resizable.rs` at all. `ResizableState`, the panel group,
the handle over each boundary and the drag that moves it are 1278 lines of
`crates/base/src/resizable`, and the only thing a theme has to say about any
of it is what colour the hairline is. Here the arithmetic —
`resize_panel_at_handle` and `adjust_to_container_size` — was in `src/base`
with two bare divs beside it, and everything else was `component::`. So the
base showcase's page carried its own `OnResizeDown`, `OnResizeDrag` and
`OnResizeUp`, its own 116..210 clamp written out twice, and a divider that was
a div: it was the only page in the showcase with hand-written mouse handlers.

The group moved down whole. The one line that could not follow is the
hairline's colour, so the group takes it: `HandleColors(rest, dragging)`,
which the themed `component::Resizable::New` fills with `th.border` and
`th.dragBorder` and the base showcase fills itself, the way it supplies every
other colour on its pages. `component::Resizable` is now that one call.

**And a bug that came out with it.** The handle was sized across the axis from
the group's *measured* box, which is written at paint — so on the frame that
first measures it the handle has no height, and a page that draws once and
then stops has a handle that cannot be grabbed at all. It never showed in the
story, where something else is always redrawing. The handle now fills its own
panel instead, which needs no measurement: the showcase's divider drags on the
first frame, and the story's resizable, dock and tiles pages are
pixel-identical.


## The calendar that every caller drew again

`base/calendar.rs` is a thousand lines and `Calendar::new(id, &state)` is an
element: the header with its two arrows, the month and year toggles, the day
grid with its flanking days, and the month and year pickers the toggles switch
to. What a *cell* looks like is the caller's, through
`.item(|item, state, _, _| ...)` — the calendar builds the slot and hands it
over. Here `base/calendar.h` was date arithmetic and a marker div, and the
whole element was `component::`, so the base showcase's page wrote its own
`DaysInMonth`, its own Sakamoto weekday, its own six-by-seven loop and its own
`ClickCalDay + 0..41` block of ids — and got no month picker, no year picker
and no toggles, because a page is not going to write those twice.

`Calendar::New(cx, id, CalendarOpts)` is that element, moved down.
`CalendarItemKind` and `CalendarItemState` are Rust's, and `CalendarItemFn` is
`item(..)`: the slot's kind, what it stands for, and the five flags a look
could turn on — active, in range, muted, disabled, today. The themed calendar
is now one function of ninety lines that switches on the kind, and the story's
calendar and date-picker pages are pixel-identical.

The showcase page is then upstream's page: an item function and the state.
Clicking `Aug` opens the month picker, clicking a month closes it, the arrows
page the years in the year view, and a day in a flanking month selects into
that month — because the cell carries its own date, which is what stops the
page having to work out which month a click landed in.

`CalendarOpts::today` is passed in rather than asked for, so what day it is
can be said rather than assumed.


## The dock that had no skin to hand out

Upstream's base showcase has forty pages and this one had thirty-nine. The
missing one is `dock`, and the reason it was missing is the same reason the
tree, the scrollbar, the virtual list, the resizable group and the calendar
were half-built: the split between `crates/base` and `crates/ui` had not been
drawn here. `crates/base/src/dock` owns the tree, the drag, the drop and the
resize and **draws nothing at all** — every pixel comes back through
`DockAreaRenderer`, `TabGroupRenderer` and `TilesRenderer`, and `crates/ui` is
one implementation of those traits. That is what lets upstream's showcase put
a dock on a page: its `ShowcaseDockSkin` is the other implementation, 478
lines of it. Here the whole element was `src/ui/dock.cpp`, so the base
showcase, which may not touch `src/ui`, had no dock to reach for and simply
had no page.

`src/base/dock_area.cpp` is that element, moved down: `DockArea::New(cx, id,
state, renderer)` walks the tree, lays the three Docks around the centre,
sizes each split and its handles, puts each group's body under its bar, springs
the drop placeholder and defers the dragged tab's preview. `DockRenderer` is
the trait table — an element holds no closures, so it is function pointers and
a `data`, the way `CalendarItemFn` and `VirtualListOpts` already are — and a
null hook falls back to a bare `Div`, which is a default method.

Where Rust hands the skin a context object whose methods reach back into the
area, this hands it a `DockCtx` / `DockHandleCtx` / `DockTabGroup` and a set of
`DockBind*` calls: the skin builds the element and base wires the behavior onto
it. `DockBindTab` is `group.select_tab(ix)` plus `group.drag_panel(ix, cx)`
plus the drop that lands on a tab; `DockBindTabStrip` is `track_scroll` and
`scroll_to_item`; `DockBindResizeStrip` is the drag upstream's showcase skin
stashes a `DockContext` for. `DockHandleCtx` carries a `hovered` Rust has no
need for, because nothing in this layer may read the window's hover itself and
a theme that lights the strip under the pointer has no other way to ask.

`src/ui/dock.cpp` is then one skin: the tab bar with its toggles, its zoom
button and its ⋯ menu, the single-panel title row, the four-DIP grab's paint,
the Dock boxes and the drag preview. The story's `dock` and `tiles` pages are
pixel-identical to the build before.

The showcase page is then upstream's page: five panels, a centre split of two
groups, a bottom Dock, and a skin of its own in the flat palette every base
showcase page supplies for itself (`0xf4f4f5` chrome, `0x2563eb` accent) —
26-DIP tab bars, hairline split handles, an absolutely-positioned resize strip
on each Dock's inner edge and a 20%-accent drop indicator. It is the first
showcase page that takes the whole viewport rather than being centred in it:
`fillsViewport` in `ShowcaseApp::Render` is Rust's `fills_viewport`, and the
row it switches the page container to is why the dock resolves to its 420 floor
instead of collapsing.

**And two dead branches that came out with it.** The themed tab bar marked the
tab a drop would land before, and lit the run of bar past the last tab, by
comparing `WindowDragOverId(cx)` against the hash of an id string the element
never carried — `Id(..)` is only a name and a hit rect is found by `clickId`,
which was still the *other* id `BindClick` had set. Neither marker had ever
drawn. One id per element in `DockBindTab` / `DockBindTabRest` is what fixed
it.

Two smaller gaps closed with the same sweep, both on pages this tree had but
had not finished. The toggle group's Italic and Underline cells were built with
an empty `Listener{}`: they drew their pressed state and could not be pressed.
They are `toggle_group_selection`'s two bits now, `|= 1` / `&= !1` the way Rust
writes them, and the second and third cells lost their left border
(`border_l_0`) so a `gap_0` row shares one hairline rather than drawing two.
The radio group's disabled row was the enabled row with a flag; it is Rust's
own shape now — `opacity(0.45)`, an empty box with no dot branch at all, and a
second line that keeps the ink colour rather than the muted one.

Thirty-five of the thirty-nine existing showcase pages are pixel-identical;
the four that moved are the overview (a fortieth tile), radio-group and
toggle-group (the fixes above), and `dock` itself.

**Not verified interactively.** Synthetic clicks did not land in the session
this was written in — `cmd/shot.ts -click` changed nothing on any page,
including ones that predate this work, while `-wheel` did — so the dock page's
tab switching, tab dragging, dock resizing and Bottom toggle have been read
rather than driven. The behavior they run through is unchanged and shared with
the story's dock page, which is pixel-identical, but the next session should
drive the page once.


## What was left of the click-id era

`ClickRadioStd` and `ClickRadioExpress` came out with the dock work: two
constants in the radio page's id enum that nothing had named since the pages
stopped switching on a click id. A sweep of every enumerator declared under
`examples/` says there were twenty-seven more.

Seven enums were dead outright — `system_monitor`'s `enum ClickId` (its tabs
and its four sortable columns), and the showcase's date-picker, number-input,
pagination, popover, sheet and toast pages. Six more had a dead member beside
a live one, and the story's shared enum carried ten: the search's clear
button, the five toolbar size ids, the two menus and the four accordion
option ids. `src/` has none — its four `ClickWin*` are all in use.

Three helpers went with them, all of them shaped by that era: `ScField`,
`ScBtnInk` and `ScBtnLine` each take an `int id`, and nothing in the tree has
called any of them since.

**And the gap the sweep was really for.** `InputBase::New(cx, id, clickId)`
is `UiRoot`, which gives the element a `Click` and a `FocusId` only when the
id is non-zero — `src/ui/input.cpp` passes `disabled ? 0 : HashClickId(id)`,
so zero is what that call means by *disabled*. Four showcase fields were
passing a literal `0` while being perfectly enabled: the colour picker's hex
readout, the combobox's search box, the dialog's Display name and the
number input's field. None of them was a tab stop, none could take the focus
ring, and none had a hit id of its own — the same shape of bug as the dock's
drop markers, which named an id the element never carried. All seven of the
showcase's fields now derive the id from their own name, the way `src/ui`
does, and `ClickEditor`, `ClickInput` and `ClickTextarea` died with the
change. `focusOnPress` defaults false, so this adds a tab stop and a focus
target and changes nothing about what a press does.

Thirty-five of the forty showcase pages are byte-identical; the five that
move are the scrollbar, tree and virtual-list thumbs mid-fade and the caret
mid-blink on the editor, input and number-input pages, all of which differ
between two runs of the same binary.

What is deliberately left: the hand-numbered id space itself. Nine constants
are still live — `ClickBack`, `ClickOverview + Comp*`, `ClickStory + Story*`,
`ClickSearch`, `ClickAlertCancel`, `ClickColor`, `ClickSwatch + 0..4`,
`ClickCombo`, `ClickDlgCancel`, `ClickHover`, `ClickSlider`, `ClickPara + i`,
`ClickTooltip`, and `dialog_overlay`'s seven — plus the story's two raw
`ScrollId(1)` / `ScrollId(2)`.

Rust has no such space at all, which is worth writing down. `ElementId` there
is a *value* — `Name`, `Integer`, `NamedInteger`, `View`, `FocusHandle`, and
five more — and what identifies an element is the **path**:
`GlobalElementId` is the stack of ids from the root down, pushed and popped by
`Window::with_id`, so an id only has to be unique among its siblings. That is
why upstream writes `div().id(("showcase-tab", ix))` inside every tab group
and never has to think about the one next door. Here a hit rect is found by a
single flat `int` per frame with no path above it, so an id has to be unique
across the whole window, and `HashClickId` is the bridge: FNV-1a of the name,
masked to 30 bits, and — this is the giveaway — bumped to 1000 if it lands
below, which is a band reserved for exactly these hand-assigned constants.

The one that is *not* in the reserved band is the story's `ClickStory = 1000`
plus its seventy pages, 1000..1069, which sits inside the hashed range. It
has never collided, and it is the first one to convert.


## GlobalElementId, folded — and what it found

`El::PathId(name)` is `div().id(name)`: the element is named, and the id it is
found by is that name folded with its ancestors'. `IdsCollect` walks the built
tree once a frame, before layout, and fills `El::pathId` in; an element with no
name inherits its parent's, exactly as one with no `.id()` pushes nothing in
Rust. `PathClick` is the same without joining the tab order. An explicit
`Click(v)` or `FocusId(v)` — including `FocusId(0)`, how a decorated wrapper
stays out of the tab order — wins over both. `tests/ElementIdTests.cpp` pins
the six rules, and the walk costs nothing measurable: the story's build phase
is 0.295 ms against 0.288 before it, inside the run-to-run spread.

`GPUI_ID_CHECK=1` reports every id two elements in one frame share. Rust cannot
have that problem — it compares whole id paths, never a hash of one — so it is
worth being able to ask whether this tree does. What it says:

- **The showcase is clean.** Forty pages, no page sharing an id.
- **The story's `table` and `data-table` pages are not**: 20 of 37 ids and 45
  of 197. The cause is not a hash collision, it is a name: `component::Table`
  opens with `gpui::Table::New(cx, StrL("table"))` and its rows are
  `table-row-%d`, neither of which knows which table it belongs to. Two tables
  on one page are one id space. **Fixed**: `component::Table::New` takes an id
  now, the way `Table::new("example-table")` does upstream, and every part
  under it is named locally — `row-3`, `head-1`, `body-0` — and told apart by
  the path. The story's table page goes from 20 shared ids to none.

  Two details from doing it. Naming the *ancestors* is what scopes a part:
  `IdCollect` folds on any element carrying an `Id`, interactive or not, so
  the table root needed a name and nothing more. And only the row and the head
  opt into being *found* by the path — a cell is `Id` and nothing else, here
  and upstream, and giving it a click id would have handed it the press the
  row is there to take. The count says it did not: 37 ids before and 37 after.
- Two pairs share an id on purpose and are worth knowing about: the story's
  search pill and the field inside it (the pill is what draws the focus ring,
  because the field is `Appearance(false)`), and a table head and the label
  inside it (hovering the label lights the whole head).

**Why the tree has not moved onto it wholesale.** A path id is only known once
the tree is built, and this tree asks for ids *during* the build in more places
than expected: `select` stashes `triggerFocusId` and `contentFocusId` on its
state, `menu` stashes `triggerId`, `popover` derives a `focusId`, three
widgets keep a `previousFocusId` to restore, `otp_input` asks
`WindowFocusedId(cx->win) == HashClickId(id)`, `DropdownOpen` is handed one,
the dock's drop markers compare `WindowDragOverId`, and the showcase's tooltip
and hover-card pages branch on `hoverId ==`. None of those can ask for a path.
GPUI does not have the problem because the equivalents are resolved at paint
against hitboxes and applied as style refinements — which is what `HoverBg`
already is here, and what the rest of them would have to become.

So the fold is in and tested, and adopting it is per-widget work that has to
carry its references with it. Converting a widget while something else still
computes `HashClickId` of the same name would put two schemes in one space,
which is worse than either. `component::Table` is the first one across, and it
was safe precisely because nothing outside it names its parts. The order for
the rest: move the paint-time questions — drag-over and hover — onto style
refinements the way `HoverBg` already is, which is what frees the widgets that
stash an id on their state, then convert from the leaves up.

A note on the other half of `HashClickId`'s callers: `KeyedEntity`,
`KeyedKey`, `MotionId`, `FocusTrapId` and `IndexPath` hash a name to key
*state*, not to identify an element. Rust keys element state by
`GlobalElementId` too, so those are path-shaped upstream; here they are looked
up while the tree is being built, which is before a path exists.


## The two questions a box should not have to ask

The blocker the fold ran into was that this tree asks about the pointer while
it *builds*: the dock's tab bar wrote `WindowDragOverId(cx) == HashClickId(id)`
and branched on the answer, which needs an id it can compute by hand and so
cannot ever be a path. GPUI does not have that shape. There a box declares
what it becomes and the answer is resolved later:

```rust
.drag_over::<DragPanel>(|this, _, _, cx| {
    this.border_l_2().border_color(cx.theme().drag_border)
})
```

`El::Hover(StateStyle)` and `El::DragOver(kind, StateStyle)` are that, and the
resolution sits where GPUI's does — `compute_style` during prepaint, which is
`PrepareEl` here, against the hover and the drag the last frame left. So unlike
`HoverBg`, which is one field baked into `Style`, these name any field a
refinement can; `StateStyle` is the builder, the same one semantic states are
written with. `PaintCtx` carries `dragOverId` and `dragKind` beside `hoverId`
now, which is the pair GPUI reads off the window in the same place.

`StyleField` gained one bit per border edge. `.border_l_2()` names the left and
leaves the other three, and a refinement that copied all four from a mostly
empty style would have cleared whatever the box already had on them —
`AnEdgeRefinementLeavesTheOtherEdgesAlone` is that case.

The dock's two markers are the first callers, and they are upstream's two:
`.drag_over::<DragPanel>(border_l_2, drag_border)` on a tab, and
`.drag_over::<DragPanel>(bg(tokens.drop_target))` on the empty run past the
last one. `DockTabDropOver` and `DockTabRestDropOver` are gone from base's API
— the marker is not a question anyone asks any more — and what replaces them
is `DockGroupDroppable`, which is upstream's `.when(droppable, ..)` and asks
nothing about the pointer. Both markers now use the tokens Rust uses
(`drag_border`, `tokens.drop_target`) rather than the primary they had been
borrowing, which shows only while a drag is in flight.

Three cases stay as they are, and each has a reason. The split handle is
handed an `active` flag because upstream hands its own skin
`ResizeHandleContext::is_active()` — that one is a query in Rust too. The
slider's thumb ring keys off hover to drive a spring and grow a child, which
is not a style refinement in either tree. And the showcase's tooltip and
hover-card pages branch on hover to render a *different subtree*; upstream
gives those an entity and an open state, which is a port of its own.

`tests/StateStyleTests.cpp` drives layout with a `PaintCtx` and reads the
resolved style back: a hover refinement holds only while hovered, a drag-over
one only for the right kind on the right element, and a box with no click id
of its own is never refined — it would otherwise match a `hoverId` of 0, which
is what "nothing is hovered" is spelled as. 18523 checks.


## A focus handle is not an element's name

The next thing in the way of the path was focus. Rust's is a `FocusHandle`: a
refcounted slotmap key made with `cx.focus_handle()`, owned by whatever holds
the state, attached to a box with `div().track_focus(&handle)`. It has nothing
to do with the element's name. Here every focusable box derived its focus id
from its own name with `HashClickId`, which is why a state that had to
remember one — a popover, a menu, a select — stashed a number it recomputed
from the same string every frame. The popover's line says the rest:

```cpp
p->focusId = HashClickId(id) * 31 + 1;
```

The `* 31 + 1` was there for one reason: to keep the popover's focus id clear
of the *click* id that same name produced. Two things forced into one number.

`FocusHandle` is now a type, `FocusHandleNew(cx)` is `cx.focus_handle()`, and
`El::TrackFocus(h)` is `track_focus`. Handles are allocated downwards from
-1000, so they cannot meet a hashed element id (positive) or the window chrome
(-1..-4) — no band, no arithmetic twist. There is no refcount and nothing is
given back: an int is cheap, and the state that owns the handle is what makes
it mean anything.

One change underneath made it work: `HitRect` carries the element's focus id
now, and a press focuses *that* rather than the id the box was hit as. Until
handles existed the two were always the same number, so nothing could tell;
a box tracking a handle is the first case where they differ.

Five states hold handles instead of numbers: `PopoverState` (its own and the
one it tracks), `PopupMenuState`, `SearchableListState` (the select's trigger
and content), and `OtpState` — whose "am I focused?" was
`WindowFocusedId(cx->win) == HashClickId(id)`, hashing the element's name a
second time to compare against it, and is now
`FocusHandleIsFocused(win, s->focus)`. Every `previousFocusId` became a
`previousFocus`, which is Rust's `previous_focus_handle`.

**One wart the conversion made visible rather than fixed.** A select parks
focus on its `contentFocus` while the list is up, but the list's box only
takes a focus id when it is *not* in a select (`if (!inSelect)` in
`searchable_list.cpp`) — so the handle names nothing in the tree, and focus
sits on an element that is not there. It behaves, because every question asked
of it is `win->focusId == handle`, which is true either way. With a number
that was invisible; with a handle it is a thing the list should be tracking.
Left alone here because giving the box a real focus id adds a tab stop inside
an open dropdown, and this session cannot drive a keyboard to see what that
does.

What is still keyed by name, and why: `menu`'s `triggerId` is a *click* id,
not a focus one — `PopupMenuState::OnPressOutside` hit-tests against it to
leave the trigger's own toggle alone — and the showcase's tooltip and
hover-card pages still ask `hoverId ==` to swap a subtree. Those are the two
shapes left before the widgets themselves can move onto the path.

## A page is told what the pointer is doing

Three showcase pages asked the window who was hovered and swapped a subtree on
the answer:

```cpp
if (app->hoverId == HashClickId(StrL("tooltip-trigger"))) { ... }
```

Rust never asks. The trigger carries `on_hover`, the handler writes state, and
the next frame reads the state — `self.tooltip_visible` in `tooltip.rs`. A
hover card goes further and does not even give the page a field: `HoverCard`
holds `window.use_keyed_state(self.id, ..)`, and the content is a closure that
runs only while the card is up.

**What the three became.** The tooltip page has a `TooltipHover` handler and a
`tooltipVisible` field. The hover-card page has neither: the base `HoverCard`
now makes its own state, keyed off its id, so `HoverCard::New(cx, id)` with no
entity is Rust's `use_keyed_state`, and the page asks `card->IsOpen()` before
it builds a thing — which is what the Rust closure's laziness amounts to. The
keying rule moved into `HoverCardStateFor` so the base card and the themed
skin cannot disagree about which state a card id means. And the color picker's
swatches preview through the `onHover` the swatch has always taken, writing
`colorPreview` / `colorHasPreview` — `ColorPickerState`'s `preview:
Option<Hsla>`, whose `displayed_color()` is `self.preview.or(self.value)`.

**One of the three was dead.** `SwatchId(i)` hashed `color-swatch-%d` while
the element it was looking for was built as `swatch-%d`, so the color picker's
hover preview had never once fired. Two names for one thing, each right on its
own. Confirmed by screenshot before and after: on the old build, hovering a
swatch with the picker open changes nothing.

**And porting the tooltip page faithfully found a runtime divergence.** Rust's
trigger is `div().id("tooltip-trigger").on_hover(..)` wrapped around a
`Button`, and in GPUI `hovered` is asked of an *element* — bounds containment
on the top layer — so the wrapper hears the pointer even though the button
inside it is what a hit test names. Here hover was the topmost hit rect and
nothing else, so a wrapper around anything hit-testable could never hover: the
ported page showed no tooltip at all. `WindowHoverChanged` now walks the chain
of enclosing hit rects — which `HitRect::parent` already recorded for the two
mouse phases — and fires `false` for the boxes the pointer left and `true` for
the ones it entered, each exactly once, outermost first. Two absolutely placed
siblings that overlap are not inside one another, so the chain is what bounds
containment would have found anyway.

`ShowcaseApp::hoverId` is gone, and with it the last place the showcase read
`win->hoverId`.

Pinned in `tests/HoverCardTests.cpp`: two card ids are two states and the same
id asked twice is one — the point of keying, since the tree that armed a
countdown is thrown away before it fires — and a close that lands while the
card is hovered does nothing, which is `schedule_close`'s closure asking
again. The hover chain itself has no unit-test seat (nothing in `tests/`
drives window input), so its evidence is a screenshot pair: the tooltip page
at rest and with the pointer on the trigger.

What is still keyed by name: `menu`'s `triggerId`, a *click* id that
`PopupMenuState::OnPressOutside` hit-tests against to leave the trigger's own
toggle alone. That is the last of the two shapes; after it the widgets
themselves can move onto the path.

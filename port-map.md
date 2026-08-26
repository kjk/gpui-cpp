# Base/UI structural fidelity map

`cmd/audit-port.ts` is the machine-readable source of truth for the mapping
from the pinned `crates/base` and `crates/ui` module trees into `src/base` and
`src/ui`. Run:

```
bun cmd/audit-port.ts
bun cmd/audit-port.ts -surface  # print every declaration/re-export/test mapping
bun cmd/audit-port.ts -missing-declarations # heuristic C++ spelling report
```

The audit always checks the pinned SHA and every declared C++ destination. If
`.work/gpui-component` exists it reads the complete Rust source trees, not
just `lib.rs`: all 762 module-level public declarations, all 272 `pub use`
statements and all 1,047 real tests are inventoried. Every item must belong to
a classified module, every tested module names existing C++ suites that own
its behavioral coverage, and stable content hashes fail when a declaration,
export or test is added, removed or renamed at the pinned checkout. (`#[test]`
in a doc comment is deliberately not a test.) This makes a pin update
introduce explicit surface and test-mapping decisions rather than silently
widening the gap. `-surface` prints the item-by-item status and destinations.

For every module marked full, the ordinary audit now also requires each
declaration's Rust spelling (or the direct PascalCase form of a free function)
in its C++ targets. The small `declarationMappings` table records intentional
placements and spellings such as `init` to `BaseInit`, `locale` to
`LocaleNow`, and the runtime-owned `AutoScroll`, as well as deliberate
collapses with a required reason. `-surface` prints those decisions next to
the Rust declaration, and `-missing-declarations` prints the unresolved names
in partial modules. Rust permits a public item under a private submodule, and
traits and functions frequently project into a C++ builder or function table,
so each of those results still needs an explicit mapping or
deliberate-collapse decision.

Statuses mean:

- `full`: no concrete structural or behavioral gap is currently known in the
  module surface used by upstream examples and stories. Every module-level
  public declaration has a direct C++ spelling or an explicit mapping, and
  all public re-export statements are inventoried.
- `partial`: a destination exists but its public surface, ownership, runtime
  placement, accessibility, or tests still differ.
- `adapter`: the repository's C++ runtime or hard dependency rules require a
  deliberately different implementation shape.
- `excluded`: a standing non-goal in `AGENTS.md`; currently only async utility
  plumbing.

Every non-full entry carries a reason in the ledger. Modules that assign
semantic accessibility roles upstream now project those roles, names, values,
states and actions into the runtime's laid-out frame tree; they are no longer
classified partial merely because keyboard behavior used to be their only
accessible seam. Native adapter depth remains runtime work outside the Base/UI
module ledger.

The important non-mechanical mappings are encoded in the audit:

| Rust module family | Canonical C++ surface | Implementation decision |
| --- | --- | --- |
| `base/dock/*` | `base/dock*`, `base/tiles*` | one Base dock family |
| `base/input/*` | `base/input*`, `base/input_keys*` | one Base input family |
| `ui/table/data_table` | `ui/data_table.h`, `ui/table*` | canonical UI include; shared behavior currently delegates to `base/data_table*` |
| `ui/list/*` | `ui/list*` | themed UI surface over shared `base/list*` behavior |
| `ui/menu/popup_menu` | `ui/popup_menu.h`, `ui/menu*` | canonical UI include over shared `base/popup_menu*` behavior |
| `ui/plot/shape/sankey` | `ui/sankey.h`, `ui/plot*` | canonical UI include over the dependency-free `base/sankey*` layout port |
| `ui/highlighter/*` | `ui/highlighter*`, `ui/syntax*` | one scanner-backed adapter |
| `ui/text/format/html` | `ui/text*`, `ui/html*` | one handwritten-parser adapter |
| serde-backed state | `base/json*` | private support, not a Base public module |

## Completed structural pass

- `base/lib.h` and `ui/lib.h` mirror the pinned Rust public module trees;
  forwarding headers preserve Rust's canonical UI include points where C++
  shares an implementation.
- `BaseInit(App*)` and `component::Init(App*)` mirror crate initialization and
  every example calls the UI initializer.
- GPUI-style application globals own the base/UI themes, theme registry, list
  settings, system-notification registry and application menu model.
- UI theme changes project semantic tokens, scrollbar mode/motion/styles and
  resizable colors into Base, and Base primitives consume that projection.
- Rust `Vec`-backed public builders and toast/notification storage no longer
  silently truncate at C++-only fixed capacities.
- Base input collection providers report their total result count and are
  retried into growing buffers. Completions, code actions, definitions,
  semantic tokens and edit lists no longer stop at port-only limits; document
  colors preserve Rust's explicit reject-above-10,000 behavior.
- The dependency-free HTML/TextView path has no content-size limits of its
  own: nesting, tag names, entities, plugins, marked/code runs and table
  shapes grow with the document. The supported HTML vocabulary remains a
  deliberate smaller substitute for html5ever.
- `History<I>` is generic over the C++ `HistoryItem` convention and matches
  upstream's two stacks, versions, grouping interval, explicit grouping,
  uniqueness, ignore state and 1,000-entry default retention. The audit
  records that `HistoryItem` is a POD-friendly item convention rather than an
  inherited C++ trait.
- `EffectTransition` is the composable arena builder corresponding to Rust's
  retained animation closure. It applies every upstream slide, fade and size
  effect to a freshly built element from keyed one-shot state, and keeps the
  deprecated `Transition` alias.
- Base `Popup` now imports the runtime's complete `gpui::Anchor` vocabulary,
  preserves Rust's first-frame trigger capture, exact corner arithmetic,
  eight-pixel viewport margin and deferred popup layer. Dropdowns remain on
  the distinct side-placement path that may flip.
- Base `Popover` owns the complete open lifecycle over that Popup host:
  controlled and default state, focus capture/restoration, deferred-context
  lifetime, Escape, per-element outside-press dismissal and a two-direction
  open-change callback. The themed facade forwards the same controls while
  retaining its older close-only callback as a compatibility adapter.
- Base `Positioner` again has the pinned public element shape rather than only
  free resolver functions: `Align`, `ResolvedPosition`, `Positioner`, and its
  doc-hidden `PositionerState` are direct C++ declarations. The arena builder
  measures an unbounded flex child group, supports corner and four-side
  strategies, and shares one resolver with runtime layout and the pure test
  API. Client-decorated window insets join the requested viewport margin.
- Base Slider's state, values, scale and `SliderEvent` live in the runtime
  because pointer, keyboard and accessibility dispatch all mutate the same
  object there. The audit records that placement explicitly; the Base
  Slider/Track/Indicator/Thumb elements are the public presentation layer.
- Base Radio, Toggle and Tab expose their pinned semantic style structs and
  resolve checked/pressed/selected before disabled over each instance style.
  Tabs and disabled toggles also stop the left mouse-down bubble through a
  hitbox flag, matching their callback-free Rust propagation handlers.
- Base Sheet owns its pinned host focus trap, transparent overlay capture,
  cutoff, Escape binding and request-then-notify close order. Its listeners
  persist in keyed state after the frame builder is consumed.
- Base Geometry's placement, side, axis and uniform-edge helpers cover the
  pinned extension vocabulary. The runtime owns the DIP-only `Edges` alias;
  the audit records the absent retained `Length` as a deliberate taffy seam.
- Base Focus Trap exposes the pinned container name over the runtime's
  in-place `El` refinement. Active membership is rebuilt per window and frame
  instead of retained in Rust's weak-handle global manager.
- UI Icon now carries the complete pinned 99-icon asset vocabulary and exact
  SVG files. Its POD `IconNamed` path value is the C++ trait seam; named and
  application-defined paths share one element, unspecified size follows the
  inherited text size before flex layout, and explicit semantic sizes, color
  and rotation match the Rust builder.
- UI Window Border now publishes the pinned `window_paddings` seam from stable
  decoration state. Dialog and Sheet fixed layers use its visible-frame origin
  and size, and Linux native resize hit testing uses the component's inset,
  tiling and configurable band rather than the outer X11 edge.
- UI WindowExt's complete behavior is available through `Window*`-prefixed
  free functions: owned dialog/sheet entities, alert-dialog convenience,
  typed notification removal, focused-input validation and the deprecated
  selection forwarders. The audit records the trait-to-free-function collapse.
- UI Description List retains Rust's `DescriptionText` variants and
  `DescriptionItem` builder, exact span grouping and separator rows, fractional
  cell bases, 1–10 column clamp, axis factories and pinned size/border rules.
- UI Label renders primary and secondary content as one styled run, preserving
  literal spacing, cross-boundary and overlapping highlights, prefix/full match
  variants, inherited typography and Unicode bullet masking without a cap.
- UI Group Box exposes the Normal/Fill/Outline variant vocabulary and exact
  string conversions, independent root/title/content refinements, arbitrary
  children, optional element titles and the pinned wrapper/content structure.
- UI Sizing's `UiSize` is the POD payload projection of Rust `Size`, including
  custom pixels, parsing, stepping and inverted min/max semantics; every
  `StyleSized` transform is available as an exact free `El` refinement.
- UI Chart exposes the pinned `RadarLabel` text/element variant and positions
  naturally measured element labels from the laid-out plot bounds. Sankey
  labels are arbitrary styled line lists through the pinned `SankeyLabel`
  value, including per-line color, font size and variable line height.
- UI Sheet carries `SheetSettings` in the active Theme and exposes the pinned
  title/footer/child, overlay-close, resizable and style-refinement surface;
  Base Sheet remains the owner of focus and modal dismissal ordering.
- UI Tab preserves the source split between an independently buildable `Tab`
  and `TabBar` (with `Tabs` retained as a compatibility alias). Tabs accept
  arbitrary prefix/content/suffix children, local callbacks and refinements;
  the bar owns callback precedence, viewport scrolling, overflow, selection
  motion, suffix spacing and the source theme tokens.
- text-selection suppression and notification visibility now follow the Rust
  event/lifecycle order instead of merely matching a static rendering.
- the runtime builds a semantic tree after layout, skipping visual-only boxes
  while preserving semantic ancestry. Base/UI controls project the pinned
  AccessKit roles and aria fields; default, focus, slider increment/decrement,
  spinbutton increment/decrement and editable SetValue actions route through
  the same state/listener paths as pointer and keyboard interaction.
- input content types project the same specialized phone, email, URL,
  password, date and date-time roles as Rust, secret values stay out of the
  tree, and developer accessibility ids remain distinct from element ids.
- Windows publishes that live tree through a raw UI Automation fragment
  provider reached from `WM_GETOBJECT`. Roles, names, bounds, focus and
  Invoke, Toggle, Value, RangeValue, ExpandCollapse, SelectionItem, Grid,
  GridItem, Table and TableItem patterns resolve current frame nodes; semantic
  and focus changes raise native events.
- the audit inventories every module-level public declaration, public
  re-export and upstream test below module granularity. Base currently
  contributes 359 declarations, 113 re-export statements and 569 tests across
  46 tested module families; UI contributes 403, 159 and 478 across 38. Test
  destinations are validated even when CI deliberately omits the Rust
  reference checkout, while the pinned content hashes are checked whenever
  that checkout is present.

## Next fidelity order

  1. Export the portable accessibility tree through Linux AT-SPI and macOS
     NSAccessibility, then deepen Windows with Text and selection-container
     patterns. Windows already has the core fragment/action and table export;
     this remaining work is in GPUI platform adapters and does not change
     Base/UI semantics.
  2. Review the 329 declaration spellings still reported in partial modules,
     adding explicit mappings where Rust traits or snake_case functions
     project into C++ builders, recording private-submodule collapses, and
     implementing the genuine omissions before promoting a module to full.

  The theme layering item is complete: `src/ui/theme.h` owns the component
  palette, `src/base/theme.h` owns Base's semantic/behavior theme, and GPUI
  consumes only the projected `RuntimeStyle` fields its renderer needs.

`port-progress.md` remains the detailed behavioral log while the declaration
inventory and spelling report expose the next symbol-level work.

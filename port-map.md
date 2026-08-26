# Base/UI structural fidelity map

`cmd/audit-port.ts` is the machine-readable source of truth for the mapping
from the pinned `crates/base` and `crates/ui` module trees into `src/base` and
`src/ui`. Run:

```
bun cmd/audit-port.ts
```

The audit always checks the pinned SHA and every declared C++ destination. If
`.work/gpui-component` exists it also reads both Rust `lib.rs` files and fails
when an upstream module is not classified. This makes a pin update introduce
an explicit mapping decision rather than silently widening the gap.

Statuses mean:

- `full`: no concrete structural or behavioral gap is currently known in the
  module surface used by upstream examples and stories. This is not a claim
  that every Rust symbol has already been checked.
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
accessible seam. Native UI Automation, AT-SPI and NSAccessibility adapters
remain runtime work outside the Base/UI module ledger.

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
  uniqueness, ignore state and 1,000-entry default retention.
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

## Next fidelity order

  1. Extend `cmd/audit-port.ts` below module granularity: inventory every
     upstream `pub use` and test, requiring `full`, `adapter` or `excluded` plus
     a C++ symbol/test destination.
  2. Export the portable accessibility tree through UI Automation, AT-SPI and
     NSAccessibility. This is GPUI platform-adapter work; Base/UI semantics no
     longer depend on it.

  The theme layering item is complete: `src/ui/theme.h` owns the component
  palette, `src/base/theme.h` owns Base's semantic/behavior theme, and GPUI
  consumes only the projected `RuntimeStyle` fields its renderer needs.

`port-progress.md` remains the detailed behavioral log until item 3 makes the
ledger symbol-complete.

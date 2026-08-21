# src/taffy — the taffy layout crate, ported to C++

This is a port of [taffy](https://github.com/DioxusLabs/taffy) **0.12.2**, the
layout crate Zed's GPUI uses and therefore the one gpui-component's layout is
defined by. Everything in `src/gpui` that has a box lays it out through here.

The pinned version is the one `gpui-component`'s `Cargo.lock` resolves for
`gpui`, which asks for `taffy = "=0.12.2"`. The pin lives in
[`cmd/versions.ts`](../../cmd/versions.ts) (`taffy`) alongside the
gpui-component and Zed GPUI pins, and moves when they do — see
[`port-upstream.md`](../../port-upstream.md).

## Where the Rust went

| Rust                                | C++                     |
| ----------------------------------- | ----------------------- |
| `src/geometry.rs`                   | `geometry.h`            |
| `src/util/sys.rs` (the f32 helpers) | `geometry.h`            |
| `src/util/math.rs`                  | `math.h`                |
| `src/util/resolve.rs`               | `style.h` / `style.cpp` |
| `src/style/*.rs`                    | `style.h` / `style.cpp` |
| `src/tree/{node,layout,cache}.rs`   | `tree.h` / `tree.cpp`   |
| `src/tree/{traits,taffy_tree}.rs`   | `taffy_tree.h` / `.cpp` |
| `src/compute/{mod,leaf,common}.rs`  | `compute.h` / `compute.cpp` |
| `src/compute/flexbox.rs`            | `compute_flexbox.cpp`   |
| `src/compute/{block,float}.rs`      | `compute_block.cpp`     |
| `src/compute/grid/**`               | `compute_grid.cpp`      |

Everything is in `namespace taffy` — not `gpui` — because it is a port of a
crate of its own rather than part of gpui-component. `gpui::Style` and
`taffy::Style` both exist and mean different things; so do `Overflow`,
`Position` and `Display`.

## What is ported

The default feature set of the crate as gpui builds it: `std`, `taffy_tree`,
`flexbox`, `grid`, `block_layout`, `float_layout`, `calc`, `content_size`.

Not ported, and not planned:

- `parse` — the CSS parser, which needs `cssparser`. Styles here are built by
  the caller, never parsed.
- `serde` — nothing serialises a style.
- `detailed_layout_info` — an accessor for the computed grid track sizes.
  Nothing in this tree reads it.

## Deliberate differences

Each of these is also stated in a comment at the place it applies.

- **No traits.** Rust's `LayoutPartialTree`, `CacheTree`,
  `LayoutFlexboxContainer`, `GridContainerStyle` and the rest exist so a caller
  can bring its own tree and style types. There is one of each here, so the
  compute pass takes a `TaffyTree*` and reads `Style` fields directly. The
  method names are the trait method names.
- **No templates on the containers.** `Size<T>`, `Point<T>`, `Rect<T>` and
  `Line<T>` become one struct per element type the algorithms actually carry:
  `SizeF`, `SizeOptF`, `SizeDim`, `SizeAvail`, `SizeLp`, `RectF`, `RectOptF`,
  `RectLp`, `RectLpa`, `PointF`, `PointOptF`, `PointOverflow`, `LineF`,
  `LineBool`, `LineOzl`, `LinePlacement`, `LinePlain`. `Option<f32>` is `Optf`,
  and the optional enums get one struct each. The only template left is
  `Slice<T>`.
- **`Style` owns nothing.** Its grid track lists are arena-backed `Slice<T>`
  rather than `Vec<T>`, and a custom ident is a `Str` pointing into that arena.
  A `Style` therefore copies as bytes; whoever built it owns the arena.
- **No `Result`.** The three calls that can be handed an out-of-range child
  index return `bool`; everything else takes a live node by contract, which is
  what Rust's `.expect()`-ing callers already assume.
- **`TaffyView` is gone.** It exists in Rust only to keep the measure closure
  out of the tree's borrow. The measure function is a field on the tree here,
  set for the duration of a `ComputeLayoutWithMeasure` call.
- **`NodeContext` is a `void*`**, handed back to the measure function
  untouched, rather than a type parameter.

## Tests

`tests/TaffyTests.cpp` ports every `#[cfg(test)]` module in the crate that
pins behaviour rather than Rust specifics — `util/math.rs`, `util/resolve.rs`,
`style/alignment.rs`, `style/flex.rs`, `style/mod.rs`, `compute/mod.rs`,
`tree/taffy_tree.rs`, and the grid's `explicit_grid.rs`, `implicit_grid.rs` and
`placement.rs` — plus end-to-end checks of flexbox, block and grid layout.

The crate's larger generated suite lives in its `tests/` directory, which is
not part of the published crate (its `Cargo.toml` `include` covers only `src/`
and `examples/`), so it takes the git checkout `port-upstream.md` clones.

Three grid internals are reached through the seams `GridExplicitSizeForTest`,
`GridChildMinMaxSpanForTest`, `GridSizeEstimateForTest`,
`GridInitTracksForTest` and `GridPlaceForTest` in `compute.h`. They exist for
the tests and have no other caller.

## Benchmarks

`bench/` ports the crate's own benchmarks — `benches/benches/flexbox.rs`,
`grid.rs` and `tree_creation.rs`, and the tree builders in `benches/src/`.
Run them with `bun cmd/bench.ts`. They come from the same checkout as the
generated tests, for the same reason.

Not ported: `benches/benches/mixed.rs`, which measures text leaves through
`parley`. Its shape — flex and grid containers alternating, with real text at
the leaves — is worth having, since measured leaves are where a real window
spends its layout time, but with our own text measure substituted it would be
a new benchmark rather than a port of that one.

The random source is not Rust's. The benchmarks draw tree shapes from a
ChaCha8 stream seeded 12345; this uses a PCG32 with the same seed, so a run is
reproducible against itself but its trees are not the Rust's trees. Matching
them would mean porting `rand`'s uniform sampling as well, and the numbers
would still be a different machine's.

## Refreshing the port

When `cmd/versions.ts` moves to a gpui-component whose `Cargo.lock` resolves a
different taffy, bump `taffy.version` there too and diff the crate:

```
git -C <a taffy checkout> log --oneline v0.12.2..vNEW -- src
```

The C++ file that owns each Rust file is in the table above, and every function
keeps its Rust name in CamelCase, so a diff maps across mechanically.

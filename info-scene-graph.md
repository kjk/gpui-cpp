# Scene graph: scope, value and next steps

This note compares the scene support in this tree with Zed GPUI at the pinned
`zedGpui` revision (`f66ed399cdde86092af8af3dc7b418abf45f37f8`). The short
answer is that porting Zed's scene graph as a whole is not currently worth the
cost. The existing recorder has already captured the useful result for the UI
this tree draws: visibility culling, persistent Direct2D path geometry and
whole-frame comparison.

The decision should be revisited if this tree starts drawing a Zed-sized editor
with many independently dirty view entities, gains custom GPU renderers outside
Windows, or needs renderer primitives such as native surfaces and transformed
atlas sprites.

## What is here

`src/gpui/scene.cpp` records the portable `Paint.h` calls into a flat primitive
array. Each primitive carries its content mask and numeric layer. At the end of
the frame the recorder sorts the primitives, hashes and compares the frame,
culls empty or out-of-damage primitives, and replays through `Paint.h`. Paths
are retained as verbs and points, with a Direct2D geometry cache keyed by their
hash.

Windows uses the recorder for both Direct2D and the custom D3D11/D3D12 renderer.
Linux, macOS and wasm still issue `Paint.h` calls directly. The custom Windows
renderer already builds one instance buffer and batches the frame, independently
of whether the scene recorder is enabled.

The current scene state is process-global. That is unsafe for multiple windows:
the previous frame, skip decision, damage history and path cache do not belong
to the window whose surface they describe.

## What upstream has in addition

Upstream `scene.rs` has separate arrays for shadows, quads, paths, underlines,
monochrome sprites, subpixel sprites, polychrome sprites and native surfaces.
It records `PaintOperation`s, uses a `BoundsTree` to assign draw order to
overlapping paint layers, sorts each primitive stream, and merges the streams
into renderer batches.

More importantly, upstream scene replay is part of view retention rather than
an isolated drawing optimization. A cached view replays a range of scene
operations together with its:

- hitboxes and mouse listeners;
- input handlers and cursor requests;
- tab stops;
- shaped text layouts;
- deferred draws;
- element state and accessed-entity dependencies.

This tree rebuilds its frame-arena `El` tree, hit testing, focus and
accessibility state on every invalidated frame. Its persistent layout cache
avoids repeating unchanged Taffy work, but it has no equivalent of upstream's
cached prepaint and paint ranges. Copying `scene.rs` and `bounds_tree.rs` would
therefore port the data structures without providing upstream's retained-view
behavior.

## Cost of a full port

A faithful end-to-end port would require:

1. Typed primitive buffers, paint operations, a layer stack, `BoundsTree`,
   batch iteration, and their unit tests.
2. Per-window previous and next scenes, explicit retained resource ownership,
   resize and device-loss handling.
3. Cached view state and replay ranges spanning prepaint, paint, listeners,
   hitboxes, tab stops, text layout and entity dependency tracking.
4. Stable cross-frame handles for text, images and atlas entries. Frame-arena
   pointers cannot be retained.
5. Renderer support for shadows, quads, paths, underlines, the sprite classes
   and surfaces. A faithful GPU path would also need its shader and atlas
   machinery on every platform on which parity is promised.
6. Ordering, replay, multi-window, resource-lifetime and device-loss tests,
   real interaction benchmarks, and cross-platform visual comparisons.

For one experienced maintainer, the likely order of work is 12--20 engineer
weeks: 1--2 for the core scene, 2--3 for window/resource ownership, 3--5 for
retained view integration, 4--8 for renderer/platform parity, and 2--4 for
validation and tuning. Some phases overlap. A collection-only port that keeps
replaying through `Paint.h` is closer to 3--5 weeks, but supplies little that
the current recorder does not already do.

## Measured value

The release measurements in `src/gpui/scene.h` show:

- On the story gallery, Direct2D paint falls from 1.46 ms with the scene off to
  0.86 ms with collection and replay, then to 0.76 ms with path caching.
- The main win is culling: 799 of 1,071 recorded story primitives are already
  clipped out. Reordering provides no measurable benefit.
- Identical-frame skipping makes repeated-frame benchmarks much faster, but an
  idle application normally requests no frame in the first place.
- On the continuously changing FPS monitor, Direct2D rises from 0.82 ms with
  the scene off to 0.91 ms with replay and 1.00 ms with damage. Collection and
  hashing are overhead when reuse is unavailable.
- With the scene disabled, the custom D3D11 renderer already takes story paint
  from Direct2D's 1.625 ms to 0.582 ms. The renderer, not a fuller scene model,
  owns that batching gain.

Retained-view replay could reduce build and prepaint work that these paint-only
numbers do not measure. Its opportunity is limited by this tree's deliberately
coarse view structure: components normally build `El`s inside one screen or
page entity. If that entity becomes dirty, there are few independently clean
subviews to replay; if nothing is dirty, the window already does not redraw.

## Decision

Keep "Zed's scene graph as a whole" as a non-goal. Do not replace the current
recorder with upstream's typed scene until a concrete feature or whole-frame
profile requires it.

The worthwhile near-term work is narrower:

1. Move all scene state to the window/paint context so two windows cannot share
   frame comparison, damage or cache state.
2. Give retained paint resources stable generation-based identities instead of
   hashing their addresses.
3. Benchmark real scrolling, hover, caret, popup and chart-tick invalidations.
   Keep damage mode only if those workloads demonstrate a useful total-frame
   improvement without stale output.
4. If path construction remains material, key paths relative to their origin
   and draw cached geometry with a translation.
5. Enable the recorder on Linux, macOS and wasm only after measuring its cost
   on those backends.
6. Port `BoundsTree` and typed batches only when an ordering problem or a new
   renderer needs them.

The first three items are implementation work following this analysis. They do
not commit the project to the full retained-scene architecture.

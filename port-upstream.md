# Upstream pins

**Source of truth for which checkin we are porting: [`cmd/versions.ts`](cmd/versions.ts)** (`gpuiComponent.sha`, `zedGpui.sha`, `taffy.version`, `markdown.version`, `wry.version`). `bun cmd/build.ts`, `bun cmd/run.ts`, and `bun cmd/versions.ts` clone or reset `.work/gpui-component` to that SHA.

This file is the ingest playbook. Diff Rust from the SHA in `cmd/versions.ts`, not `HEAD`.

When bumping a pin: change `gpuiComponent.sha` (and `zedGpui` if `Cargo.lock` moved) in `cmd/versions.ts`, run `bun cmd/versions.ts`, `git log OLD..NEW` on the crates we ported, apply the C++ equivalent.

## gpui-component (what we port)

See `gpuiComponent` in `cmd/versions.ts` for repo, SHA, date, subject, and crate versions.

| | |
| --- | --- |
| Repo | `gpuiComponent.repo` |
| Local clone | `gpuiComponent.dir` (gitignored) |
| Commit | `gpuiComponent.sha` |

Trees we actually translate:

- `crates/base` → `src/base/`
- `crates/ui` → `src/ui/`
- `crates/story` → `examples/story/`
- `crates/base/examples/showcase` → `examples/showcase/`
- `examples/*` → `examples/*.cpp`

A file under `src/base/` or `src/ui/` is named after the Rust module it ports,
so the map is mechanical in both directions:

- a file: `crates/base/src/actions.rs` → `src/base/actions.cpp`
- a directory: `crates/base/src/input/` → `src/base/input.cpp`

A Rust directory is one C++ file, however many modules it holds; `lib.rs` is
`lib.h`, the umbrella header. Where we have code the crate has no module for,
it takes the name of the nearest one (`element_ext.h`, `sizing.h`).

One upstream file is checked in verbatim rather than translated:
`README.md` → `assets/story/README.md`, which is what the Introduction page
renders (`markdown(include_str!("README.md"))` in `welcome_story.rs`). Copy it
again when the pin moves: `cp .work/gpui-component/README.md assets/story/`.

Ingest a newer checkin:

```
bun cmd/versions.ts
cd .work/gpui-component
git fetch origin
git log --oneline <gpuiComponent.sha>..origin/main -- crates/base crates/ui crates/story crates/base/examples/showcase examples
git diff <gpuiComponent.sha> origin/main -- <path>
```

## Zed GPUI (reference only — not a crate we port)

`Cargo.lock` pins GPUI from Zed. Fields live in `zedGpui` in `cmd/versions.ts`. Read that snapshot when matching runtime behavior (text measure cache, DirectWrite shaping, window). Do **not** treat later Zed `main` as the spec.

Local cargo checkout (after a rust build): `%USERPROFILE%\.cargo\git\checkouts\zed-*\<zedGpui.sha prefix>\`

We reimplement a Win32 + D2D + DWrite subset in `src/gpui/`. Not ported: Blade, entity/observer, cosmic-text, font-kit. Taffy, `markdown` and `wry` *are* ported — see below.

## taffy (a crate we port)

`src/taffy/` is a C++ port of [taffy](https://github.com/DioxusLabs/taffy) at
the version `gpui-component`'s `Cargo.lock` resolves for `gpui` — currently the
`=0.12.2` that `crates/gpui/Cargo.toml` asks for. It is the layout engine, not
a reference: every box in this tree goes through it.

**It moves when the gpui-component pin moves.** After bumping
`gpuiComponent.sha`, check whether the resolved taffy changed:

```
grep -A3 'name = "taffy"' .work/gpui-component/Cargo.lock
```

If it did, set `taffy.version` in `cmd/versions.ts` to the new one and diff the
crate between the two versions:

```
git clone https://github.com/DioxusLabs/taffy .work/taffy   # once
git -C .work/taffy log --oneline v0.12.2..vNEW -- src
git -C .work/taffy diff v0.12.2 vNEW -- src/compute/flexbox.rs
```

`src/taffy/readme.md` has the file-for-file map from the Rust modules to the
C++ files, and every ported function keeps its Rust name in CamelCase, so a
diff applies mechanically. The crate's own unit tests are ported in
`tests/TaffyTests.cpp`; a version bump that changes behaviour should show up
there first.

Two things we port are in the repository but not in the published crate, whose
`include` covers only `src/` and `examples/`: the `#[cfg(test)]` modules'
larger generated suite in `tests/`, and the benchmarks in `benches/`. Both
need the clone above. `bench/` is the port of `benches/benches/flexbox.rs`,
`grid.rs` and `tree_creation.rs` plus the tree builders in `benches/src/`;
`benches/benches/mixed.rs` is not ported, because it measures text through
`parley`.

## markdown (a crate we port)

`src/markdown/` is a C++ port of
[markdown-rs](https://github.com/wooorm/markdown-rs) at the version
`crates/ui/Cargo.toml` asks for — currently `markdown = { version = "1.0.0",
features = ["serde"] }`. It is the parser, not a reference: every
`component::TextView` in this tree reads its mdast, the way
`crates/ui/src/text/format/markdown.rs` reads the crate's.

**It moves when the gpui-component pin moves.** After bumping
`gpuiComponent.sha`, check whether the resolved `markdown` changed:

```
grep -A3 'name = "markdown"' .work/gpui-component/Cargo.lock
```

If it did, set `markdown.version` in `cmd/versions.ts` to the new one and diff
the crate between the two versions:

```
git clone https://github.com/wooorm/markdown-rs .work/markdown-rs   # once
git -C .work/markdown-rs log --oneline 1.0.0..NEW -- src
git -C .work/markdown-rs diff 1.0.0 NEW -- src/construct/gfm_table.rs
```

`src/markdown/readme.md` has the file-for-file map from the Rust modules to
the C++ files, and every state function keeps the name the crate's `StateName`
enum gives it, so a diff applies mechanically.

As with taffy, the crate's own test suite is in its `tests/` directory, which
the published crate does not carry (`include` covers `src/` only); it needs the
clone above. `tests/MarkdownTests.cpp` ports the `#[cfg(test)]` modules inside
`src/` and adds an end-to-end check per construct. The readme records the
differential run the port was checked with — 3283 documents, both parsers'
event streams and trees compared — and how to repeat it.

MDX and `to_html` are not ported, for reasons the readme gives.

## wry (a crate we port)

`src/wry/` is a C++ port of [wry](https://github.com/tauri-apps/wry) at the
version `crates/webview/Cargo.toml` asks for — currently `wry = { version =
"0.53.3", package = "lb-wry" }`, longbridge's fork of the crate. It is the
webview, not a reference: `src/webview/` is the port of `crates/webview`
itself and drives this one exactly as `gpui-wry` drives the crate.

**It moves when the gpui-component pin moves.** After bumping
`gpuiComponent.sha`, check whether the resolved `lb-wry` changed:

```
grep -A3 'name = "lb-wry"' .work/gpui-component/Cargo.lock
```

If it did, set `wry.version` in `cmd/versions.ts` to the new one and diff the
crate between the two versions. `lb-wry` is published from a fork, so the
crate tarball is the thing to compare rather than a git tag:

```
curl -sL -o .work/lb-wry.crate https://static.crates.io/crates/lb-wry/lb-wry-NEW.crate
tar xzf .work/lb-wry.crate -C .work            # unpacks lb-wry-NEW/
diff -ru .work/wry/src .work/lb-wry-NEW/src
```

`src/wry/readme.md` has the file-for-file map, and — more to the point when
reading a diff — the list of what is deliberately not ported (cookies,
downloads, drag and drop, the `NewWindowResponse::Create` arm, Android and
iOS) so a change to one of those needs no work here.

**Only Windows has a backend.** `src/wry/wry_win.cpp` is
`src/webview2/mod.rs`; the other three files are stubs that answer "there is
no webview here", and each says what a real one would take. A wry release
that only touches `wkwebview/` or `webkitgtk/` changes nothing in this tree.

The WebView2 declaration block in `wry_win.cpp` is transcribed from the SDK
header and is the one thing a wry bump never touches — it moves when the
*SDK* does, and only to reach an interface we do not already declare.

## Not ported (do not pin / do not chase)

`sysinfo`, `battery`, `smol`, `reqwest` (zed fork), ropey, tree-sitter, syntect, html5ever, resvg — C++ uses Win32 / our own code instead.

taffy's own transitive dependencies — `arrayvec`, `grid`, `slotmap`,
`cssparser` — are not ported either; the C++ port uses `Vec`, a flat occupancy
matrix, its own generational slots, and no CSS parser. `markdown`'s one
dependency, `unicode-id`, belongs to MDX, which is not ported.

wry's own dependencies are not ported either: `webview2-com` and
`webview2-com-sys` (the SDK bindings and Microsoft's loader) are written out
in `wry_win.cpp` instead, `http` and `cookie` are a pair of small structs and
a feature we skipped, and `raw-window-handle` is one `void*`.

`src/base.h` / `src/base.cpp` are SumatraPDF, not gpui-component.

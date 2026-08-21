# Upstream pins

**Source of truth for which checkin we are porting: [`cmd/versions.ts`](cmd/versions.ts)** (`gpuiComponent.sha`, `zedGpui.sha`, `taffy.version`). `bun cmd/build.ts`, `bun cmd/run.ts`, and `bun cmd/versions.ts` clone or reset `.work/gpui-component` to that SHA.

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

We reimplement a Win32 + D2D + DWrite subset in `src/gpui/`. Not ported: Blade, entity/observer, cosmic-text, font-kit. Taffy *is* ported — see below.

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

## Not ported (do not pin / do not chase)

`sysinfo`, `battery`, `smol`, `reqwest` (zed fork), ropey, tree-sitter, syntect, html5ever, resvg — C++ uses Win32 / our own code instead.

taffy's own transitive dependencies — `arrayvec`, `grid`, `slotmap`,
`cssparser` — are not ported either; the C++ port uses `Vec`, a flat occupancy
matrix, its own generational slots, and no CSS parser.

`src/base.h` / `src/base.cpp` are SumatraPDF, not gpui-component.

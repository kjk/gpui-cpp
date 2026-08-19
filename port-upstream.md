# Upstream pins

**Source of truth for which checkin we are porting: [`cmd/versions.ts`](cmd/versions.ts)** (`gpuiComponent.sha`, `zedGpui.sha`). `bun cmd/build.ts`, `bun cmd/run.ts`, and `bun cmd/versions.ts` clone or reset `.work/gpui-component` to that SHA.

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

We reimplement a Win32 + D2D + DWrite subset in `src/gpui/`. Not ported: Taffy, Blade, entity/observer, cosmic-text, font-kit.

## Not ported (do not pin / do not chase)

`sysinfo`, `battery`, `smol`, `reqwest` (zed fork), ropey, tree-sitter, syntect, html5ever, resvg — C++ uses Win32 / our own code instead.

`src/base.h` / `src/base.cpp` are SumatraPDF, not gpui-component.

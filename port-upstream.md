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

- `crates/base` → `src/ui/`
- `crates/ui` → `src/component/`
- `crates/story` → `src/examples/story/`
- `crates/base/examples/showcase` → `src/examples/showcase/`
- `examples/*` → `src/examples/*.cpp`

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

`src/base/` is SumatraPDF, not gpui-component.

# Upstream pins

Baseline for porting **future** `gpui-component` changes. Diff Rust from these SHAs, not `HEAD`.

When bumping a pin: `git fetch` in `.work/gpui-component`, `git log OLD..NEW` on the crates we ported, apply the C++ equivalent, then update this file.

## gpui-component (what we port)

| | |
| --- | --- |
| Repo | https://github.com/longbridge/gpui-component |
| Local clone | `.work/gpui-component/` (gitignored) |
| Commit | `da4f93696dc2b2b4d91bcc42412b9053a3d24de8` |
| Date | 2026-08-16 |
| Subject | docs: Fix EditorState::new() API usage in editor documentation (#2739) |
| Crate versions | `gpui-base` 0.5.2, `gpui-component` 0.5.2 |

Trees we actually translate:

- `crates/base` → `src/ui/`
- `crates/ui` → `src/component/`
- `crates/story` → `src/examples/story/`
- `crates/base/examples/showcase` → `src/examples/showcase/`
- `examples/*` → `src/examples/*.cpp`

Ingest a newer checkin:

```
cd .work/gpui-component
git fetch origin
git log --oneline da4f93696dc2..origin/main -- crates/base crates/ui crates/story crates/base/examples/showcase examples
git diff da4f93696dc2b2b4d91bcc42412b9053a3d24de8 origin/main -- <path>
```

## Zed GPUI (reference only — not a crate we port)

`Cargo.lock` pins GPUI from Zed. Read that snapshot when matching runtime behavior (text measure cache, DirectWrite shaping, window). Do **not** treat later Zed `main` as the spec.

| | |
| --- | --- |
| Repo | https://github.com/zed-industries/zed |
| Commit | `cc053a4a6fa2fd0e8793201ed9099466af1be0b1` |
| Date | 2026-08-07 |
| Subject | gpui: Expose accessibility identifiers to platform clients (#61926) |
| Crates | `gpui` 0.2.2, `gpui_platform`, `gpui_macros` |
| Lock line | `git+https://github.com/zed-industries/zed#cc053a4a6fa2fd0e8793201ed9099466af1be0b1` |
| Local checkout | `%USERPROFILE%\.cargo\git\checkouts\zed-a70e2ad075855582\cc053a4\` |

We reimplement a Win32 + D2D + DWrite subset in `src/gpui/`. Not ported: Taffy, Blade, entity/observer, cosmic-text, font-kit.

## Not ported (do not pin / do not chase)

`sysinfo`, `battery`, `smol`, `reqwest` (zed fork), ropey, tree-sitter, syntect, html5ever, resvg — C++ uses Win32 / our own code instead.

`src/base/` is SumatraPDF, not gpui-component.

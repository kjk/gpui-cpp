# gpui2

A **C++ / Windows** port of [longbridge/gpui-component](https://github.com/longbridge/gpui-component), a Rust UI kit built on [Zed GPUI](https://github.com/zed-industries/zed).

Original project:

- Repository: https://github.com/longbridge/gpui-component
- Docs: https://longbridge.github.io/gpui-component

This tree reimplements the component examples and a small Win32 + Direct2D + DirectWrite runtime. It is not a binding to the Rust crates and does not use Taffy, Blade, or Zed’s entity system.

The Rust sources used as the spec live in a gitignored clone at `.work/gpui-component/`. Pinned checkins are in [port-upstream.md](port-upstream.md).

## Build

Windows, MSVC `cl.exe` on PATH, [Bun](https://bun.sh):

```
bun cmd/build.ts -rel story
bun cmd/run.ts -rel -compare story
```

`bun cmd/build.ts` with no example name lists targets (`system_monitor`, `showcase`, `story`, …).

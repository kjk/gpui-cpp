# gpui for C++

A **C++** port of [longbridge/gpui-component](https://github.com/longbridge/gpui-component), a Rust UI kit built on [Zed GPUI](https://github.com/zed-industries/zed). Runs on **Windows** and **Linux**.

Original project:

- Repository: https://github.com/longbridge/gpui-component
- Docs: https://longbridge.github.io/gpui-component

This tree reimplements the component examples and a small runtime on top of the OS: Win32 + Direct2D + DirectWrite on Windows, X11 + cairo + Pango on Linux. Everything above the `Paint.h` / `Platform.h` seam is shared. It is not a binding to the Rust crates and does not use Taffy, Blade, or Zed’s renderer.

The API follows GPUI's shape: an `App` owns the entity store and the windows, a `Window` renders a view, and a view is a struct with state plus `static El* Render(T* self, Ctx* cx)`:

```cpp
struct Example {
    static void OnGo(Example*, Ctx*, const ClickEvent*) { log(StrL("Clicked!")); }

    static El* Render(Example*, Ctx* cx) {
        return Div(cx->a)->FlexCol()->SizeFull()->ItemsCenter()->JustifyCenter()
            ->Child(TextEl(cx->a, StrL("Hello, World!")))
            ->Child(ButtonEl(cx->a, 0, StrL("Let's Go!"), BtnKind::Primary)
                        ->OnClick(Listen(cx, &Example::OnGo)));
    }
};

int GpuiMain(int argc, char** argv) {
    App* app = AppNew();
    return AppRunView(StrL("Hello World"), 800, 600,
                      EntityNew<Example>(app).id, app, WinOpts{});
}
```

Entities are generational handles owned by `App`, not refcounted; `cx.listener` becomes `Listen(cx, &T::Handler)` and `cx.notify()` becomes `Notify(cx)`. See the *App, Window, Entity, Ctx* section of [AGENTS.md](AGENTS.md).

The Rust sources used as the spec live in a gitignored clone at `.work/gpui-component/`. Exact checkins we are porting are in [`cmd/versions.ts`](cmd/versions.ts); `bun cmd/build.ts` installs that tree. Ingest playbook: [port-upstream.md](port-upstream.md).

## Build

`bun cmd/build.ts` and `bun cmd/run.ts` dispatch to the toolchain for the
machine they run on, so the same commands work on both platforms:

```
bun cmd/build.ts -rel story
bun cmd/run.ts -rel -compare story
```

`bun cmd/build.ts` with no example name lists targets (`system_monitor`, `showcase`, `story`, …).

**Windows** needs MSVC `cl.exe` on PATH and [Bun](https://bun.sh).

**Linux** needs g++ (or clang++), pkg-config and the X11 / cairo / pango dev
packages. On Ubuntu or Debian:

```
bash cmd/ubuntu-install-deps.sh
```

**macOS** needs the Xcode command line tools (`xcode-select --install`).

From a Windows checkout you can build and run the Linux binaries under WSL
without leaving the shell, and compile the macOS ones on a Mac over ssh:

```
bun cmd/wsl-run.ts -rel system_monitor
bun cmd/mac-build.ts -rel -all
```

CI compiles every example on all three platforms on each push
([`.github/workflows/build.yml`](.github/workflows/build.yml)).

# Why port to C++?

- Do you know a good joke?
- Yes, Rust.

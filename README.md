# gpui for C++

A **C++** port of [longbridge/gpui-component](https://github.com/longbridge/gpui-component), a Rust UI kit built on [Zed GPUI](https://github.com/zed-industries/zed). Runs on **Windows**, **Linux**, **macOS** and **in the browser** (wasm).

Original project:

- Repository: https://github.com/longbridge/gpui-component
- Docs: https://longbridge.github.io/gpui-component

This tree reimplements the component examples and a small runtime on top of the OS: Win32 + Direct2D + DirectWrite on Windows, X11 + cairo + Pango on Linux, Cocoa + Core Graphics + Core Text on macOS, and a `<canvas>` 2D context in the browser. Everything above the `Paint.h` / `Platform.h` seam is shared. It is not a binding to the Rust crates, and it does not use Blade or Zed’s renderer. Layout is the exception: `src/taffy/` is a C++ port of the taffy crate GPUI itself lays out with, at the version gpui-component pins.

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

Entities are generational handles owned by `App`, not refcounted; `cx.listener` becomes `Listen(cx, &T::Handler)` and `cx.notify()` becomes `Notify(cx)`. See the _App, Window, Entity, Ctx_ section of [AGENTS.md](AGENTS.md).

The Rust sources used as the spec live in a gitignored clone at `.work/gpui-component/`. Exact checkins we are porting are in [`cmd/run.ts`](cmd/run.ts); `bun cmd/build.ts` installs that tree. Ingest playbook: [port-upstream.md](port-upstream.md).

## Build

`bun cmd/build.ts` and `bun cmd/run.ts` dispatch to the toolchain for the
machine they run on, so the same commands work on all three platforms:

```
bun cmd/build.ts -rel story
bun cmd/run.ts -rel -compare story
```

`bun cmd/build.ts` with no example name lists targets (`system_monitor`, `showcase`, `story`, …).

Markdown defaults to the complete CommonMark + GFM parser. Applications that
prefer a smaller executable can select the basic parser at build time:

```
GPUI_MARKDOWN=mini bun cmd/build.ts -rel story
GPUI_MARKDOWN=full bun cmd/build.ts -rel story   # default
```

The mini feature list and intentional omissions are in
[`src/markdown-mini/readme.md`](src/markdown-mini/readme.md).

**Windows** needs [Bun](https://bun.sh) and the MSVC C++ toolset. `cl.exe` on
PATH is used as it is; otherwise Visual Studio is found through `vswhere` and
its `vcvars64.bat` is read for the environment, so a plain shell builds.
`bun cmd/build.ts -clang <example>` builds with `clang-cl` from the same
toolset instead.

**Linux** needs g++ (or clang++), pkg-config and the X11 / cairo / pango dev
packages. On Ubuntu or Debian:

```
bash cmd/ubuntu-install-deps.sh
```

**macOS** needs the Xcode command line tools (`xcode-select --install`).

**The browser** is a target rather than a host, so it is asked for by name and
builds from any of the three:

```
bun cmd/build.ts -wasm story
bun cmd/run.ts -wasm story         # builds, serves, opens a tab
```

It needs [emscripten](https://emscripten.org), found through `$EMCC`, `$EMSDK`,
`PATH`, or an emsdk checkout beside this one:

```
git clone https://github.com/emscripten-core/emsdk ../.emsdk
cd ../.emsdk && ./emsdk install latest && ./emsdk activate latest
```

There is no other dependency: the page draws through Canvas2D. What a tab
cannot do that a desktop can — a second window, a background thread, a
blocking fetch, the machine's process table — is listed in
[AGENTS.md](AGENTS.md).

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

# gpui for C++

A **C++ / Windows** port of [longbridge/gpui-component](https://github.com/longbridge/gpui-component), a Rust UI kit built on [Zed GPUI](https://github.com/zed-industries/zed).

Original project:

- Repository: https://github.com/longbridge/gpui-component
- Docs: https://longbridge.github.io/gpui-component

This tree reimplements the component examples and a small Win32 + Direct2D + DirectWrite runtime. It is not a binding to the Rust crates and does not use Taffy, Blade, or Zed’s renderer.

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

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    App* app = AppNew();
    return AppRunView(StrL("Hello World"), 800, 600,
                      EntityNew<Example>(app).id, app, WinOpts{});
}
```

Entities are generational handles owned by `App`, not refcounted; `cx.listener` becomes `Listen(cx, &T::Handler)` and `cx.notify()` becomes `Notify(cx)`. See the *App, Window, Entity, Ctx* section of [AGENTS.md](AGENTS.md).

The Rust sources used as the spec live in a gitignored clone at `.work/gpui-component/`. Exact checkins we are porting are in [`cmd/versions.ts`](cmd/versions.ts); `bun cmd/build.ts` installs that tree. Ingest playbook: [port-upstream.md](port-upstream.md).

## Build

Windows, MSVC `cl.exe` on PATH, [Bun](https://bun.sh):

```
bun cmd/build.ts -rel story
bun cmd/run.ts -rel -compare story
```

`bun cmd/build.ts` with no example name lists targets (`system_monitor`, `showcase`, `story`, …).

# Why port to C++?

- Do you know a good joke?
- Yes, Rust.

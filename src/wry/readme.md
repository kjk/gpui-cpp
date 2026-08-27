# src/wry — the wry webview crate, ported to C++

This is a port of [wry](https://github.com/tauri-apps/wry) **0.53.3** — the
`lb-wry` fork gpui-component's `crates/webview` (the `gpui-wry` crate) depends
on, and therefore the crate that defines what a webview in a gpui window
means. `src/webview/` is the gpui-side half — the port of `crates/webview`
itself — and it is the only thing in the tree that calls in here.

The pin lives in [`cmd/run.ts`](../../cmd/run.ts) (`wry`) next to
the gpui-component, Zed GPUI, taffy and markdown pins, and moves the same way
— see [`port-upstream.md`](../../port-upstream.md).

## Where the Rust went

| Rust                                                 | C++                            |
| ---------------------------------------------------- | ------------------------------ |
| `src/lib.rs` (types, attributes, the builder, the API) | `wry.h`                        |
| `src/webview2/mod.rs`                                | `wry_win.cpp`                  |
| `src/webview2/util.rs`                               | `wry_win.cpp` (the DPI helpers) |
| `src/proxy.rs`                                       | `wry.h` (`ProxyConfig`)        |
| `src/web_context.rs`                                 | `wry.h` (`dataDirectory`)      |
| `src/error.rs`                                       | — (see below)                  |
| `src/wkwebview/mod.rs`, `class/**`, `navigation.rs`  | `wry_mac.cpp`                  |
| `src/webkitgtk/**`                                   | `wry_linux.cpp` — a stub       |
| —                                                    | `wry_wasm.cpp` — a stub        |
| `src/android/**`, `src/wkwebview/ios/**`             | not ported                     |

Everything is in `namespace wry`, not `gpui`, and the directory is isolated
the way `src/taffy` and `src/markdown` are: it includes `base.h` and its own
header and nothing else, and names no `gpui::` symbol.
`cmd/update-dist.ts` fails the build if that stops being true. The parent
window arrives as a `void*` — Rust takes a `raw_window_handle::
HasWindowHandle`, and `src/gpui`'s `PlatWindowHandle` is what hands one over.

## The WebView2 dependency, and why there is none

Rust reaches WebView2 through two crates this tree may not have (hard rule 3):
`webview2-com`, which generates COM bindings from the SDK, and
`webview2-com-sys`, which links Microsoft's `WebView2LoaderStatic.lib`. Both
halves are written out instead, and both are in `wry_win.cpp`:

- **The interfaces.** A block of `ICoreWebView2*` declarations transcribed
  from the WebView2 SDK header: same vtable order, same IIDs, comments
  dropped. It declares an ABI the way `<d2d1.h>` does — nothing of the SDK is
  compiled in, and the runtime it talks to ships with Windows' Edge. An
  interface that only appears in a signature we never call is
  forward-declared; the ones we implement (`ICoreWebView2EnvironmentOptions`
  and its 6 and 8, plus every event and completion handler) are there in
  full, because an implementation has to answer for every slot.
- **The loader.** `CreateEnvironmentWithOptions` is about a hundred lines
  doing what `WebView2Loader` does: read `WEBVIEW2_BROWSER_EXECUTABLE_FOLDER`
  or the EdgeUpdate registry keys
  (`SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-…}`, `pv` and `location`,
  per-machine through the WOW64 view and then per-user), `LoadLibrary` the
  runtime's `EBWebView\<arch>\EmbeddedBrowserWebView.dll`, and call its
  `CreateWebViewEnvironmentWithOptionsInternal` export. The argument list is
  the one the SDK's own static loader passes, read off its
  `CreateWebViewEnvironmentWithClientDll` — whose mangled name spells out the
  types: `(clientDllPath, bool, WebView2RunTimeType, userDataFolder,
  IUnknown* options, handler)`, of which everything but the path is
  forwarded.

  Two values the SDK's helper supplies and the runtime insists on: a user data
  folder (the exe's path with `.WebView2` after it, when the caller names
  none) and `TargetCompatibleBrowserVersion`, which is a *browser* version
  (`149.0.4022.49`, `CORE_WEBVIEW_TARGET_PRODUCT_VERSION` for the 1.0.4022.49
  package) and not the package's own.

## The macOS backend

`wry_mac.cpp` is `wkwebview/` and takes no dependency at all: WKWebView is in
the system SDK, and `cmd/build.ts` links one more framework for it. The
amalgam is compiled `-x objective-c++ -fobjc-arc` on macOS, so an
Objective-C pointer in one of the C++ structs there is a strong reference and
Rust's `Retained<T>` fields need nothing said about them.

Rust's delegates are `define_class!` invocations with an ivars struct. Here
each is a real `@interface` at file scope — an Objective-C class cannot live
inside a C++ namespace — holding the `wry::WebView*` those ivars would have
held: the IPC message handler, the navigation delegate, the UI delegate (the
open panel, the media-capture permission and `window.open`), the title
observer, and one `WKURLSchemeHandler` class where Rust builds a class per
scheme at runtime to have somewhere to put the index. `WryWebView` is the
sixth class: like the source it implements `acceptsFirstMouse`, lets a child
pass Command-key equivalents back to the application's menu and turns mouse
buttons four and five into DOM back/forward events.

The Darwin builder state is carried in the portable attribute record:
persistent data-store identifiers on macOS 14+, an optional existing
`WKWebViewConfiguration`, link previews, the macOS 14 inactive scheduling
policy and the traffic-light inset. A configuration supplied by the caller
keeps its data store and any custom scheme it already registered, as Rust's
builder does. A child does not take first-responder status when it is made;
the pinned crate documents `focused` as unsupported on macOS and only makes
a non-child webview first responder.

Two things differ beyond that, and both are in the file's header comment:

- **A webview that is not a child is a subview**, not the window's
  `contentView`. Rust swaps in a `WryWebViewParent` so the page gets key
  events; the content view here is the gpui view that draws everything else
  and owns the window's input, so evicting it would take the application with
  it.
- **`reparent` is ours.** Rust has it on Windows only; on macOS it is
  `removeFromSuperview` plus `addSubview`, which is what the one caller means
  by it.

An asynchronous custom-protocol responder does not retain the C++ `WebView`.
It retains the live-task set and its `WKURLSchemeTask`; closing the view first
empties that set, so an answer posted later is discarded on the main thread.
This is the same lifetime boundary as Rust's global webview-id lookup plus
per-task UUID. Teardown also nulls every delegate's non-owning pointer before
ARC releases it, because WebKit is allowed to drain an already-queued
callback after the view leaves its hierarchy.

`set_theme` and `set_memory_usage_level` answer false there, because both are
`WebViewExtWindows` in Rust with no WKWebView counterpart.
`set_background_color` is a successful no-op — the exact macOS arm of the
Rust method — because on macOS the only creation-time knob is the
`transparent` attribute.

**The backend and headless suite are compile- and test-verified; the window is
not visually run-verified.** `bun cmd/mac-build.ts -rel webview` and the full
release test target build on a Mac over ssh, and that native test binary runs
20,820 checks. A Cocoa window needs a login session, so the example still has
to be run on the Mac itself (`bun cmd/run.ts -rel webview`) for anyone to see
and interact with the page.

## What is ported

The whole of the portable API in `lib.rs`, and under it the WebView2 and
WKWebView backends:
attributes and defaults, a webview built into a window or as a child of one,
bounds / visibility / focus, `evaluate_script` with and without a callback,
`load_url`, `load_url_with_headers`, `load_html`, `reload`, `url`, zoom,
background colour, theme, the memory usage level, `reparent`, `print`,
`clear_all_browsing_data`, devtools, `webview_version`, the IPC channel
(`window.ipc.postMessage`), initialization scripts, custom protocols with
their `http://<scheme>.host` work-around and their asynchronous responder,
navigation / page-load / document-title / new-window handlers, the clipboard
permission, incognito, background throttling, the proxy switches, the Darwin
builder extensions named above and the Windows-only builder extensions
(browser arguments, accelerator keys, context menus, the https scheme,
scrollbar style, extensions).

The macOS backend has all of that except what the platform does not have: the
custom protocol work-around is Windows' alone (WKWebView takes a scheme
handler for the real scheme), the browser arguments and their neighbours are
WebView2 settings, and the two Windows-extension calls named in the section
above answer false.

Not ported, each for a reason:

- **Cookies** (`cookies`, `cookies_for_url`, `set_cookie`, `delete_cookie`).
  The API is `cookie::Cookie` from the `cookie` crate throughout — a parser,
  a builder and a `time::OffsetDateTime` — so porting the four calls means
  porting a cookie crate. Nothing here wants one yet.
- **Downloads** (`with_download_started_handler`,
  `with_download_completed_handler`). WebView2's own download behaviour is
  what the default handler allows anyway; a custom one is a `PathBuf` API
  plus a `StateChanged` handler per operation, and no caller needs it.
- **Drag and drop** (`DragDropEvent`, `webview2/drag_drop.rs`). Behind wry's
  own `drag-drop` feature flag, and it is an `IDropTarget` on the container
  plus the composition windows under it.
- **Browser extension loading**. The attribute is honoured — extensions can be
  enabled — but `load_extensions`, which walks a directory and calls
  `ICoreWebView2Profile7::AddBrowserExtension`, is not written.
- **`NewWindowResponse::Create`**, the arm that hands back a webview to open
  the request in. It needs the opener's `ICoreWebView2` and environment in the
  public API, which would put a COM type in the portable header.
- **The `_async` constructors**, which exist for callers with an async
  runtime. There is none here; `WebViewNew` is the blocking one, and it
  blocks the way Rust's does.
- **Android and iOS**, which are not platforms this tree builds for.
- **`error.rs`**. There are no exceptions here, so a call that can fail
  answers `bool` (or null) and logs. Nothing in the crate branches on an
  error kind.

## Deliberate differences

Each is also stated in a comment where it applies.

- **The builder is the attribute struct.** `WebViewBuilder::with_*` fills in
  `WebViewAttributes` one call at a time; a struct with defaults says the
  same thing in C++, so `WebViewAttributes` *is* the builder and `WebViewNew`
  is `build()` / `build_as_child()`.
- **A closure is a function pointer and a `void*`.** Every handler group
  shares the one `ctx` on the attributes, which is what a Rust closure would
  have captured.
- **An init script is registered without waiting for it.** Rust blocks on
  `AddScriptToExecuteOnDocumentCreatedCompletedHandler`; the registration and
  the navigation after it are queued on the browser thread in order, so the
  script still runs first, and the wait would re-enter the message loop once
  per script.
- **The new-window handler runs where the event arrives.** Rust spawns a
  thread and holds the request open with a deferral, because its closure may
  block. Nothing here can block on the UI thread, so there is no thread and
  no deferral.
- **No Windows 7 branches.** `is_windows_7()` gated the transparency and
  background-colour paths; nothing in this tree runs there.
- **The controller's background colour is set on the controller.** Rust asks
  `ICoreWebView2ControllerOptions3` for it before the controller exists and
  then sets it again afterwards; only the second is done here, since the
  first interface is newer than the runtime this has to work against.

The Windows custom-protocol responder owns its environment, event arguments,
deferral and dispatch coordinates independently of the C++ `WebView`. A
worker may therefore answer after the owner has closed without dereferencing
freed state, matching the ownership of Rust's responder closure. If teardown
has already destroyed the dispatch window, the copied response and COM
references are released instead of leaked; a missing handler receives a 500
response rather than a null call.

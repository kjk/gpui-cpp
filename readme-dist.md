# gpui-cpp-dist

The single-file build of [gpui-cpp](https://github.com/kjk/gpui-cpp): `gpui.h` and `gpui.cpp`,
amalgamated from that repo's `src/**` by its `cmd/update-dist.ts`. Nothing
here is written by hand, so issues and pull requests belong in the source repo.

## Try it

First install [bun](https://bun.sh/). On Windows, install Visual Studio 2026
(the free community edition is fine).

Run: `bun run.ts story`

This runs a comprehensive showcase of the available functionality
([examples/story](./examples/story)).

Run `bun run.ts` to see all the options.

## Use it

Drop both files into your tree, `#include "gpui.h"` where you need the API,
and compile `gpui.cpp` as one more source file. It is C++20, and the platform
halves are already inside it behind `GPUI_OS_*` guards, so the same pair
builds on all four:

- **Windows** — `cl /std:c++20 /EHsc /utf-8 /DUNICODE /D_UNICODE`, static CRT;
  links against the Win32, Direct2D and DirectWrite import libraries.
- **Linux** — `g++ -std=c++20` with `pkg-config --cflags --libs x11 cairo pangocairo`.
- **macOS** — `clang++ -std=c++20 -x objective-c++` with the Cocoa, CoreText and
  IOKit frameworks. The file is Objective-C++ because the mac half is.
- **wasm** — `em++ -std=c++20` with `-sALLOW_MEMORY_GROWTH`; the browser half
  draws through Canvas2D and needs no library at all. em++ rather than emcc:
  the link needs the C++ runtime and emcc leaves it out.

No other dependencies, no build system, no STL containers.

## This copy

Amalgamated from gpui-cpp [`<checkin-sha1>`](https://github.com/kjk/gpui-cpp/commit/<checkin-sha1>) — input: one dismiss, not two

[What has changed in gpui-cpp since](https://github.com/kjk/gpui-cpp/compare/<checkin-sha1>...main)
shows every commit this copy is behind by; if that page is empty, it is current.

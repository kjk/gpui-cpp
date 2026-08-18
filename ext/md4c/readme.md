# md4c

Markdown parser used by `src/component/Text.cpp` (`component::TextView`).

- Upstream: https://github.com/mity/md4c
- Version: **0.5.3** (tag `release-0.5.3`)
- License: MIT — see `LICENSE.md`

Only the parser is vendored, not the HTML renderer or the `md2html` CLI:

| file | upstream path |
| --- | --- |
| `md4c.c` | `src/md4c.c` |
| `md4c.h` | `src/md4c.h` |
| `LICENSE.md` | `LICENSE.md` |

The files are unmodified. To update, bump the tag in the three URLs below and
re-run them from the repo root, then update the version above:

```
curl -o ext/md4c/md4c.c    https://raw.githubusercontent.com/mity/md4c/release-0.5.3/src/md4c.c
curl -o ext/md4c/md4c.h    https://raw.githubusercontent.com/mity/md4c/release-0.5.3/src/md4c.h
curl -o ext/md4c/LICENSE.md https://raw.githubusercontent.com/mity/md4c/release-0.5.3/LICENSE.md
```

## Why this one

It is a single C file with no dependencies and no allocator of its own beyond
`malloc`, it is CommonMark-compliant, and it parses SAX-style: it calls back
with `enter_block` / `leave_block` / `enter_span` / `leave_span` / `text`
rather than building an AST. `component::TextView` turns those callbacks into
the block tree it renders, which is the same shape gpui-component gets by
converting the `markdown` crate's mdast into `crates/ui/src/text/node.rs`.

## How it is built

`cmd/build-dist.ts` amalgamates it along with `src/**`: `md4c.h` becomes the
tail of `gpui.h` and `md4c.c` the tail of `gpui.cpp`, so it compiles as C++
inside the one translation unit the library is. Nothing includes `md4c.h` by
name any more — `src/component/Text.cpp` still says so, and the amalgamator
strips that the way it strips every other internal include.

Two things follow from that, and both live in `cmd/build-dist.ts`, not here:

- **The casts C++ needs and C did not.** Five `malloc` / `realloc` results and
  one `md_mark_get_ptr` result are assigned or passed as a typed pointer, which
  C converts from `void*` on its own and C++ does not. The amalgamator adds
  those six casts as it emits the chunk. Each one must match exactly once, so a refresh
  that moves them fails the build instead of silently skipping a fix.
- **Warnings.** The tree builds with `/W4 /WX` and `-Wall -Wextra -Werror`, and
  this file is not ours to keep clean under those. The chunk is bracketed by
  `#pragma warning(push, 0)` plus an explicit `disable` for C4701 and C4702 —
  MSVC decides those two after code generation, so the level-0 push does not
  reach them — and by `#pragma GCC diagnostic ignored
  "-Wmissing-field-initializers"`. `MIN` and `MAX` are `#undef`ed first,
  because md4c defines both unguarded and glib already has them.

The files themselves stay unmodified. Update by re-running the curl commands
above, never by patching in place.

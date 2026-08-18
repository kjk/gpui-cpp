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

It is compiled as C, as its own translation unit — it is not part of the
`src/**` amalgamation that `cmd/build-dist.ts` produces. The three build
scripts add `ext/md4c` to the include path and compile this one file with
`/TC /std:c17 /w` (MSVC) or `-x c -std=c11 -w` (gcc, clang), so the tree's
`-Werror` / `/WX` applies to our code and not to a vendored file we do not
edit. Keep it that way: update by re-running the curl commands above, never by
patching in place.

# src/html5ever — the `html5ever` crate, ported to C++

This is the dependency-free C++ port of
[html5ever](https://github.com/servo/html5ever) **0.27.0** used by
`gpui-base`. The version and crates.io checksum are pinned in
[`cmd/run.ts`](../../cmd/run.ts). `src/base/text_format.cpp` parses through
this DOM and folds it into the same `MdNode` tree as markdown, matching
`crates/base/src/text/format/html.rs`.

Everything is in `namespace html5ever` and depends only on `base.h`. Nodes,
attributes, token text and decoded values are arena-owned POD. There are no
STL containers, exceptions, RTTI or platform calls.

```cpp
Arena* a = ArenaNew();
html5ever::Node* document =
    html5ever::ParseDocument(a, StrL("<p>Hello <b>world</b>"));
Str normalized = html5ever::Serialize(a, document);
ArenaDelete(a);
```

## Rust-to-C++ map

| Rust | C++ |
| --- | --- |
| `tokenizer/interface.rs`, `tokenizer/*` | `Token`, `TokenSink`, `Tokenize` in `html5ever.h/.cpp` |
| `tree_builder/interface.rs`, `tree_builder/*` | arena DOM plus `ParseDocument` / `ParseFragment` |
| `driver.rs` | the two parse entry points and `ParseOptions` |
| `serialize/mod.rs` | `Serialize` |
| generated tag atoms and sets | `SeqStrings` runs |

The tree builder implements implicit html/head/body and table containers,
scope-based implied ends for paragraphs, list items and headings, table foster
parenting, formatting-element reconstruction for misnested inline markup,
raw-text/RCDATA tokenization, duplicate-attribute removal, numeric character
reference replacement, HTML/SVG/MathML namespaces and document/fragment
parsing.

The Rust-only generic `TreeSink`/`Tracer` ownership machinery is represented by
the concrete arena DOM. Incremental tendril feeding and parser suspension for
an executing script are omitted: this tree parses complete UTF-8 `Str` values
and never executes HTML scripts. Exact-error mode reports tokenizer errors as
tokens rather than preserving html5ever's Rust log strings. The named-reference
table is limited to reader-mode spellings plus the long reference covered by
the upstream projection tests; numeric references are complete.

## Mini and standalone builds

`GPUI_HTML5EVER=mini` selects [`src/html5ever-mini`](../html5ever-mini), which
implements the same public header with the former small reader-mode parser.
The generated `GPUI_HTML5EVER_FULL` / `GPUI_HTML5EVER_MINI` macros describe
which implementation is present.

`cmd/update-dist.ts` also generates `extras/html5ever/html5ever.h/.cpp` and
`extras/html5ever-mini/html5ever.h/.cpp`. Each is standalone and carries the
base implementation, so neither is linked beside `gpui.cpp`, which already
contains the selected parser.

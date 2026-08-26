# markdown-mini

`markdown-mini` implements the public API in `src/markdown/markdown.h` and
produces the same `markdown::Node` mdast. Select it when building with:

```text
GPUI_MARKDOWN=mini bun cmd/build.ts -rel story
```

`GPUI_MARKDOWN=full` is the default. Selection happens while `gpui.cpp` is
amalgamated, so only one parser reaches the compiler and linker.

The mini parser supports paragraphs, ATX and setext headings, bold, italic,
inline and fenced/indented code, ordered and unordered lists, blockquotes,
inline links and images, thematic breaks, backslash escapes, hard breaks,
numeric entities, and the small named-entity set `amp`, `lt`, `gt`, `quot`,
`apos`, and `nbsp` (including the HTML uppercase aliases where they exist).

For size, it omits GFM tables, task lists, footnotes, raw HTML, autolinks,
reference-style links, math/frontmatter, strikethrough, and the full HTML5
named-entity database. Unsupported syntax remains ordinary readable text.

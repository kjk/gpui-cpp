#include "ui/text.h"

namespace gpui {

namespace component {

TextViewStyle UiTextViewStyle(const Theme& theme) {
    // The colours first — `with_foreground`, `with_link` and the rest — from
    // the themed palette rather than from the Base one, so an application
    // theme reaches rich text.
    TextViewStyle style = TextViewStyle::Default();
    style.WithForeground(theme.foreground)
        .WithMutedForeground(theme.mutedFg)
        .WithLink(theme.link)
        .WithSelection(theme.selection)
        .WithCodeBackground(theme.muted)
        .WithBorder(theme.border)
        .WithDark(theme.mode == ThemeMode::Dark);

    // The radius Base no longer reads off a theme arrives as a refinement on
    // the table and the code block, which is exactly where node.rs looks.
    gpui::Style radius = {};
    radius.radius = theme.radius;
    style.WithTable(radius, StyleFieldRadius);
    style.WithCodeBlock(radius, StyleFieldRadius);

    // The header row keeps the table theme's own pair rather than the code
    // background Base falls back to.
    gpui::Style head = {};
    head.bg = Background(theme.tokens.tableHead);
    head.color = theme.tableHeadFg;
    style.WithTableHead(head, StyleFieldBg | StyleFieldColor);

    gpui::Style inlineCode = {};
    inlineCode.bg = Background(theme.accent);
    style.WithInlineCode(inlineCode, StyleFieldBg);
    return style;
}

void UiCodeBlockHighlighter(void* data, const CodeBlock* block, Arena* a,
                            ArenaVec<CodeHighlight>* out) {
    App* app = (App*)data;
    if (!block || !out || !a || !app) {
        return;
    }
    SyntaxLang lang = SyntaxLangFor(block->Lang());
    if (lang == SyntaxLangNone) {
        return;
    }
    // The colours are the theme's, read live rather than captured, so a
    // theme change repaints the same parsed block in the new palette —
    // `a_new_highlighter_replaces_styles_instead_of_reusing_the_cache`.
    ThemeMode mode = ThemeGet(app);
    Rgba fallback = ThemeNow(app).foreground;
    Str code = block->Code();
    SyntaxLexer lx;
    SyntaxLexStart(&lx, lang, code);
    while (SyntaxLexNext(&lx)) {
        Rgba color = SyntaxTokColor(lx.tok, mode, fallback);
        if (!color.a || lx.text.len <= 0) {
            continue;
        }
        CodeHighlight span;
        span.start = (int)(lx.text.s - code.s);
        span.end = span.start + lx.text.len;
        span.color = color;
        out->Append(a, span);
    }
}

void TextViewInstallDefaults(App* app) {
    if (!app) {
        return;
    }
    TextViewDefaults::New()
        .WithStyle(UiTextViewStyle(ThemeNow(app)))
        .WithCodeBlockHighlighter(&UiCodeBlockHighlighter, app)
        .Install(app);
}

} // namespace component
} // namespace gpui

#include "Story.h"

static El* Shield(Arena* a, const char* left, const char* right, Rgba rightBg) {
    Rgba leftBg = Rgb(0x4b, 0x55, 0x63);
    El* row = Div(a)->FlexRow()->H(20)->Radius(3);
    row->Child(Div(a)->H(20)->PadX(6)->ItemsCenter()->Bg(leftBg)->Child(
        StoryTxt(a, Str(left), 11, Rgb(0xff, 0xff, 0xff))));
    row->Child(Div(a)->H(20)->PadX(6)->ItemsCenter()->Bg(rightBg)->Child(
        StoryTxt(a, Str(right), 11, Rgb(0xff, 0xff, 0xff))));
    return row;
}

static El* LogoPiece(Arena* a, float x, float y, float w, float h, Rgba c) {
    const float s = 112.f / 32.f;
    return Div(a)->W(w * s)->H(h * s)->Bg(c)->Absolute()->Left(x * s)->Top(y *
                                                                           s);
}

static El* LogoMark(Arena* a) {
    // website/public/logo.svg — 32 viewBox, README shows it at 112px.
    Rgba dark = Rgb(0x1f, 0x20, 0x23);
    Rgba blue = Rgb(0x3b, 0x82, 0xf6);
    El* mark = Div(a)->W(112)->H(112)->Shrink0();
    mark->Child(LogoPiece(a, 4, 4, 24, 5, dark));
    mark->Child(LogoPiece(a, 4, 4, 6, 24, dark));
    mark->Child(LogoPiece(a, 4, 23, 24, 5, dark));
    mark->Child(LogoPiece(a, 16, 13, 12, 5, blue));
    mark->Child(LogoPiece(a, 23, 13, 5, 10, blue));
    return mark;
}

static El* FeatureLine(Arena* a, const char* label, const char* rest) {
    const Theme& th = ThemeNow();
    El* row = Div(a)->FlexRow()->Gap(4)->W(kFill)->ItemsStart();
    row->Child(
        StoryTxt(a, StoryFmt(a, "\xE2\x80\xA2  %s:", label), 14, th.foreground)
            ->Bold()
            ->Shrink0());
    row->Child(
        StoryTxt(a, StoryDup(a, rest), 14, th.foreground)->Wrap()->Grow());
    return row;
}

static El* MdTableCell(Arena* a, const char* s, bool header) {
    const Theme& th = ThemeNow();
    El* txt = StoryTxt(a, StoryDup(a, s), 14, th.foreground);
    if (header) {
        txt->Bold();
    }
    return Div(a)->Grow()->PadX(12)->PadY(8)->ItemsCenter()->Child(txt);
}

static El* MdTableRow(Arena* a, const char* left, const char* right,
                      bool header) {
    const Theme& th = ThemeNow();
    El* row = Div(a)->FlexRow()->W(kFill)->Shrink0();
    if (!header) {
        row->BorderT(1, th.border);
    }
    El* mid = Div(a)->W(1)->Bg(th.border)->Shrink0();
    row->Child(MdTableCell(a, left, header))
        ->Child(mid)
        ->Child(MdTableCell(a, right, header));
    return row;
}

static El* AsciiLine(Arena* a, const char* s) {
    const Theme& th = ThemeNow();
    return StoryTxt(a, StoryDup(a, s), 12, th.mutedFg);
}

El* WelcomeRender(StoryApp* app, Arena* a) {
    (void)app;
    const Theme& th = ThemeNow();
    El* col = Div(a)->FlexCol()->Gap(12)->W(kFill)->MaxW(760);

    El* hero = Div(a)->FlexCol()->Gap(8)->ItemsCenter()->W(kFill);
    hero->Child(LogoMark(a));
    hero->Child(StoryTxt(a, StrL("GPUI Component"), 18, th.foreground)->Bold());
    col->Child(hero);

    El* langs = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    langs->Child(StoryTxt(a, StrL("English"), 14, th.foreground));
    langs->Child(StoryTxt(a, StrL("|"), 14, th.mutedFg));
    langs->Child(
        StoryTxt(a, StrL("\xE7\xAE\x80\xE4\xBD\x93\xE4\xB8\xAD\xE6\x96\x87"),
                 14, th.blue));
    col->Child(langs);

    El* badges = Div(a)->FlexRow()->Gap(6)->ItemsCenter();
    badges->Child(Shield(a, "CI", "passing", Rgb(0x4c, 0xc7, 0x1f)));
    badges->Child(Shield(a, "docs", "passing", Rgb(0x4c, 0xc7, 0x1f)));
    badges->Child(Shield(a, "crates.io", "v0.5.1", Rgb(0xfe, 0x7d, 0x37)));
    col->Child(badges);

    col->Child(StoryTxt(a,
                        StrL("UI components for building fantastic desktop "
                             "applications using GPUI."),
                        14, th.foreground)
                   ->Wrap());

    col->Child(StoryTxt(a, StrL("Features"), 18, th.foreground)->Bold());
    El* feats = Div(a)->FlexCol()->Gap(4)->W(kFill);
    feats->Child(FeatureLine(a, "Richness",
                             "60+ cross-platform desktop UI components."));
    feats->Child(FeatureLine(a, "Native",
                             "Inspired by macOS and Windows controls, combined "
                             "with shadcn/ui design for a modern "
                             "experience."));
    feats->Child(FeatureLine(
        a, "Ease of Use",
        "Stateless RenderOnce components, simple and user-friendly."));
    feats->Child(FeatureLine(a, "Customizable",
                             "Built-in Theme and ThemeColor, supporting "
                             "multi-theme and variable-based configurations."));
    feats->Child(
        FeatureLine(a, "Versatile", "Supports sizes like xs, sm, md, and lg."));
    feats->Child(FeatureLine(a, "Flexible Layout",
                             "Dock layout for panel arrangements, resizing, "
                             "and freeform (Tiles) layouts."));
    feats->Child(FeatureLine(a, "High Performance",
                             "Virtualized Table and List components for smooth "
                             "large-data rendering."));
    feats->Child(FeatureLine(a, "Content Rendering",
                             "Native support for Markdown and simple HTML."));
    feats->Child(FeatureLine(a, "Charting",
                             "Built-in charts for visualizing your data."));
    feats->Child(FeatureLine(
        a, "Editor",
        "High performance code editor (Up to 200K lines for stable "
        "performance) with LSP (diagnostics, completion, hover, etc)."));
    feats->Child(FeatureLine(a, "Syntax Highlighting",
                             "Syntax highlighting for editor and markdown "
                             "components using Tree Sitter."));
    col->Child(feats);

    El* eco = Div(a)->FlexCol()->Gap(6)->W(kFill);
    eco->Child(StoryTxt(a, StrL("Ecosystem Architecture"), 18, th.foreground)
                   ->Bold());
    eco->Child(
        StoryTxt(a, StrL("Two layers. One ecosystem."), 16, th.foreground)
            ->Bold());
    eco->Child(StoryTxt(a,
                        StrL("Choose the layer that matches how much of the "
                             "interface you want to own."),
                        14, th.foreground)
                   ->Wrap());
    col->Child(eco);

    El* table = Div(a)->FlexCol()->W(kFill)->Border(1, th.border)->Shrink0();
    table->Child(MdTableRow(a, "GPUI Component", "gpui-base", true));
    table->Child(MdTableRow(a, "Complete, styled components",
                            "Unstyled behavior and infrastructure", false));
    table->Child(MdTableRow(a, "Productive defaults with theming",
                            "Full control over structure and visual design",
                            false));
    table->Child(MdTableRow(a, "Best for building applications",
                            "Best for building design systems", false));
    col->Child(table);

    El* diagram = Div(a)->FlexCol()->Gap(1)->W(kFill)->Pad(16)->Bg(th.muted);
    static const char* kDiagram[] = {
        "                             APPLICATION",
        "                                  |",
        "                +-----------------+-----------------+",
        "                |                                   |",
        "                v                                   v",
        "       +------------------+               +------------------+",
        "       |  gpui-component  |               | Your Design      |",
        "       |    Styled UI     |               | System           |",
        "       +--------+---------+               +--------+---------+",
        "                |                                  |",
        "                +----------------+-----------------+",
        "                                 v",
        "                       +------------------+",
        "                       |    gpui-base     |",
        "                       | Behavior . State |",
        "                       | Infrastructure   |",
        "                       +--------+---------+",
        "                                v",
        "                              GPUI",
    };
    for (int i = 0; i < (int)(sizeof(kDiagram) / sizeof(kDiagram[0])); i++) {
        diagram->Child(AsciiLine(a, kDiagram[i]));
    }
    col->Child(diagram);

    return col;
}

void WelcomeClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryWelcome, WelcomeRender, WelcomeClick);

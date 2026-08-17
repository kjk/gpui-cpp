#include "Story.h"

static El* MdTxt(Arena* a, Str s, float px, Rgba c) {
    return StoryTxt(a, s, px, c)->Selectable();
}

static El* Shield(Arena* a, const char* left, const char* right, Rgba rightBg) {
    Rgba leftBg = Rgb(0x4b, 0x55, 0x63);
    El* row = Div(a)->FlexRow()->H(20)->Radius(3);
    row->Child(Div(a)->H(20)->PadX(6)->ItemsCenter()->Bg(leftBg)->Child(
        MdTxt(a, Str(left), 11, Rgb(0xff, 0xff, 0xff))));
    row->Child(Div(a)->H(20)->PadX(6)->ItemsCenter()->Bg(rightBg)->Child(
        MdTxt(a, Str(right), 11, Rgb(0xff, 0xff, 0xff))));
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

// Match crates/ui TextView markdown: body 16px, h2 1.5*14=21, h3 1.25*14=17.5.
static const float kMd = 16.f;
static const float kMdH2 = 21.f;
static const float kMdH3 = 17.5f;
static const float kMdCode = 13.f;

static El* FeatureLine(Arena* a, const char* label, const char* rest) {
    const Theme& th = ThemeNow();
    El* row = Div(a)->FlexRow()->Gap(6)->W(kFill)->ItemsStart();
    row->Child(
        MdTxt(a, StoryFmt(a, "\xE2\x80\xA2  %s:", label), kMd, th.foreground)
            ->Bold()
            ->Shrink0());
    row->Child(MdTxt(a, StoryDup(a, rest), kMd, th.foreground)->Wrap()->Grow());
    return row;
}

static El* MdTableCell(Arena* a, const char* s, bool header) {
    const Theme& th = ThemeNow();
    El* txt = MdTxt(a, StoryDup(a, s), kMd, th.foreground);
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
    return MdTxt(a, StoryDup(a, s), kMdCode, th.foreground);
}

static El* Body(Arena* a, const char* s) {
    const Theme& th = ThemeNow();
    return MdTxt(a, StoryDup(a, s), kMd, th.foreground)
        ->Wrap()
        ->W(kFill)
        ->PadB(16);
}

static El* LinkBody(Arena* a, const char* s) {
    const Theme& th = ThemeNow();
    return MdTxt(a, StoryDup(a, s), kMd, th.blue)->Wrap()->W(kFill)->PadB(16);
}

static El* H2(Arena* a, const char* s) {
    const Theme& th = ThemeNow();
    return MdTxt(a, StoryDup(a, s), kMdH2, th.foreground)->Semibold()->PadB(5);
}

static El* H3(Arena* a, const char* s) {
    const Theme& th = ThemeNow();
    return MdTxt(a, StoryDup(a, s), kMdH3, th.foreground)->Semibold()->PadB(5);
}

static El* Quote(Arena* a, const char* s) {
    const Theme& th = ThemeNow();
    El* row = Div(a)->FlexRow()->W(kFill)->ItemsStart()->PadB(16);
    row->Child(
        Div(a)->W(3)->H(kFill)->MinH(20)->Bg(th.secondaryActive)->Shrink0());
    row->Child(Div(a)->PadX(16)->Grow()->Child(
        MdTxt(a, StoryDup(a, s), kMd, th.mutedFg)->Wrap()->W(kFill)));
    return row;
}

static El* CodeBlock(Arena* a, const char* s) {
    const Theme& th = ThemeNow();
    El* box = Div(a)->W(kFill)->Pad(12)->Radius(th.radius)->Bg(th.muted)->Child(
        MdTxt(a, StoryDup(a, s), kMdCode, th.foreground)->Wrap()->W(kFill));
    return Div(a)->W(kFill)->PadB(16)->Child(box);
}

static El* Bullet(Arena* a, const char* s) {
    const Theme& th = ThemeNow();
    return MdTxt(a, StoryFmt(a, "\xE2\x80\xA2  %s", s), kMd, th.foreground)
        ->Wrap()
        ->W(kFill);
}

El* WelcomeRender(StoryApp* app, Arena* a) {
    (void)app;
    const Theme& th = ThemeNow();
    El* col = Div(a)->FlexCol()->Gap(0)->W(kFill);

    El* hero = Div(a)->FlexCol()->Gap(8)->W(kFill)->PadB(16);
    hero->Child(LogoMark(a));
    hero->Child(MdTxt(a, StrL("GPUI Component"), kMd, th.foreground)->Bold());
    col->Child(hero);

    El* langs = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    langs->Child(MdTxt(a, StrL("English"), kMd, th.foreground));
    langs->Child(MdTxt(a, StrL("|"), kMd, th.mutedFg));
    langs->Child(MdTxt(a,
                       StrL("\xE7\xAE\x80\xE4\xBD\x93\xE4\xB8\xAD\xE6\x96\x87"),
                       kMd, th.blue));
    col->Child(langs->PadB(16));

    El* badges = Div(a)->FlexRow()->Gap(6)->ItemsCenter();
    badges->Child(Shield(a, "CI", "passing", Rgb(0x4c, 0xc7, 0x1f)));
    badges->Child(Shield(a, "docs", "passing", Rgb(0x4c, 0xc7, 0x1f)));
    badges->Child(Shield(a, "crates.io", "v0.5.1", Rgb(0xfe, 0x7d, 0x37)));
    col->Child(badges->PadB(16));

    col->Child(MdTxt(a,
                     StrL("UI components for building fantastic desktop "
                          "applications using GPUI."),
                     kMd, th.foreground)
                   ->Wrap()
                   ->W(kFill)
                   ->PadB(16));

    col->Child(H2(a, "Features"));
    El* feats = Div(a)->FlexCol()->Gap(2)->W(kFill)->PadB(16);
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

    El* eco = Div(a)->FlexCol()->Gap(4)->W(kFill);
    eco->Child(H2(a, "Ecosystem Architecture"));
    eco->Child(H3(a, "Two layers. One ecosystem."));
    eco->Child(Body(a,
                    "Choose the layer that matches how much of the "
                    "interface you want to own."));
    col->Child(eco->PadB(16));

    El* table = Div(a)->FlexCol()->W(kFill)->Border(1, th.border)->Shrink0();
    table->Child(MdTableRow(a, "GPUI Component", "gpui-base", true));
    table->Child(MdTableRow(a, "Complete, styled components",
                            "Unstyled behavior and infrastructure", false));
    table->Child(MdTableRow(a, "Productive defaults with theming",
                            "Full control over structure and visual design",
                            false));
    table->Child(MdTableRow(a, "Best for building applications",
                            "Best for building design systems", false));
    col->Child(table->PadB(16));

    El* diagram =
        Div(a)->FlexCol()->Gap(0)->W(kFill)->Pad(12)->Radius(th.radius)->Bg(
            th.muted);
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
    col->Child(diagram->PadB(16));

    col->Child(
        Quote(a,
              "Behavior belongs to the foundation. Presentation belongs to the "
              "application."));
    col->Child(Body(
        a,
        "Use GPUI Component when you want polished controls ready to ship. "
        "Build on gpui-base when your application should own its component "
        "source, layout, styling, and motion while reusing difficult "
        "interaction behavior."));
    col->Child(Body(a,
                    "The layering follows the same separation that makes "
                    "the shadcn ecosystem flexible:"));

    El* eco2 = Div(a)->FlexCol()->W(kFill)->Border(1, th.border)->Shrink0();
    eco2->Child(
        MdTableRow(a, "GPUI Component ecosystem", "Web ecosystem", true));
    eco2->Child(MdTableRow(a, "GPUI", "HTML + Tailwind CSS", false));
    eco2->Child(MdTableRow(a, "gpui-base", "Base UI", false));
    eco2->Child(MdTableRow(a, "GPUI Component",
                           "shadcn's styled component layer", false));
    col->Child(eco2->PadB(16));

    col->Child(H2(a, "Showcase"));
    col->Child(
        LinkBody(a, "https://longbridge.github.io/gpui-component/gallery/"));
    col->Child(
        Body(a,
             "Here is the first application: Longbridge Pro, built using "
             "GPUI Component."));

    col->Child(H2(a, "Usage"));
    col->Child(CodeBlock(
        a,
        "gpui = { git = \"https://github.com/zed-industries/zed\" }\n"
        "gpui_platform = { git = \"https://github.com/zed-industries/zed\", "
        "features = [\"font-kit\"] }\n"
        "gpui-component = { git = "
        "\"https://github.com/longbridge/gpui-component\" }"));

    col->Child(H3(a, "Basic Example"));
    col->Child(Body(
        a,
        "See examples/hello_world.cpp in this tree, or the Rust hello_world "
        "example: a centered \"Hello, World!\" label and a primary Let's Go "
        "button inside Root."));

    col->Child(H3(a, "Icons"));
    col->Child(Body(
        a,
        "GPUI Component has an Icon element, but it does not include SVG "
        "files by default. The example uses Lucide icons; add any icons you "
        "need to your project."));

    col->Child(H2(a, "Development"));
    col->Child(H3(a, "Desktop Gallery (Story)"));
    col->Child(Body(a, "The story crate is a gallery of every component."));
    col->Child(CodeBlock(a, "cargo run"));
    col->Child(H3(a, "Examples"));
    col->Child(Body(a, "Some examples are built into the story crate:"));
    col->Child(CodeBlock(a,
                         "cargo run --example editor\n"
                         "cargo run --example dock\n"
                         "cargo run --example markdown\n"
                         "cargo run --example html"));
    col->Child(Body(
        a,
        "Standalone crates live under examples/; run them with cargo run -p "
        "<name> (hello_world, system_monitor, window_title, \xE2\x80\xA6)."));
    col->Child(H3(a, "Web Gallery (WASM)"));
    col->Child(CodeBlock(
        a, "cd crates/story-web\nmake install   # first time\nmake dev"));
    col->Child(LinkBody(a, "http://localhost:3000"));

    col->Child(H2(a, "Compare to others"));
    El* cmp = Div(a)->FlexCol()->W(kFill)->Border(1, th.border)->Shrink0();
    cmp->Child(MdTableRow(a, "Feature", "GPUI Component", true));
    cmp->Child(MdTableRow(a, "Language", "Rust", false));
    cmp->Child(MdTableRow(a, "Core Render", "GPUI", false));
    cmp->Child(MdTableRow(a, "License", "Apache 2.0", false));
    cmp->Child(MdTableRow(a, "Min Binary Size", "12MB", false));
    cmp->Child(MdTableRow(a, "Cross-Platform", "Yes", false));
    cmp->Child(MdTableRow(a, "Web", "Yes (WASM)", false));
    cmp->Child(MdTableRow(a, "UI Style", "Modern", false));
    cmp->Child(MdTableRow(a, "CJK Support", "Yes", false));
    cmp->Child(MdTableRow(a, "Chart", "Yes", false));
    cmp->Child(MdTableRow(a, "Table (Large dataset)",
                          "Yes (Virtual Rows, Columns)", false));
    cmp->Child(MdTableRow(a, "CodeEditor", "Simple", false));
    cmp->Child(MdTableRow(a, "Dock Layout", "Yes", false));
    cmp->Child(MdTableRow(a, "Syntax Highlight", "Tree Sitter", false));
    cmp->Child(MdTableRow(a, "Markdown Rendering", "Yes", false));
    cmp->Child(MdTableRow(a, "HTML Rendering", "Basic", false));
    cmp->Child(MdTableRow(a, "Custom Theme", "Yes", false));
    cmp->Child(MdTableRow(a, "I18n", "Yes", false));
    col->Child(cmp->PadB(16));

    col->Child(H2(a, "License"));
    col->Child(Body(a, "Apache-2.0"));
    col->Child(Bullet(a, "UI design based on shadcn/ui, some from Reui."));
    col->Child(Bullet(a, "Icons from Lucide."));

    return col;
}

void WelcomeClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryWelcome, WelcomeRender, WelcomeClick);

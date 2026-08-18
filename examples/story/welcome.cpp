#include "Story.h"

struct WelcomeStory {
    static El* Render(WelcomeStory* self, Ctx* cx);
};

static El* MdTxt(Ctx* cx, Str s, float px, Rgba c) {
    return StoryTxt(cx, s, px, c)->Selectable();
}

// A shields.io badge: dark label on the left, colored value on the right.
// The colors are the mid-tone of each badge SVG's vertical gradient, which is
// what the README renders to; a flat fill of the nominal color reads brighter.
// `mark` stands in for a badge logo — the CI badge carries the GitHub mark,
// which is not one of the Lucide icons this tree ships.
static El* Shield(Ctx* cx, const char* left, const char* right, Rgba rightBg,
                  bool mark = false, Rgba leftBg = Rgb(0x5a, 0x5a, 0x5a)) {
    Arena* a = cx->a;
    Rgba white = Rgb(0xff, 0xff, 0xff);
    El* row = Div(a)->FlexRow()->H(20)->Radius(3);
    El* label =
        Div(a)->FlexRow()->H(20)->PadX(6)->Gap(4)->ItemsCenter()->Bg(leftBg);
    if (mark) {
        label->Child(Div(a)->W(11)->H(11)->Radius(6)->Bg(white)->Shrink0());
    }
    label->Child(MdTxt(cx, Str(left), 11, white));
    row->Child(label);
    row->Child(Div(a)->H(20)->PadX(6)->ItemsCenter()->Bg(rightBg)->Child(
        MdTxt(cx, Str(right), 11, white)));
    return row;
}

static void FillLogoBox(PaintCtx* ctx, float x, float y, float w, float h,
                        Rgba c) {
    CanvasFillRect(ctx, x, y, w, h, c);
}

static void PaintLogoMark(PaintCtx* ctx, El* e, void*) {
    // website/public/logo.svg — 32 viewBox, README shows it at 112px.
    // #1F2023 and #3B82F6 are the literal path fills in that file.
    float s = e->w / 32.f;
    Rgba dark = Rgb(0x1f, 0x20, 0x23);
    Rgba blue = Rgb(0x3b, 0x82, 0xf6);
    FillLogoBox(ctx, e->x + 4 * s, e->y + 4 * s, 24 * s, 5 * s, dark);
    FillLogoBox(ctx, e->x + 4 * s, e->y + 4 * s, 6 * s, 24 * s, dark);
    FillLogoBox(ctx, e->x + 4 * s, e->y + 23 * s, 24 * s, 5 * s, dark);
    FillLogoBox(ctx, e->x + 16 * s, e->y + 13 * s, 12 * s, 5 * s, blue);
    FillLogoBox(ctx, e->x + 23 * s, e->y + 13 * s, 5 * s, 10 * s, blue);
}

static El* LogoMark(Ctx* cx) {
    Arena* a = cx->a;
    El* mark = Div(a)->W(112)->H(112)->Shrink0();
    mark->customPaint = PaintLogoMark;
    return mark;
}

// Match crates/ui TextView markdown: body 16px, h2 1.5*14=21, h3 1.25*14=17.5.
static const float kMd = 16.f;
static const float kMdH2 = 21.f;
static const float kMdH3 = 17.5f;
static const float kMdCode = 13.f;

static El* FeatureLine(Ctx* cx, const char* label, const char* rest) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* row = Div(a)->FlexRow()->Gap(6)->W(kFill)->ItemsStart();
    row->Child(
        MdTxt(cx, StoryFmt(cx, "\xE2\x80\xA2  %s:", label), kMd, th.foreground)
            ->Bold()
            ->Shrink0());
    row->Child(
        MdTxt(cx, StoryDup(cx, rest), kMd, th.foreground)->Wrap()->Grow());
    return row;
}

static El* MdTableCell(Ctx* cx, const char* s, bool header) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* txt = MdTxt(cx, StoryDup(cx, s), kMd, th.foreground);
    if (header) {
        txt->Bold();
    }
    return Div(a)->Grow()->PadX(12)->PadY(8)->ItemsCenter()->Child(txt);
}

static El* MdTableRow(Ctx* cx, const char* left, const char* right,
                      bool header) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* row = Div(a)->FlexRow()->W(kFill)->Shrink0();
    if (!header) {
        row->BorderT(1, th.border);
    }
    El* mid = Div(a)->W(1)->Bg(th.border)->Shrink0();
    row->Child(MdTableCell(cx, left, header))
        ->Child(mid)
        ->Child(MdTableCell(cx, right, header));
    return row;
}

static El* AsciiLine(Ctx* cx, const char* s) {
    const Theme& th = cx->theme();
    return MdTxt(cx, StoryDup(cx, s), kMdCode, th.foreground);
}

static El* Body(Ctx* cx, const char* s) {
    const Theme& th = cx->theme();
    return MdTxt(cx, StoryDup(cx, s), kMd, th.foreground)
        ->Wrap()
        ->W(kFill)
        ->PadB(16);
}

static El* LinkBody(Ctx* cx, const char* s) {
    const Theme& th = cx->theme();
    return MdTxt(cx, StoryDup(cx, s), kMd, th.primary)
        ->Wrap()
        ->W(kFill)
        ->PadB(16);
}

static El* H2(Ctx* cx, const char* s) {
    const Theme& th = cx->theme();
    return MdTxt(cx, StoryDup(cx, s), kMdH2, th.foreground)
        ->Semibold()
        ->PadB(5);
}

static El* H3(Ctx* cx, const char* s) {
    const Theme& th = cx->theme();
    return MdTxt(cx, StoryDup(cx, s), kMdH3, th.foreground)
        ->Semibold()
        ->PadB(5);
}

static El* Quote(Ctx* cx, const char* s) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* row = Div(a)->FlexRow()->W(kFill)->ItemsStart()->PadB(16);
    row->Child(
        Div(a)->W(3)->H(kFill)->MinH(20)->Bg(th.secondaryActive)->Shrink0());
    row->Child(Div(a)->PadX(16)->Grow()->Child(
        MdTxt(cx, StoryDup(cx, s), kMd, th.mutedFg)->Wrap()->W(kFill)));
    return row;
}

static El* CodeBlock(Ctx* cx, const char* s) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* box = Div(a)->W(kFill)->Pad(12)->Radius(th.radius)->Bg(th.muted)->Child(
        MdTxt(cx, StoryDup(cx, s), kMdCode, th.foreground)->Wrap()->W(kFill));
    return Div(a)->W(kFill)->PadB(16)->Child(box);
}

static El* Bullet(Ctx* cx, const char* s) {
    const Theme& th = cx->theme();
    return MdTxt(cx, StoryFmt(cx, "\xE2\x80\xA2  %s", s), kMd, th.foreground)
        ->Wrap()
        ->W(kFill);
}

El* WelcomeStory::Render(WelcomeStory*, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* col = Div(a)->FlexCol()->Gap(0)->W(kFill);

    // Rust TextView does not honor README <p align="center">; logo stays left.
    El* hero = Div(a)->FlexCol()->Gap(8)->W(kFill)->ItemsStart()->PadB(16);
    hero->Child(LogoMark(cx));
    hero->Child(MdTxt(cx, StrL("GPUI Component"), kMd, th.foreground)->Bold());
    col->Child(hero);

    // TextView links use theme.link, which falls back to theme.primary.
    El* langs = Div(a)->FlexRow()->Gap(4)->ItemsCenter();
    langs->Child(MdTxt(cx, StrL("English"), kMd, th.primary)
                     ->BorderB(1, th.primary));
    langs->Child(MdTxt(cx, StrL("|"), kMd, th.foreground));
    langs->Child(MdTxt(cx,
                       StrL("\xE7\xAE\x80\xE4\xBD\x93\xE4\xB8\xAD\xE6\x96\x87"),
                       kMd, th.primary)
                     ->BorderB(1, th.primary));
    col->Child(langs->PadB(16));

    El* badges = Div(a)->FlexRow()->Gap(6)->ItemsCenter();
    // The CI badge is GitHub's own, with a slate label rather than shields'.
    badges->Child(Shield(cx, "CI", "failing", Rgb(0xd4, 0x35, 0x43), true,
                         Rgb(0x3c, 0x44, 0x4d)));
    badges->Child(Shield(cx, "docs", "passing", Rgb(0x4b, 0xb6, 0x0f)));
    badges->Child(Shield(cx, "crates.io", "v0.5.1", Rgb(0xdf, 0x74, 0x3d)));
    col->Child(badges->PadB(16));

    El* blurb = Div(a)->FlexRow()->W(kFill)->Wrap()->PadB(16);
    blurb->Child(
        MdTxt(cx,
              StrL("UI components for building fantastic desktop applications "
                   "using "),
              kMd, th.foreground));
    blurb->Child(MdTxt(cx, StrL("GPUI"), kMd, th.primary)
                     ->BorderB(1, th.primary));
    blurb->Child(MdTxt(cx, StrL("."), kMd, th.foreground));
    col->Child(blurb);

    col->Child(H2(cx, "Features"));
    El* feats = Div(a)->FlexCol()->Gap(2)->W(kFill)->PadB(16);
    feats->Child(FeatureLine(cx, "Richness",
                             "60+ cross-platform desktop UI components."));
    feats->Child(FeatureLine(cx, "Native",
                             "Inspired by macOS and Windows controls, combined "
                             "with shadcn/ui design for a modern "
                             "experience."));
    feats->Child(FeatureLine(
        cx, "Ease of Use",
        "Stateless RenderOnce components, simple and user-friendly."));
    feats->Child(FeatureLine(cx, "Customizable",
                             "Built-in Theme and ThemeColor, supporting "
                             "multi-theme and variable-based configurations."));
    feats->Child(FeatureLine(cx, "Versatile",
                             "Supports sizes like xs, sm, md, and lg."));
    feats->Child(FeatureLine(cx, "Flexible Layout",
                             "Dock layout for panel arrangements, resizing, "
                             "and freeform (Tiles) layouts."));
    feats->Child(FeatureLine(cx, "High Performance",
                             "Virtualized Table and List components for smooth "
                             "large-data rendering."));
    feats->Child(FeatureLine(cx, "Content Rendering",
                             "Native support for Markdown and simple HTML."));
    feats->Child(FeatureLine(cx, "Charting",
                             "Built-in charts for visualizing your data."));
    feats->Child(FeatureLine(
        cx, "Editor",
        "High performance code editor (Up to 200K lines for stable "
        "performance) with LSP (diagnostics, completion, hover, etc)."));
    feats->Child(FeatureLine(cx, "Syntax Highlighting",
                             "Syntax highlighting for editor and markdown "
                             "components using Tree Sitter."));
    col->Child(feats);

    El* eco = Div(a)->FlexCol()->Gap(4)->W(kFill);
    eco->Child(H2(cx, "Ecosystem Architecture"));
    eco->Child(H3(cx, "Two layers. One ecosystem."));
    eco->Child(Body(cx,
                    "Choose the layer that matches how much of the "
                    "interface you want to own."));
    col->Child(eco->PadB(16));

    El* table = Div(a)->FlexCol()->W(kFill)->Border(1, th.border)->Shrink0();
    table->Child(MdTableRow(cx, "GPUI Component", "gpui-base", true));
    table->Child(MdTableRow(cx, "Complete, styled components",
                            "Unstyled behavior and infrastructure", false));
    table->Child(MdTableRow(cx, "Productive defaults with theming",
                            "Full control over structure and visual design",
                            false));
    table->Child(MdTableRow(cx, "Best for building applications",
                            "Best for building design systems", false));
    col->Child(table->PadB(16));

    El* diagram =
        Div(a)->FlexCol()->Gap(0)->W(kFill)->Pad(12)->Radius(th.radius)->Bg(
            th.muted);
    static const char* kDiagram[] = {
        "                             APPLICATION",
        "                                  \xE2\x94\x82",
        "                \xE2\x94\x8C\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\xB4"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x90",
        "                \xE2\x94\x82                                   "
        "\xE2\x94\x82",
        "                \xE2\x96\xBC                                   "
        "\xE2\x96\xBC",
        "       \xE2\x94\x8C\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x90               "
        "\xE2\x94\x8C\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x90",
        "       \xE2\x94\x82  gpui-component  \xE2\x94\x82               "
        "\xE2\x94\x82 Your Design      \xE2\x94\x82",
        "       \xE2\x94\x82    Styled UI     \xE2\x94\x82               "
        "\xE2\x94\x82 System           \xE2\x94\x82",
        "       \xE2\x94\x94\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\xAC"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x98"
        "               \xE2\x94\x94\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\xAC\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x98",
        "                \xE2\x94\x82                                  "
        "\xE2\x94\x82",
        "                \xE2\x94\x94\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\xAC\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x98",
        "                                 \xE2\x96\xBC",
        "                       \xE2\x94\x8C\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x90",
        "                       \xE2\x94\x82    gpui-base     \xE2\x94\x82",
        "                       \xE2\x94\x82 Behavior \xC2\xB7 State "
        "\xE2\x94\x82",
        "                       \xE2\x94\x82 Infrastructure   \xE2\x94\x82",
        "                       \xE2\x94\x94\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\xAC\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x98",
        "                                \xE2\x96\xBC",
        "                              GPUI",
    };
    for (int i = 0; i < (int)(sizeof(kDiagram) / sizeof(kDiagram[0])); i++) {
        diagram->Child(AsciiLine(cx, kDiagram[i]));
    }
    col->Child(diagram->PadB(16));

    col->Child(
        Quote(cx,
              "Behavior belongs to the foundation. Presentation belongs to the "
              "application."));
    col->Child(Body(
        cx,
        "Use GPUI Component when you want polished controls ready to ship. "
        "Build on gpui-base when your application should own its component "
        "source, layout, styling, and motion while reusing difficult "
        "interaction behavior."));
    col->Child(Body(cx,
                    "The layering follows the same separation that makes "
                    "the shadcn ecosystem flexible:"));

    El* eco2 = Div(a)->FlexCol()->W(kFill)->Border(1, th.border)->Shrink0();
    eco2->Child(
        MdTableRow(cx, "GPUI Component ecosystem", "Web ecosystem", true));
    eco2->Child(MdTableRow(cx, "GPUI", "HTML + Tailwind CSS", false));
    eco2->Child(MdTableRow(cx, "gpui-base", "Base UI", false));
    eco2->Child(MdTableRow(cx, "GPUI Component",
                           "shadcn's styled component layer", false));
    col->Child(eco2->PadB(16));

    col->Child(H2(cx, "Showcase"));
    col->Child(
        LinkBody(cx, "https://longbridge.github.io/gpui-component/gallery/"));
    col->Child(
        Body(cx,
             "Here is the first application: Longbridge Pro, built using "
             "GPUI Component."));

    col->Child(H2(cx, "Usage"));
    col->Child(CodeBlock(
        cx,
        "gpui = { git = \"https://github.com/zed-industries/zed\" }\n"
        "gpui_platform = { git = \"https://github.com/zed-industries/zed\", "
        "features = [\"font-kit\"] }\n"
        "gpui-component = { git = "
        "\"https://github.com/longbridge/gpui-component\" }"));

    col->Child(H3(cx, "Basic Example"));
    col->Child(Body(
        cx,
        "See examples/hello_world.cpp in this tree, or the Rust hello_world "
        "example: a centered \"Hello, World!\" label and a primary Let's Go "
        "button inside Root."));

    col->Child(H3(cx, "Icons"));
    col->Child(Body(
        cx,
        "GPUI Component has an Icon element, but it does not include SVG "
        "files by default. The example uses Lucide icons; add any icons you "
        "need to your project."));

    col->Child(H2(cx, "Development"));
    col->Child(H3(cx, "Desktop Gallery (Story)"));
    col->Child(Body(cx, "The story crate is a gallery of every component."));
    col->Child(CodeBlock(cx, "cargo run"));
    col->Child(H3(cx, "Examples"));
    col->Child(Body(cx, "Some examples are built into the story crate:"));
    col->Child(CodeBlock(cx,
                         "cargo run --example editor\n"
                         "cargo run --example dock\n"
                         "cargo run --example markdown\n"
                         "cargo run --example html"));
    col->Child(Body(
        cx,
        "Standalone crates live under examples/; run them with cargo run -p "
        "<name> (hello_world, system_monitor, window_title, \xE2\x80\xA6)."));
    col->Child(H3(cx, "Web Gallery (WASM)"));
    col->Child(CodeBlock(
        cx, "cd crates/story-web\nmake install   # first time\nmake dev"));
    col->Child(LinkBody(cx, "http://localhost:3000"));

    col->Child(H2(cx, "Compare to others"));
    El* cmp = Div(a)->FlexCol()->W(kFill)->Border(1, th.border)->Shrink0();
    cmp->Child(MdTableRow(cx, "Feature", "GPUI Component", true));
    cmp->Child(MdTableRow(cx, "Language", "Rust", false));
    cmp->Child(MdTableRow(cx, "Core Render", "GPUI", false));
    cmp->Child(MdTableRow(cx, "License", "Apache 2.0", false));
    cmp->Child(MdTableRow(cx, "Min Binary Size", "12MB", false));
    cmp->Child(MdTableRow(cx, "Cross-Platform", "Yes", false));
    cmp->Child(MdTableRow(cx, "Web", "Yes (WASM)", false));
    cmp->Child(MdTableRow(cx, "UI Style", "Modern", false));
    cmp->Child(MdTableRow(cx, "CJK Support", "Yes", false));
    cmp->Child(MdTableRow(cx, "Chart", "Yes", false));
    cmp->Child(MdTableRow(cx, "Table (Large dataset)",
                          "Yes (Virtual Rows, Columns)", false));
    cmp->Child(MdTableRow(cx, "CodeEditor", "Simple", false));
    cmp->Child(MdTableRow(cx, "Dock Layout", "Yes", false));
    cmp->Child(MdTableRow(cx, "Syntax Highlight", "Tree Sitter", false));
    cmp->Child(MdTableRow(cx, "Markdown Rendering", "Yes", false));
    cmp->Child(MdTableRow(cx, "HTML Rendering", "Basic", false));
    cmp->Child(MdTableRow(cx, "Custom Theme", "Yes", false));
    cmp->Child(MdTableRow(cx, "I18n", "Yes", false));
    col->Child(cmp->PadB(16));

    col->Child(H2(cx, "License"));
    col->Child(Body(cx, "Apache-2.0"));
    col->Child(Bullet(cx, "UI design based on shadcn/ui, some from Reui."));
    col->Child(Bullet(cx, "Icons from Lucide."));

    return col;
}

STORY_PAGE(StoryWelcome, WelcomeStory);

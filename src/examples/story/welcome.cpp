#include "Story.h"

El* WelcomeRender(StoryApp* app, Arena* a) {
    (void)app;
    const Theme& th = ThemeNow();
    El* col = Div(a)->FlexCol()->Gap(16)->W(kFill)->MaxW(720);
    col->Child(StoryTxt(a, StrL("GPUI Component"), 22, th.foreground)->Semibold());
    col->Child(StoryTxt(a, StrL("UI components for building fantastic desktop applications using GPUI."), 14, th.mutedFg)
                   ->Wrap()
                   ->MaxW(680));

    El* feats = StorySection(a, "Features", "What this library covers.");
    const char* lines[] = {
        "Richness: 60+ desktop UI components.",
        "Native: inspired by macOS and Windows controls, combined with shadcn/ui.",
        "Ease of Use: stateless components, simple and user-friendly.",
        "Customizable: built-in Theme and ThemeColor.",
        "Flexible Layout: dock, resizable panels, and tiles.",
        "High Performance: virtualized Table and List.",
        "Content Rendering: Markdown and simple HTML.",
        "Charting and a high-performance code editor.",
    };
    El* list = Div(a)->FlexCol()->Gap(6);
    for (int i = 0; i < 8; i++) {
        list->Child(StoryTxt(a, StoryDup(a, lines[i]), 13, th.foreground)->Wrap()->MaxW(640));
    }
    StorySectionAdd(feats, list);
    col->Child(feats);

    El* layers = StorySection(a, "Two layers. One ecosystem.", nullptr);
    El* row = Div(a)->FlexRow()->Gap(16)->W(kFill);
    El* left = Div(a)->FlexCol()->Gap(4)->Grow();
    left->Child(StoryTxt(a, StrL("GPUI Component"), 14, th.foreground)->Semibold());
    left->Child(StoryTxt(a, StrL("Complete, styled components. Productive defaults with theming. Best for building applications."), 13, th.mutedFg)
                    ->Wrap()
                    ->MaxW(300));
    El* right = Div(a)->FlexCol()->Gap(4)->Grow();
    right->Child(StoryTxt(a, StrL("gpui-base"), 14, th.foreground)->Semibold());
    right->Child(StoryTxt(a, StrL("Unstyled behavior and infrastructure. Full control over structure and visual design."), 13, th.mutedFg)
                     ->Wrap()
                     ->MaxW(300));
    row->Child(left)->Child(right);
    StorySectionAdd(layers, row);
    col->Child(layers);
    return col;
}

void WelcomeClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryWelcome, WelcomeRender, WelcomeClick);

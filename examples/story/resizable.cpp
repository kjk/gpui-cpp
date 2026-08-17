#include "Story.h"

El* ResizableRender(StoryApp* app, Arena* a) {
    (void)app;
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default",
                           "Accessible resizable panel groups and layouts.");
    El* nav =
        ResizablePanel::New(a)->W(140)->H(kFill)->Pad(8)->FlexCol()->Gap(4);
    nav->Child(StoryTxt(a, StrL("PROJECT"), 12, th.mutedFg));
    nav->Child(StoryTxt(a, StrL("Overview"), 13, th.foreground));
    nav->Child(StoryTxt(a, StrL("Components"), 13, th.foreground));
    El* main =
        ResizablePanel::New(a)->Grow()->H(kFill)->Pad(8)->FlexCol()->Gap(8);
    main->Child(StoryTxt(a, StrL("Workspace"), 13, th.foreground));
    main->Child(StoryTxt(a, StrL("Drag the divider to resize navigation."), 12,
                         th.mutedFg)
                    ->Wrap()
                    ->MaxW(140));
    El* box = Resizable::New(a, StrL("story-resizable"))
                  ->W(288)
                  ->H(160)
                  ->Border(1, th.border)
                  ->FlexRow()
                  ->Child(nav)
                  ->Child(Div(a)->W(1)->H(kFill)->Bg(th.border))
                  ->Child(main);
    StorySectionAdd(sec, box);
    page->Child(sec);
    return page;
}

void ResizableClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryResizable, ResizableRender, ResizableClick);

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

    El* nested = StorySection(a, "Nested Panels", nullptr);
    El* inner = Resizable::New(a, StrL("story-resizable-nested"))
                    ->W(288)
                    ->H(200)
                    ->Border(1, th.border)
                    ->FlexCol()
                    ->Child(ResizablePanel::New(a)->H(80)->Pad(8)->Child(
                        StoryTxt(a, StrL("Top"), 13, th.foreground)))
                    ->Child(Div(a)->H(1)->W(kFill)->Bg(th.border))
                    ->Child(ResizablePanel::New(a)->Grow()->Pad(8)->Child(
                        StoryTxt(a, StrL("Bottom grows"), 13, th.foreground)));
    StorySectionAdd(nested, inner);
    page->Child(nested);

    El* grow = StorySection(a, "Growing Panel", nullptr);
    El* growBox = Resizable::New(a, StrL("story-resizable-grow"))
                      ->W(288)
                      ->H(120)
                      ->Border(1, th.border)
                      ->FlexRow()
                      ->Child(ResizablePanel::New(a)->W(80)->Pad(8)->Child(
                          StoryTxt(a, StrL("Fixed"), 13, th.mutedFg)))
                      ->Child(Div(a)->W(1)->H(kFill)->Bg(th.border))
                      ->Child(ResizablePanel::New(a)->Grow()->Pad(8)->Child(
                          StoryTxt(a, StrL("Grows"), 13, th.foreground)));
    StorySectionAdd(grow, growBox);
    page->Child(grow);
    return page;
}

void ResizableClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryResizable, ResizableRender, ResizableClick);

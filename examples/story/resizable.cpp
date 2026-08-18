#include "Story.h"

struct ResizableStory {
    static El* Render(ResizableStory* self, Ctx* cx);
};

El* ResizableStory::Render(ResizableStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(cx, "Default",
                           "Accessible resizable panel groups and layouts.");
    El* nav =
        ResizablePanel::New(cx)->W(140)->H(kFill)->Pad(8)->FlexCol()->Gap(4);
    nav->Child(StoryTxt(cx, StrL("PROJECT"), 12, th.mutedFg));
    nav->Child(StoryTxt(cx, StrL("Overview"), 13, th.foreground));
    nav->Child(StoryTxt(cx, StrL("Components"), 13, th.foreground));
    El* main =
        ResizablePanel::New(cx)->Grow()->H(kFill)->Pad(8)->FlexCol()->Gap(8);
    main->Child(StoryTxt(cx, StrL("Workspace"), 13, th.foreground));
    main->Child(StoryTxt(cx, StrL("Drag the divider to resize navigation."), 12,
                         th.mutedFg)
                    ->Wrap()
                    ->MaxW(140));
    El* box = Resizable::New(cx, StrL("story-resizable"))
                  ->W(288)
                  ->H(160)
                  ->Border(1, th.border)
                  ->FlexRow()
                  ->Child(nav)
                  ->Child(Div(a)->W(1)->H(kFill)->Bg(th.border))
                  ->Child(main);
    StorySectionAdd(sec, box);
    page->Child(sec);

    El* nested = StorySection(cx, "Nested Panels", nullptr);
    El* inner = Resizable::New(cx, StrL("story-resizable-nested"))
                    ->W(288)
                    ->H(200)
                    ->Border(1, th.border)
                    ->FlexCol()
                    ->Child(ResizablePanel::New(cx)->H(80)->Pad(8)->Child(
                        StoryTxt(cx, StrL("Top"), 13, th.foreground)))
                    ->Child(Div(a)->H(1)->W(kFill)->Bg(th.border))
                    ->Child(ResizablePanel::New(cx)->Grow()->Pad(8)->Child(
                        StoryTxt(cx, StrL("Bottom grows"), 13, th.foreground)));
    StorySectionAdd(nested, inner);
    page->Child(nested);

    El* grow = StorySection(cx, "Growing Panel", nullptr);
    El* growBox = Resizable::New(cx, StrL("story-resizable-grow"))
                      ->W(288)
                      ->H(120)
                      ->Border(1, th.border)
                      ->FlexRow()
                      ->Child(ResizablePanel::New(cx)->W(80)->Pad(8)->Child(
                          StoryTxt(cx, StrL("Fixed"), 13, th.mutedFg)))
                      ->Child(Div(a)->W(1)->H(kFill)->Bg(th.border))
                      ->Child(ResizablePanel::New(cx)->Grow()->Pad(8)->Child(
                          StoryTxt(cx, StrL("Grows"), 13, th.foreground)));
    StorySectionAdd(grow, growBox);
    page->Child(grow);
    return page;
}

STORY_PAGE(StoryResizable, ResizableStory);

#include "Story.h"

struct NativeMenuStory {
    static El* Render(NativeMenuStory* self, Ctx* cx);
    static void Click(NativeMenuStory* self, Ctx* cx, int id);
};

El* NativeMenuStory::Render(NativeMenuStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* builder = StorySection(cx, "Builder API", nullptr);
    StorySectionAdd(builder, component::Menu::New(cx)
                                 ->Item(StrL("File"))
                                 ->Item(StrL("Edit"))
                                 ->Item(StrL("View"))
                                 ->IntoEl());
    page->Child(builder);

    El* items = StorySection(cx, "Menu Items", nullptr);
    StorySectionAdd(items, component::Menu::New(cx)
                               ->Item(StrL("New"))
                               ->Item(StrL("Open…"))
                               ->Item(StrL("Save"))
                               ->Item(StrL("Quit"))
                               ->IntoEl());
    page->Child(items);

    El* drop = StorySection(cx, "Dropdown", nullptr);
    StorySectionAdd(
        drop, StoryTxt(cx,
                       StrL("Native application menus are not wired on this "
                            "Win32 port. Use the in-window Menu story."),
                       13, ThemeNow().mutedFg)
                  ->Wrap()
                  ->MaxW(420));
    page->Child(drop);
    return page;
}

void NativeMenuStory::Click(NativeMenuStory* self, Ctx* cx, int id) {
    (void)cx;
    (void)id;
}

STORY_PAGE(StoryNativeMenu, NativeMenuStory);

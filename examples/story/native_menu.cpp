#include "Story.h"

El* NativeMenuRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* builder = StorySection(a, "Builder API", nullptr);
    StorySectionAdd(builder, component::Menu::New(a)
                                 ->Item(StrL("File"))
                                 ->Item(StrL("Edit"))
                                 ->Item(StrL("View"))
                                 ->IntoEl());
    page->Child(builder);

    El* items = StorySection(a, "Menu Items", nullptr);
    StorySectionAdd(items, component::Menu::New(a)
                               ->Item(StrL("New"))
                               ->Item(StrL("Open…"))
                               ->Item(StrL("Save"))
                               ->Item(StrL("Quit"))
                               ->IntoEl());
    page->Child(items);

    El* drop = StorySection(a, "Dropdown", nullptr);
    StorySectionAdd(
        drop, StoryTxt(a,
                       StrL("Native application menus are not wired on this "
                            "Win32 port. Use the in-window Menu story."),
                       13, ThemeNow().mutedFg)
                  ->Wrap()
                  ->MaxW(420));
    page->Child(drop);
    return page;
}

void NativeMenuClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryNativeMenu, NativeMenuRender, NativeMenuClick);

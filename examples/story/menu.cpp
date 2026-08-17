#include "Story.h"

El* MenuRender(StoryApp* app, Arena* a) {
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* popup = StorySection(a, "Popup Menu", nullptr);
    StorySectionAdd(popup, component::Menu::New(a)
                               ->Item(StrL("New File"))
                               ->Item(StrL("Open…"))
                               ->Item(StrL("Save"))
                               ->Item(StrL("Quit"))
                               ->IntoEl());
    page->Child(popup);

    El* ctx = StorySection(a, "Context Menu", nullptr);
    StorySectionAdd(ctx, component::Menu::New(a)
                             ->Item(StrL("Cut"))
                             ->Item(StrL("Copy"))
                             ->Item(StrL("Paste"))
                             ->IntoEl());
    page->Child(ctx);

    El* scroll = StorySection(a, "Scrollable", nullptr);
    component::Menu* m = component::Menu::New(a);
    for (int i = 1; i <= 8; i++) {
        m->Item(StoryFmt(a, "Item %d", i));
    }
    StorySectionAdd(scroll, m->IntoEl());
    page->Child(scroll);
    return page;
}

void MenuClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryMenu, MenuRender, MenuClick);

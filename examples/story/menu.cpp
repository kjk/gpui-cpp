#include "Story.h"

struct MenuStory {
    static El* Render(MenuStory* self, Ctx* cx);
};

El* MenuStory::Render(MenuStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* popup = StorySection(cx, "Popup Menu", nullptr);
    StorySectionAdd(popup, component::Menu::New(cx)
                               ->Item(StrL("New File"))
                               ->Item(StrL("Open…"))
                               ->Item(StrL("Save"))
                               ->Item(StrL("Quit"))
                               ->IntoEl());
    page->Child(popup);

    El* ctx = StorySection(cx, "Context Menu", nullptr);
    StorySectionAdd(ctx, component::Menu::New(cx)
                             ->Item(StrL("Cut"))
                             ->Item(StrL("Copy"))
                             ->Item(StrL("Paste"))
                             ->IntoEl());
    page->Child(ctx);

    El* scroll = StorySection(cx, "Scrollable", nullptr);
    component::Menu* m = component::Menu::New(cx);
    for (int i = 1; i <= 8; i++) {
        m->Item(StoryFmt(cx, "Item %d", i));
    }
    StorySectionAdd(scroll, m->IntoEl());
    page->Child(scroll);
    return page;
}

STORY_PAGE(StoryMenu, MenuStory);

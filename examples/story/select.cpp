#include "Story.h"

struct SelectStory {
    int selectIx = 0;
    bool selectOpen = false;
    int selB = -1;

    static El* Render(SelectStory* self, Ctx* cx);
    static void OnKey(SelectStory* self, Ctx* cx, const KeyEvent* ev);
};

static void ToggleSel(SelectStory* self, Ctx* cx, const ClickEvent*) {
    self->selectOpen = !self->selectOpen;
}
static void PickSel(SelectStory* self, Ctx* cx, const ClickEvent*, intptr_t i) {
    self->selectIx = i;
    self->selectOpen = false;
}

static component::Select* Framework(Ctx* cx, SelectStory* self,
                                    const char* id) {
    Arena* a = cx->a;
    return component::Select::New(cx, Str(id))
        ->Option(StrL("GPUI"))
        ->Option(StrL("React"))
        ->Option(StrL("SwiftUI"))
        ->Option(StrL("Vue"))
        ->Selected(self->selectIx)
        ->Open(self->selectOpen && self->selB == (int)id[0])
        ->OnToggle(Listen(cx, &ToggleSel))
        ->OnChange(Listen(cx, &PickSel));
}

El* SelectStory::Render(SelectStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* search = StorySection(cx, "Search and clear", nullptr);
    StorySectionAdd(search, Framework(cx, self, "framework")->IntoEl());
    page->Child(search);

    El* width = StorySection(cx, "Menu width", nullptr);
    StorySectionAdd(width, Framework(cx, self, "width")->IntoEl());
    page->Child(width);

    El* dis = StorySection(cx, "Disabled", nullptr);
    StorySectionAdd(dis, Framework(cx, self, "disabled")->IntoEl());
    page->Child(dis);

    El* prefix = StorySection(cx, "Title prefix", nullptr);
    StorySectionAdd(prefix, Framework(cx, self, "prefix")->IntoEl());
    page->Child(prefix);

    El* empty = StorySection(cx, "Empty", nullptr);
    StorySectionAdd(empty, component::Select::New(cx, StrL("empty"))
                               ->Open(false)
                               ->IntoEl());
    page->Child(empty);
    return page;
}

// Esc closes what this page has open, like an overlay dismiss.
void SelectStory::OnKey(SelectStory* self, Ctx* cx, const KeyEvent* ev) {
    if (ev->vk != VK_ESCAPE) {
        return;
    }
    self->selectOpen = false;
    Notify(cx);
}

STORY_PAGE_KEYS(StorySelect, SelectStory);

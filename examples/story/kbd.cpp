#include "Story.h"

struct KbdStory {
    static El* Render(KbdStory* self, Ctx* cx);
};

El* KbdStory::Render(KbdStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    const char* keys[] = {"⌘⇧P", "⌘⌃T", "⌘−", "⌘+", "Esc", "⌫", "/", "Enter"};
    El* def = StorySection(cx, "Default",
                           "Displays single keys and multi-key shortcuts.");
    El* row = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    for (int i = 0; i < 8; i++) {
        row->Child(component::Kbd::New(cx, Str(keys[i]))->IntoEl());
    }
    StorySectionAdd(def, row);
    page->Child(def);

    El* out =
        StorySection(cx, "Outlined",
                     "An outlined treatment adds emphasis on dense surfaces.");
    El* row2 = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    row2->Child(component::Kbd::New(cx, StrL("⌘⇧P"))->Outline()->IntoEl());
    row2->Child(component::Kbd::New(cx, StrL("⌘⌃T"))->Outline()->IntoEl());
    row2->Child(component::Kbd::New(cx, StrL("Enter"))->Outline()->IntoEl());
    StorySectionAdd(out, row2);
    page->Child(out);
    return page;
}

STORY_PAGE(StoryKbd, KbdStory);

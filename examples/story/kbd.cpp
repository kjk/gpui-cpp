#include "Story.h"

El* KbdRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    const char* keys[] = {"⌘⇧P", "⌘⌃T", "⌘−", "⌘+", "Esc", "⌫", "/", "Enter"};
    El* def = StorySection(a, "Default",
                           "Displays single keys and multi-key shortcuts.");
    El* row = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    for (int i = 0; i < 8; i++) {
        row->Child(component::Kbd::New(a, Str(keys[i]))->IntoEl());
    }
    StorySectionAdd(def, row);
    page->Child(def);

    El* out =
        StorySection(a, "Outlined",
                     "An outlined treatment adds emphasis on dense surfaces.");
    El* row2 = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    row2->Child(component::Kbd::New(a, StrL("⌘⇧P"))->Outline()->IntoEl());
    row2->Child(component::Kbd::New(a, StrL("⌘⌃T"))->Outline()->IntoEl());
    row2->Child(component::Kbd::New(a, StrL("Enter"))->Outline()->IntoEl());
    StorySectionAdd(out, row2);
    page->Child(out);
    return page;
}

void KbdClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryKbd, KbdRender, KbdClick);

#include "Story.h"

struct KbdStory {
    static El* Render(KbdStory* self, Ctx* cx);
};

El* KbdStory::Render(KbdStory*, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    // Kbd::format spells the modifiers out on Windows, in Ctrl+Alt+Shift+Win
    // order: the same keystrokes the Rust story builds.
    const char* keys[] = {"Shift+Win+P", "Ctrl+Win+T", "Win+-", "Win++",
                          "Esc",         "Backspace",  "/",     "Enter"};
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
    row2->Child(
        component::Kbd::New(cx, StrL("Shift+Win+P"))->Outline()->IntoEl());
    row2->Child(
        component::Kbd::New(cx, StrL("Ctrl+Win+T"))->Outline()->IntoEl());
    row2->Child(component::Kbd::New(cx, StrL("Enter"))->Outline()->IntoEl());
    StorySectionAdd(out, row2);
    page->Child(out);
    return page;
}

STORY_PAGE(StoryKbd, KbdStory);

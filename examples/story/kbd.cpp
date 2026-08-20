#include "Story.h"

struct KbdStory {
    static El* Render(KbdStory* self, Ctx* cx);
};

// The keystrokes the Rust story builds. Kbd::format is what turns one into
// the spelling this platform uses — ⌃⌥⇧⌘ run together on macOS, and
// Ctrl+Alt+Shift+Win joined with a plus everywhere else.
static const component::Keystroke kStrokes[] = {
    {false, false, true, true, StrL("p")},
    {true, false, false, true, StrL("t")},
    {false, false, false, true, StrL("-")},
    {false, false, false, true, StrL("=")},
    {false, false, false, false, StrL("escape")},
    {false, false, false, false, StrL("backspace")},
    {false, false, false, false, StrL("/")},
    {false, false, false, false, StrL("enter")},
};

El* KbdStory::Render(KbdStory*, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* def = StorySection(cx, "Default",
                           "Displays single keys and multi-key shortcuts.");
    El* row = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    for (int i = 0; i < 8; i++) {
        row->Child(component::Kbd::New(cx, kStrokes[i])->IntoEl());
    }
    StorySectionAdd(def, row);
    page->Child(def);

    El* out =
        StorySection(cx, "Outlined",
                     "An outlined treatment adds emphasis on dense surfaces.");
    El* row2 = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    for (int i = 0; i < 3; i++) {
        row2->Child(component::Kbd::New(cx, kStrokes[i == 2 ? 7 : i])
                        ->Outline()
                        ->IntoEl());
    }
    StorySectionAdd(out, row2);
    page->Child(out);

    El* named =
        StorySection(cx, "Named keys",
                     "Arrows, paging and the editing keys have names of their "
                     "own.");
    El* row3 = Div(a)->FlexRow()->Gap(8)->ItemsCenter()->FlexWrap();
    static const char* kNamed[] = {"left",   "right",    "up",     "down",
                                   "pageup", "pagedown", "delete", "space"};
    for (int i = 0; i < 8; i++) {
        component::Keystroke k;
        k.key = Str(kNamed[i]);
        row3->Child(component::Kbd::New(cx, k)->IntoEl());
    }
    StorySectionAdd(named, row3);
    page->Child(named);
    return page;
}

STORY_PAGE(StoryKbd, KbdStory);

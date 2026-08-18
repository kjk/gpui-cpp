#include "Story.h"

struct ClipboardStory {
    static El* Render(ClipboardStory* self, Ctx* cx);
};

static void OnCopy(ClipboardStory* self, Ctx* cx, const ClickEvent*,
                   intptr_t v) {
    (void)v;
}

El* ClipboardStory::Render(ClipboardStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* page = Div(a)->FlexCol()->Gap(12)->W(kFill);

    El* def = StorySection(cx, "Default",
                           "Copies a value supplied by the application.");
    El* defRow = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    defRow->Child(component::Label::New(cx, StrL("A clipboard button"))
                      ->IntoEl());
    defRow->Child(component::Clipboard::New(cx, StrL("masked :false"))
                      ->OnCopy(Listen(cx, &OnCopy))
                      ->IntoEl());
    StorySectionAdd(def, defRow);
    page->Child(def);

    El* input =
        StorySection(cx, "With Input", "Copies the field's current value.");
    El* field = Div(a)
                    ->FlexRow()
                    ->ItemsCenter()
                    ->W(360)
                    ->H(32)
                    ->PadX(8)
                    ->Gap(8)
                    ->Border(1, th.border)
                    ->Radius(th.radius)
                    ->Bg(th.background);
    field->Child(StoryTxt(cx, StrL("https://github.com"), 13, th.foreground)
                     ->Grow());
    field->Child(component::Button::New(cx, StrL("clipboard2"))
                     ->Icon(IconName::Copy)
                     ->Ghost()
                     ->Tooltip(StrL("Copy"))
                     ->IntoEl());
    StorySectionAdd(input, field);
    page->Child(input);
    return page;
}

STORY_PAGE(StoryClipboard, ClipboardStory);

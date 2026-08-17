#include "Story.h"

static void OnCopy(StoryApp* app, Str v) {
    (void)app;
    (void)v;
}

El* ClipboardRender(StoryApp* app, Arena* a) {
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* def = StorySection(a, "Default",
                           "Copies a value supplied by the application.");
    El* defRow = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    defRow
        ->Child(component::Label::New(a, StrL("A clipboard button"))->IntoEl());
    defRow->Child(component::Clipboard::New(a, StrL("masked :false"))
                      ->OnCopy(MkFunc1(&OnCopy, app))
                      ->IntoEl());
    StorySectionAdd(def, defRow);
    page->Child(def);

    El* input =
        StorySection(a, "With Input", "Copies the field's current value.");
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
    field->Child(StoryTxt(a, StrL("https://github.com"), 13, th.foreground)
                     ->Grow());
    field->Child(component::Button::New(a, StrL("clipboard2"))
                     ->Icon(IconName::Copy)
                     ->Ghost()
                     ->Tooltip(StrL("Copy"))
                     ->IntoEl());
    StorySectionAdd(input, field);
    page->Child(input);
    return page;
}

void ClipboardClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryClipboard, ClipboardRender, ClipboardClick);

#include "Story.h"

static void OnCopy(StoryApp* app, Str v) {
    (void)app;
    (void)v;
}

El* ClipboardRender(StoryApp* app, Arena* a) {
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default", "A button that copies text to the clipboard.");
    StorySectionAdd(sec, component::Clipboard::New(a, StrL("gpui-component"))->OnCopy(MkFunc1(&OnCopy, app))->IntoEl());
    page->Child(sec);
    return page;
}

void ClipboardClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryClipboard, ClipboardRender, ClipboardClick);

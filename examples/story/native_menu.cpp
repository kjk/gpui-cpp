#include "Story.h"

El* NativeMenuRender(StoryApp* app, Arena* a) {
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(
        a, "Windows",
        "Native application menus are not wired on this Win32 port.");
    StorySectionAdd(
        sec,
        StoryTxt(a,
                 StrL("Use the in-window Menu story for application actions."),
                 13, ThemeNow().mutedFg));
    page->Child(sec);
    return page;
}

void NativeMenuClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryNativeMenu, NativeMenuRender, NativeMenuClick);

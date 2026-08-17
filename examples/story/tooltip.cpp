#include "Story.h"

El* TooltipRender(StoryApp* app, Arena* a) {
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default",
                           "A popup that displays information on hover.");
    StorySectionAdd(sec, component::Button::New(a, StrL("tip-btn"))
                             ->Label(StrL("Hover me"))
                             ->Outline()
                             ->Tooltip(StrL("Save the document"))
                             ->IntoEl());
    (void)th;
    page->Child(sec);
    return page;
}

void TooltipClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryTooltip, TooltipRender, TooltipClick);

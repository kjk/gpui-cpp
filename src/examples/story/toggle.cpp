#include "Story.h"

enum { ClickToggleStory = 2300 };

El* ToggleRender(StoryApp* app, Arena* a) {
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default", "A two-state button that can be either on or off.");
    El* t = Toggle::New(a, StrL("italic"), ClickToggleStory)
                ->H(32)
                ->PadX(12)
                ->ItemsCenter()
                ->JustifyCenter()
                ->Radius(6)
                ->Border(1, th.border)
                ->Child(StoryTxt(a, StrL("Italic"), 13, th.foreground));
    if (app->toggleOn) {
        t->Bg(th.accent);
    }
    StorySectionAdd(sec, t);
    page->Child(sec);
    return page;
}

void ToggleClick(StoryApp* app, int id) {
    if (id == ClickToggleStory) {
        app->toggleOn = !app->toggleOn;
    }
}

STORY_PAGE(StoryToggle, ToggleRender, ToggleClick);

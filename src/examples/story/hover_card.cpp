#include "Story.h"

El* HoverCardRender(StoryApp* app, Arena* a) {
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default", "For sighted users to preview content available behind a link.");
    El* trigger = StoryTxt(a, StrL("Hover over gpui-base"), 13, th.foreground);
    trigger->Click(1);
    El* card = nullptr;
    if (app->hoverId) {
        card = Div(a)->W(240)->Pad(12)->FlexCol()->Gap(4)->Border(1, th.border)->Bg(th.background)->Radius(th.radius);
        card->Child(StoryTxt(a, StrL("gpui-base"), 14, th.foreground)->Semibold());
        card->Child(StoryTxt(a, StrL("Unstyled primitives for building design systems."), 12, th.mutedFg)->Wrap()->MaxW(220));
    }
    StorySectionAdd(sec, component::HoverCard::New(a)->Trigger(trigger)->Content(card)->Open(app->hoverId != 0)->IntoEl());
    page->Child(sec);
    return page;
}

void HoverCardClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryHoverCard, HoverCardRender, HoverCardClick);

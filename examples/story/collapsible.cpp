#include "Story.h"

enum { ClickCollTrigger = 2400 };

El* CollapsibleRender(StoryApp* app, Arena* a) {
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default", "An interactive component which expands/collapses a panel.");
    El* trigger = Div(a)
                      ->FlexRow()
                      ->W(320)
                      ->H(32)
                      ->ItemsCenter()
                      ->JustifyBetween()
                      ->Click(ClickCollTrigger)
                      ->Child(StoryTxt(a, StrL("@longbridge/gpui-component"), 13, th.foreground))
                      ->Child(IconEl(a, app->collapsibleOpen ? IconName::ChevronDown : IconName::ChevronRight, 14)->Fg(th.mutedFg));
    El* body = Div(a)->Pad(8)->W(320)->Child(
        StoryTxt(a, StrL("A collection of UI components for building fantastic desktop applications."), 13, th.mutedFg)
            ->Wrap()
            ->MaxW(300));
    StorySectionAdd(sec, component::Collapsible::New(a)->Open(app->collapsibleOpen)->Trigger(trigger)->Content(body)->IntoEl());
    page->Child(sec);
    return page;
}

void CollapsibleClick(StoryApp* app, int id) {
    if (id == ClickCollTrigger) {
        app->collapsibleOpen = !app->collapsibleOpen;
    }
}

STORY_PAGE(StoryCollapsible, CollapsibleRender, CollapsibleClick);

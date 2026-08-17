#include "Story.h"

enum {
    ClickStoryPop = 2740
};

El* PopoverRender(StoryApp* app, Arena* a) {
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(
        a, "Default",
        "Displays rich content in a portal, triggered by a button.");
    El* trigger = component::Button::New(a, StrL("open-pop"))
                      ->Label(StrL("Open popover"))
                      ->Outline()
                      ->IntoEl();
    El* content = nullptr;
    if (app->selectOpen) {
        content = Div(a)
                      ->W(220)
                      ->Pad(12)
                      ->FlexCol()
                      ->Gap(4)
                      ->Border(1, th.border)
                      ->Bg(th.background)
                      ->Radius(th.radius);
        content->Child(StoryTxt(a, StrL("Dimensions"), 14, th.foreground)
                           ->Semibold());
        content
            ->Child(StoryTxt(a, StrL("Set the width and height of the layer."),
                             12, th.mutedFg)
                        ->Wrap()
                        ->MaxW(200));
    }
    StorySectionAdd(sec, component::Popover::New(a)
                             ->Trigger(trigger)
                             ->Content(content)
                             ->Open(app->selectOpen)
                             ->IntoEl());
    page->Child(sec);
    return page;
}

void PopoverClick(StoryApp* app, int id) {
    if (id == ClickStoryPop || id == HashClickId(StrL("open-pop"))) {
        app->selectOpen = !app->selectOpen;
    }
}

STORY_PAGE(StoryPopover, PopoverRender, PopoverClick);

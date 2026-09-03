#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

static void TogglePopover(ShowcaseApp* app, Ctx* cx, const ClickEvent*) {
    app->popoverOpen = !app->popoverOpen;
    Notify(cx);
}

static void ClosePopover(ShowcaseApp* app, Ctx* cx, const ClickEvent*) {
    app->popoverOpen = false;
    Notify(cx);
}

El* ShowcasePopover(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    El* trigger = Button::New(cx, StrL("popover-trigger"))
                      ->OnClick(Listen(cx, &TogglePopover))
                      ->H(28)
                      ->PadX(12)
                      ->ItemsCenter()
                      ->JustifyCenter()
                      ->Bg(Rgb(0, 0, 0))
                      ->Child(TextEl(a, StrL("Open Popover"))
                                  ->Font(12)
                                  ->Fg(ExampleRgb(0xffffff)));
    El* content = nullptr;
    if (app->popoverOpen) {
        content = Div(a)
                      ->W(256)
                      ->Pad(8)
                      ->FlexCol()
                      ->Gap(8)
                      ->Bg(ExampleRgb(0xffffff))
                      ->Border(1, ExampleRgb(0xd4d4d4))
                      ->Child(TextEl(a, StrL("Workspace access"))
                                  ->Font(12)
                                  ->Fg(ExampleRgb(0x171717)))
                      ->Child(TextEl(a, StrL("Anyone with the link can view."))
                                  ->Font(12)
                                  ->Fg(ExampleRgb(0x737373)))
                      ->Child(Div(a)->FlexRow()->JustifyEnd()->Child(
                          Button::New(cx, StrL("popover-done"))
                              ->OnClick(Listen(cx, &ClosePopover))
                              ->H(28)
                              ->PadX(12)
                              ->ItemsCenter()
                              ->JustifyCenter()
                              ->Bg(Rgb(0, 0, 0))
                              ->Child(TextEl(a, StrL("Done"))
                                          ->Font(12)
                                          ->Fg(ExampleRgb(0xffffff)))));
    }
    return Popover::New(cx, StrL("example-popover"))
        ->Trigger(trigger)
        ->Content(content)
        ->IntoEl();
}

SHOWCASE_PAGE(CompPopover, ShowcasePopover);

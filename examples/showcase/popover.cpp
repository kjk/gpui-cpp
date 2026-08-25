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
                                  ->Fg(Rgb(0xff, 0xff, 0xff)));
    El* content = nullptr;
    if (app->popoverOpen) {
        content = Div(a)
                      ->W(256)
                      ->Pad(8)
                      ->FlexCol()
                      ->Gap(8)
                      ->Bg(Rgb(0xff, 0xff, 0xff))
                      ->Border(1, Rgb(0xd4, 0xd4, 0xd4))
                      ->Child(TextEl(a, StrL("Workspace access"))
                                  ->Font(12)
                                  ->Fg(Rgb(0x17, 0x17, 0x17)))
                      ->Child(TextEl(a, StrL("Anyone with the link can view."))
                                  ->Font(12)
                                  ->Fg(Rgb(0x73, 0x73, 0x73)))
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
                                          ->Fg(Rgb(0xff, 0xff, 0xff)))));
    }
    return Popover::New(cx, StrL("example-popover"))
        ->Trigger(trigger)
        ->Content(content)
        ->IntoEl();
}

SHOWCASE_PAGE(CompPopover, ShowcasePopover);

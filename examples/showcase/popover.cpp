#include "Showcase.h"
#include "gpui.h"

enum {
    ClickPopover = 440,
    ClickPopoverDone = 441
};

El* ShowcasePopover(ShowcaseApp* app, Arena* a) {
    El* trigger = Button::New(a, StrL("popover-trigger"), ClickPopover)
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
                          Button::New(a, StrL("popover-done"), ClickPopoverDone)
                              ->H(28)
                              ->PadX(12)
                              ->ItemsCenter()
                              ->JustifyCenter()
                              ->Bg(Rgb(0, 0, 0))
                              ->Child(TextEl(a, StrL("Done"))
                                          ->Font(12)
                                          ->Fg(Rgb(0xff, 0xff, 0xff)))));
    }
    return Popover::New(a, StrL("example-popover"))
        ->Trigger(trigger)
        ->Content(content)
        ->IntoEl();
}

void ShowcasePopoverClick(ShowcaseApp* app, int id) {
    if (id == ClickPopover) {
        app->popoverOpen = !app->popoverOpen;
    } else if (id == ClickPopoverDone) {
        app->popoverOpen = false;
    }
}

SHOWCASE_PAGE(CompPopover, ShowcasePopover, ShowcasePopoverClick);

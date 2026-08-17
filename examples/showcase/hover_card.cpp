#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

enum {
    ClickHover = 370
};

El* ShowcaseHoverCard(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    El* trigger = Div(a)
                      ->Id(StrL("hover-trigger"))
                      ->PadX(12)
                      ->PadY(4)
                      ->Click(ClickHover)
                      ->FocusId(ClickHover)
                      ->Child(TextEl(a, StrL("Hover over gpui-base"))
                                  ->Font(12)
                                  ->Fg(Rgb(0x17, 0x17, 0x17))
                                  ->BorderB(1, Rgb(0x17, 0x17, 0x17)));
    El* content = nullptr;
    if (app->hoverId == ClickHover) {
        content =
            Div(a)
                ->Id(StrL("hover-content"))
                ->W(210)
                ->Pad(8)
                ->FlexCol()
                ->Bg(Rgb(0xff, 0xff, 0xff))
                ->Border(1, Rgb(0xd4, 0xd4, 0xd4))
                ->Child(
                    Div(a)
                        ->FlexRow()
                        ->ItemsCenter()
                        ->Gap(8)
                        ->Child(Div(a)
                                    ->W(28)
                                    ->H(28)
                                    ->ItemsCenter()
                                    ->JustifyCenter()
                                    ->Border(1, Rgb(0x17, 0x17, 0x17))
                                    ->Child(TextEl(a, StrL("G"))
                                                ->Font(14)
                                                ->Fg(Rgb(0x17, 0x17, 0x17))))
                        ->Child(Div(a)
                                    ->FlexCol()
                                    ->Child(TextEl(a, StrL("gpui-base"))
                                                ->Font(14)
                                                ->Fg(Rgb(0x17, 0x17, 0x17)))
                                    ->Child(TextEl(a, StrL("@gpui-base"))
                                                ->Font(14)
                                                ->Fg(Rgb(0x73, 0x73, 0x73)))))
                ->Child(Div(a)->PadT(8)->Child(
                    TextEl(a, StrL("Unstyled primitives for GPUI."))
                        ->Font(14)
                        ->Fg(Rgb(0x73, 0x73, 0x73))));
    }
    return HoverCard::New(cx, StrL("example-hover-card"))
        ->Trigger(trigger)
        ->Content(content)
        ->IntoEl();
}

void ShowcaseHoverCardClick(ShowcaseApp* app, int id) {
    (void)app;
    (void)id;
}

SHOWCASE_PAGE(CompHoverCard, ShowcaseHoverCard, ShowcaseHoverCardClick);

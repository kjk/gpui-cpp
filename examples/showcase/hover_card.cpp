#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

El* ShowcaseHoverCard(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    (void)app;
    El* trigger = Div(a)
                      ->PathClick(StrL("hover-trigger"))
                      ->PadX(12)
                      ->PadY(4)
                      ->Child(TextEl(a, StrL("Hover over gpui-base"))
                                  ->Font(12)
                                  ->Fg(Rgb(0x17, 0x17, 0x17))
                                  ->BorderB(1, Rgb(0x17, 0x17, 0x17)));
    HoverCard* card = HoverCard::New(cx, StrL("example-hover-card"));
    card->Trigger(trigger);
    // `.content(|_, _, _| ..)`: the builder runs only while the card is up, so
    // a closed card builds nothing. The card is what knows — it holds the two
    // delays and both hover reports — rather than the page asking the window
    // what the pointer is over.
    if (card->IsOpen()) {
        card->Content(
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
                        ->Fg(Rgb(0x73, 0x73, 0x73)))));
    }
    return card->IntoEl();
}

SHOWCASE_PAGE(CompHoverCard, ShowcaseHoverCard);

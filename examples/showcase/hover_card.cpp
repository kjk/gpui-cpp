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
                                  ->Fg(ExampleRgb(0x171717))
                                  ->BorderB(1, ExampleRgb(0x171717)));
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
                ->Bg(ExampleRgb(0xffffff))
                ->Border(1, ExampleRgb(0xd4d4d4))
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
                                    ->Border(1, ExampleRgb(0x171717))
                                    ->Child(TextEl(a, StrL("G"))
                                                ->Font(14)
                                                ->Fg(ExampleRgb(0x171717))))
                        ->Child(Div(a)
                                    ->FlexCol()
                                    ->Child(TextEl(a, StrL("gpui-base"))
                                                ->Font(14)
                                                ->Fg(ExampleRgb(0x171717)))
                                    ->Child(TextEl(a, StrL("@gpui-base"))
                                                ->Font(14)
                                                ->Fg(ExampleRgb(0x737373)))))
                ->Child(Div(a)->PadT(8)->Child(
                    TextEl(a, StrL("Unstyled primitives for GPUI."))
                        ->Font(14)
                        ->Fg(ExampleRgb(0x737373)))));
    }
    return card->IntoEl();
}

SHOWCASE_PAGE(CompHoverCard, ShowcaseHoverCard);

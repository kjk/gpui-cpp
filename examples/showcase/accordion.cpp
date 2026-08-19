#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

static void ToggleAcc(ShowcaseApp* app, Ctx* cx, const ClickEvent*,
                      intptr_t i) {
    app->accordionOpen[i] = !app->accordionOpen[i];
    Notify(cx);
}

El* ShowcaseAccordion(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    static const char* qs[] = {
        "What is GPUI Base?",
        "Can I bring my own theme?",
        "Does it support keyboard input?",
    };
    static const char* as[] = {
        "Unstyled, accessible primitives for building native GPUI interfaces.",
        "Yes. Every visual detail remains application-owned.",
        "Focus, activation, and semantic state are built into the primitives.",
    };

    El* root = Accordion::New(cx, StrL("example-accordion"))
                   ->W(270)
                   ->BorderT(1, Rgb(0xd4, 0xd4, 0xd4))
                   ->FlexCol();
    for (int i = 0; i < 3; i++) {
        bool open = app->accordionOpen[i];
        El* trigger =
            AccordionTrigger::New(cx, DupFmt(cx, "accordion-trigger-%d", i),
                                  open, false, Listen(cx, &ToggleAcc, i))
                ->FlexRow()
                ->W(kFill)
                ->H(28)
                ->ItemsCenter()
                ->JustifyBetween()
                ->BorderB(1, Rgb(0xd4, 0xd4, 0xd4))
                ->HoverBg(Rgb(0xf5, 0xf5, 0xf5))
                ->Child(
                    TextEl(a, Str(qs[i]))->Font(12)->Fg(Rgb(0x17, 0x17, 0x17)))
                ->Child(TextEl(a, open ? StrL("−") : StrL("+"))
                            ->Font(12)
                            ->Fg(Rgb(0x73, 0x73, 0x73)));
        AccordionItem* item = AccordionItem::New(cx)
                                  ->Open(open)
                                  ->Header(AccordionHeader::New(cx, trigger));
        item->Panel(AccordionPanel::New(cx)
                        ->PadX(4)
                        ->PadY(4)
                        ->W(kFill)
                        ->BorderB(1, Rgb(0xd4, 0xd4, 0xd4))
                        ->Child(TextEl(a, Str(as[i]))
                                    ->Font(12)
                                    ->Fg(Rgb(0x52, 0x52, 0x52))
                                    ->Wrap()
                                    ->MaxW(262)));
        root->Child(item->IntoEl());
    }
    return root;
}

SHOWCASE_PAGE(CompAccordion, ShowcaseAccordion);

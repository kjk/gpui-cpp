#include "Showcase.h"
#include "ui/Accordion.h"

enum { ClickAcc0 = 200, ClickAcc1 = 201, ClickAcc2 = 202 };

El* ShowcaseAccordion(ShowcaseApp* app, Arena* a) {
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

    El* root = Accordion::New(a, StrL("example-accordion"))->W(270)->BorderT(1, Rgb(0xd4, 0xd4, 0xd4))->FlexCol();
    for (int i = 0; i < 3; i++) {
        bool open = app->accordionOpen[i];
        El* trigger = AccordionTrigger::New(a, DupFmt(a, "accordion-trigger-%d", i), ClickAcc0 + i)
                          ->FlexRow()
                          ->W(kFill)
                          ->H(28)
                          ->ItemsCenter()
                          ->JustifyBetween()
                          ->BorderB(1, Rgb(0xd4, 0xd4, 0xd4))
                          ->HoverBg(Rgb(0xf5, 0xf5, 0xf5))
                          ->Child(TextEl(a, Str(qs[i]))->Font(12)->Fg(Rgb(0x17, 0x17, 0x17)))
                          ->Child(TextEl(a, open ? StrL("−") : StrL("+"))->Font(12)->Fg(Rgb(0x73, 0x73, 0x73)));
        AccordionItem* item = AccordionItem::New(a)->Open(open)->Header(AccordionHeader::New(a, trigger));
        item->Panel(AccordionPanel::New(a)
                        ->PadX(4)
                        ->PadY(4)
                        ->W(kFill)
                        ->BorderB(1, Rgb(0xd4, 0xd4, 0xd4))
                        ->Child(TextEl(a, Str(as[i]))->Font(12)->Fg(Rgb(0x52, 0x52, 0x52))->Wrap()->MaxW(262)));
        root->Child(item->IntoEl());
    }
    return root;
}

void ShowcaseAccordionClick(ShowcaseApp* app, int id) {
    if (id >= ClickAcc0 && id <= ClickAcc2) {
        int i = id - ClickAcc0;
        app->accordionOpen[i] = !app->accordionOpen[i];
    }
}

SHOWCASE_PAGE(CompAccordion, ShowcaseAccordion, ShowcaseAccordionClick);


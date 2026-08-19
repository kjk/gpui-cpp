/* Unstyled accordion — crates/base/src/accordion.rs */

#include "gpui/gpui.h"

namespace gpui {

struct Accordion {
    static El* New(Ctx* cx, Str id);
};

struct AccordionTrigger {
    static El* New(Ctx* cx, Str id, int clickId = 0);
};

struct AccordionHeader {
    static El* New(Ctx* cx, El* trigger);
};

struct AccordionPanel {
    static El* New(Ctx* cx);
};

struct AccordionItem {
    El* root = nullptr;
    bool open = false;

    static AccordionItem* New(Ctx* cx);
    AccordionItem* Open(bool v);
    AccordionItem* Header(El* header);
    AccordionItem* Panel(El* panel);
    El* IntoEl();
};
} // namespace gpui

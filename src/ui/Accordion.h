/* Unstyled accordion — crates/base/src/accordion.rs */

#include "gpui/Gpui.h"

namespace gpui {

struct Accordion {
    static El* New(Arena* a, Str id);
};

struct AccordionTrigger {
    static El* New(Arena* a, Str id, int clickId = 0);
};

struct AccordionHeader {
    static El* New(Arena* a, El* trigger);
};

struct AccordionPanel {
    static El* New(Arena* a);
};

struct AccordionItem {
    El* root = nullptr;
    bool open = false;

    static AccordionItem* New(Arena* a);
    AccordionItem* Open(bool v);
    AccordionItem* Header(El* header);
    AccordionItem* Panel(El* panel);
    El* IntoEl();
};
} // namespace gpui

#include "ui/Accordion.h"
#include "ui/Primitive.h"

namespace gpui {

El* Accordion::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}

El* AccordionTrigger::New(Arena* a, Str id, int clickId) {
    return UiRoot(a, id, clickId);
}

El* AccordionHeader::New(Arena* a, El* trigger) {
    return Div(a)->Child(trigger);
}

El* AccordionPanel::New(Arena* a) {
    return Div(a);
}

AccordionItem* AccordionItem::New(Arena* a) {
    AccordionItem* item = ArenaNew<AccordionItem>(a);
    item->root = Div(a)->FlexCol();
    return item;
}

AccordionItem* AccordionItem::Open(bool v) {
    open = v;
    return this;
}

AccordionItem* AccordionItem::Header(El* header) {
    root->Child(header);
    return this;
}

AccordionItem* AccordionItem::Panel(El* panel) {
    if (open && panel) {
        root->Child(panel);
    }
    return this;
}

El* AccordionItem::IntoEl() {
    return root;
}
} // namespace gpui

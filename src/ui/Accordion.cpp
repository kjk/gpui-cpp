#include "ui/Accordion.h"
#include "ui/Primitive.h"

namespace gpui {

El* Accordion::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return UiRoot(a, id, 0);
}

El* AccordionTrigger::New(Ctx* cx, Str id, int clickId) {
    Arena* a = cx->a;
    return UiRoot(a, id, clickId);
}

El* AccordionHeader::New(Ctx* cx, El* trigger) {
    Arena* a = cx->a;
    return Div(a)->Child(trigger);
}

El* AccordionPanel::New(Ctx* cx) {
    Arena* a = cx->a;
    return Div(a);
}

AccordionItem* AccordionItem::New(Ctx* cx) {
    Arena* a = cx->a;
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

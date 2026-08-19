#include "base/accordion.h"

namespace gpui {

El* Accordion::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->Id(id);
}

El* AccordionTrigger::New(Ctx* cx, Str id, bool open, bool disabled,
                          Listener onChange) {
    Arena* a = cx->a;
    El* e = Div(a)->Id(id)->Click(HashClickId(id));
    if (!disabled && onChange.IsValid()) {
        e->OnClick(ListenerFill(onChange, !open));
    }
    return e;
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

AccordionItem* AccordionItem::KeepMounted(bool v) {
    keepMounted = v;
    return this;
}

AccordionItem* AccordionItem::Header(El* header) {
    root->Child(header);
    return this;
}

AccordionItem* AccordionItem::Panel(El* panel) {
    if (panel && (open || keepMounted)) {
        root->Child(panel);
    }
    return this;
}

El* AccordionItem::IntoEl() {
    return root;
}
} // namespace gpui

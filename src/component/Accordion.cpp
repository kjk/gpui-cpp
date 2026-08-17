#include "component/Accordion.h"

namespace component {

struct AccBind {
    Func1<int> fn;
    int index = 0;
};

static void FireAcc(AccBind* b) {
    b->fn.Call(b->index);
}

Accordion* Accordion::New(Arena* a, Str id) {
    Accordion* acc = ::New<Accordion>(a);
    acc->a = a;
    acc->id = id;
    return acc;
}

Accordion* Accordion::Multiple(bool v) {
    multiple = v;
    return this;
}
Accordion* Accordion::Bordered(bool v) {
    bordered = v;
    return this;
}
Accordion* Accordion::Disabled(bool v) {
    disabled = v;
    return this;
}
Accordion* Accordion::Item(Str title, Str body, bool open) {
    if (nItems < 8) {
        items[nItems].title = title;
        items[nItems].body = body;
        items[nItems].open = open;
        nItems++;
    }
    return this;
}
Accordion* Accordion::OnToggle(Func1<int> fn) {
    onToggle = fn;
    return this;
}

El* Accordion::IntoEl() {
    const Theme& th = ThemeNow();
    El* root = ::Accordion::New(a, id)->FlexCol()->W(kFill);
    if (bordered) {
        root->BorderT(1, th.border);
    }
    for (int i = 0; i < nItems; i++) {
        El* trig = AccordionTrigger::New(a, items[i].title, disabled ? 0 : HashClickId(items[i].title))
                       ->FlexRow()
                       ->H(36)
                       ->ItemsCenter()
                       ->JustifyBetween()
                       ->PadX(8)
                       ->BorderB(1, th.border)
                       ->Child(TextEl(a, items[i].title)->Font(14)->Fg(th.foreground))
                       ->Child(IconEl(a, items[i].open ? IconName::ChevronDown : IconName::ChevronRight, 14)->Fg(th.mutedFg));
        if (onToggle.IsValid() && !disabled) {
            AccBind* b = ::New<AccBind>(a);
            b->fn = onToggle;
            b->index = i;
            trig->OnClick(MkFunc0(&FireAcc, b));
        }
        ::AccordionItem* it = ::AccordionItem::New(a)->Open(items[i].open)->Header(::AccordionHeader::New(a, trig));
        it->Panel(::AccordionPanel::New(a)->Pad(8)->Child(TextEl(a, items[i].body)->Font(13)->Fg(th.mutedFg)->Wrap()));
        root->Child(it->IntoEl());
    }
    return root;
}

} // namespace component

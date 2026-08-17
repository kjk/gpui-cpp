#include "component/Accordion.h"
#include "component/Tag.h"

namespace gpui {

namespace component {

struct AccBind {
    Func1<int> fn;
    int index = 0;
};

static void FireAcc(AccBind* b) {
    b->fn.Call(b->index);
}

Accordion* Accordion::New(Arena* a, Str id) {
    Accordion* acc = ArenaNew<Accordion>(a);
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
Accordion* Accordion::WithSize(UiSize s) {
    size = s;
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
Accordion* Accordion::SettingsItem(Str title, Str body, bool open,
                                   IconName icon, Str tag) {
    if (nItems < 8) {
        items[nItems].title = title;
        items[nItems].body = body;
        items[nItems].open = open;
        items[nItems].icon = icon;
        items[nItems].tag = tag;
        items[nItems].settings = true;
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
    El* root = gpui::Accordion::New(a, id)->FlexCol()->W(kFill);
    if (bordered) {
        root->BorderT(1, th.border);
    }
    for (int i = 0; i < nItems; i++) {
        float h = UiSizePx(size) + 8.f;
        float font = UiFontPx(size);
        El* trig = AccordionTrigger::New(
            a, items[i].title, disabled ? 0 : HashClickId(items[i].title));
        trig->FlexRow()->ItemsCenter()->JustifyBetween()->PadX(8)->W(kFill);
        if (i + 1 < nItems) {
            trig->BorderB(1, th.border);
        }
        if (items[i].settings) {
            trig->PadY(8);
            El* left = Div(a)->FlexRow()->Gap(8)->ItemsCenter()->Grow();
            if (items[i].icon != IconName::None) {
                left->Child(
                    Div(a)
                        ->W(32)
                        ->H(32)
                        ->Radius(8)
                        ->Bg(RgbaOpacity(th.secondary, 0.5f))
                        ->ItemsCenter()
                        ->JustifyCenter()
                        ->Shrink0()
                        ->Child(IconEl(a, items[i].icon, 16)->Fg(th.mutedFg)));
            }
            left->Child(TextEl(a, items[i].title)
                            ->Font(font)
                            ->Fg(th.foreground)
                            ->Bold());
            if (items[i].tag.s) {
                left->Child(Tag::New(a, items[i].tag)
                                ->Success()
                                ->Outline()
                                ->WithSize(UiSize::Small)
                                ->IntoEl());
            }
            trig->Child(left);
        } else {
            trig->H(h)->W(kFill)->Child(
                TextEl(a, items[i].title)->Font(font)->Fg(th.foreground));
        }
        trig->Child(
            IconEl(a,
                   items[i].open ? IconName::ChevronUp : IconName::ChevronDown,
                   14)
                ->Fg(th.mutedFg));
        if (onToggle.IsValid() && !disabled) {
            AccBind* b = ArenaNew<AccBind>(a);
            b->fn = onToggle;
            b->index = i;
            trig->OnClick(MkFunc0(&FireAcc, b));
        }
        gpui::AccordionItem* it =
            gpui::AccordionItem::New(a)
                ->Open(items[i].open)
                ->Header(gpui::AccordionHeader::New(a, trig));
        El* panel = gpui::AccordionPanel::New(a);
        if (items[i].settings) {
            panel->PadL(52)->PadR(8)->PadT(0)->PadB(12);
        } else {
            panel->Pad(8);
        }
        it->Panel(panel->Child(
            TextEl(a, items[i].body)->Font(13)->Fg(th.mutedFg)->Wrap()));
        root->Child(it->IntoEl());
    }
    return root;
}

} // namespace component
} // namespace gpui

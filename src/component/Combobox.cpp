#include "component/Combobox.h"

namespace gpui {

namespace component {

Combobox* Combobox::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    Combobox* c = ArenaNew<Combobox>(a);
    c->a = a;
    c->cx = cx;
    c->id = id;
    return c;
}
Combobox* Combobox::Option(Str s) {
    if (n < 12) {
        options[n++] = s;
    }
    return this;
}
Combobox* Combobox::Options(const char* const* items, int count) {
    for (int i = 0; i < count; i++) {
        Option(Str(items[i]));
    }
    return this;
}
Combobox* Combobox::Selected(Str s) {
    selected = s;
    return this;
}
Combobox* Combobox::Placeholder(Str s) {
    placeholder = s;
    return this;
}
Combobox* Combobox::SearchPlaceholder(Str s) {
    searchPlaceholder = s;
    return this;
}
Combobox* Combobox::Icon(IconName i) {
    icon = i;
    return this;
}
Combobox* Combobox::W(float v) {
    width = v;
    return this;
}
Combobox* Combobox::Open(bool v) {
    open = v;
    return this;
}
Combobox* Combobox::Query(LineInput* q) {
    query = q;
    return this;
}
Combobox* Combobox::OnChange(Listener fn) {
    onChange = fn;
    return this;
}
Combobox* Combobox::OnToggle(Listener fn) {
    onToggle = fn;
    return this;
}

El* Combobox::IntoEl() {
    const Theme& th = cx->theme();
    // The trigger is input-shaped, like Select's.
    bool hasValue = selected.s != nullptr;
    El* trigger = Div(a)
                      ->FlexRow()
                      ->W(width)
                      ->H(32)
                      ->PadX(10)
                      ->Gap(8)
                      ->ItemsCenter()
                      ->JustifyBetween()
                      ->Radius(th.radius)
                      ->Bg(th.inputBg)
                      ->Border(1, open ? th.ring : th.inputBorder);
    El* title = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    // The icon comes from the selected item, so nothing shows until there
    // is one.
    if (icon != IconName::None && hasValue) {
        title->Child(IconEl(a, icon, 14)->Fg(th.mutedFg));
    }
    title->Child(TextEl(a, hasValue ? selected : placeholder)
                     ->Font(14)
                     ->Fg(hasValue ? th.foreground : th.mutedFg));
    trigger->Child(title);
    trigger->Child(IconEl(a, IconName::ChevronDown, 16)->Fg(th.mutedFg));
    BindClick(trigger, id, onToggle);

    El* pop = nullptr;
    if (open) {
        pop = Div(a)
                  ->FlexCol()
                  ->W(width)
                  ->Pad(4)
                  ->Gap(2)
                  ->Radius(th.radiusLg)
                  ->Border(1, th.border)
                  ->Bg(th.background);
        if (query) {
            El* search = Div(a)
                             ->FlexRow()
                             ->W(kFill)
                             ->H(32)
                             ->PadX(8)
                             ->Gap(8)
                             ->ItemsCenter()
                             ->BorderB(1, th.border);
            search->Child(IconEl(a, IconName::Search, 14)->Fg(th.mutedFg));
            search->Child(Div(a)->Grow()->Child(gpui::Input::New(cx, query)));
            pop->Child(search);
        }
        for (int i = 0; i < n; i++) {
            El* row = Div(a)
                          ->FlexRow()
                          ->W(kFill)
                          ->H(28)
                          ->PadX(8)
                          ->ItemsCenter()
                          ->JustifyBetween()
                          ->Radius(th.radius)
                          ->HoverBg(th.accent);
            row->Child(TextEl(a, options[i])->Font(14)->Fg(th.foreground));
            if (selected.s && StrEqI(options[i], selected)) {
                row->Child(IconEl(a, IconName::Check, 14)->Fg(th.foreground));
            }
            if (onChange.IsValid()) {
                BindClick(row, StrDup(a, fmt("%s-opt%d", id, i)),
                          ListenerArg(onChange, i));
            }
            pop->Child(row);
        }
    }
    El* root = gpui::Combobox::New(cx, id)->W(width)->Child(trigger);
    return Popup::New(cx, StrDup(a, fmt("%s-pop", id)), root)
        ->Content(pop)
        ->IntoEl();
}

} // namespace component
} // namespace gpui

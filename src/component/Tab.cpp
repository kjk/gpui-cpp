#include "component/Tab.h"

namespace gpui {

namespace component {

struct TabBind {
    Func1<int> fn;
    int index = 0;
};
static void FireTab(TabBind* b) {
    b->fn.Call(b->index);
}

Tabs* Tabs::New(Arena* a) {
    Tabs* t = ArenaNew<Tabs>(a);
    t->a = a;
    return t;
}
Tabs* Tabs::Tab(Str label) {
    if (n < 8) {
        labels[n++] = label;
    }
    return this;
}
Tabs* Tabs::Selected(int i) {
    selected = i;
    return this;
}
Tabs* Tabs::OnChange(Func1<int> fn) {
    onChange = fn;
    return this;
}

El* Tabs::IntoEl() {
    const Theme& th = ThemeNow();
    El* bar = gpui::Tabs::New(a, StrL("tabs"))
                  ->FlexRow()
                  ->Gap(4)
                  ->BorderB(1, th.border);
    for (int i = 0; i < n; i++) {
        bool on = i == selected;
        El* tab =
            gpui::Tab::New(a, labels[i], HashClickId(labels[i]))
                ->H(28)
                ->PadX(8)
                ->ItemsCenter()
                ->BorderB(2, on ? th.foreground : th.background)
                ->Child(TextEl(a, labels[i])->Font(13)->Fg(th.foreground));
        if (on) {
            tab->first->style.fontSemibold = true;
        }
        if (onChange.IsValid()) {
            TabBind* b = ArenaNew<TabBind>(a);
            b->fn = onChange;
            b->index = i;
            tab->OnClick(MkFunc0(&FireTab, b));
        }
        bar->Child(tab);
    }
    return bar;
}

} // namespace component
} // namespace gpui

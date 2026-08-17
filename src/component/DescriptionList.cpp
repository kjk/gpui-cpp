#include "component/DescriptionList.h"

namespace component {

DescriptionList* DescriptionList::New(Arena* a) {
    DescriptionList* d = ::New<DescriptionList>(a);
    d->a = a;
    return d;
}
DescriptionList* DescriptionList::Item(Str key, Str val) {
    if (n < 12) {
        keys[n] = key;
        vals[n] = val;
        n++;
    }
    return this;
}

El* DescriptionList::IntoEl() {
    const Theme& th = ThemeNow();
    El* col = Div(a)->FlexCol()->Gap(8);
    for (int i = 0; i < n; i++) {
        col->Child(Div(a)
                       ->FlexRow()
                       ->Gap(12)
                       ->Child(Div(a)->W(140)->Child(TextEl(a, keys[i])->Font(13)->Fg(th.mutedFg)))
                       ->Child(TextEl(a, vals[i])->Font(13)->Fg(th.foreground)));
    }
    return col;
}

} // namespace component

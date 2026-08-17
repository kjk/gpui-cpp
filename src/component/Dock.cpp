#include "component/Dock.h"

namespace gpui {

namespace component {

Dock* Dock::New(Arena* a) {
    Dock* d = ArenaNew<Dock>(a);
    d->a = a;
    return d;
}
Dock* Dock::Left(El* e) {
    left = e;
    return this;
}
Dock* Dock::Center(El* e) {
    center = e;
    return this;
}
Dock* Dock::Right(El* e) {
    right = e;
    return this;
}

El* Dock::IntoEl() {
    El* row = Div(a)->FlexRow()->SizeFull();
    if (left) {
        row->Child(left);
    }
    if (center) {
        row->Child(Div(a)->Grow()->H(kFill)->Child(center));
    }
    if (right) {
        row->Child(right);
    }
    return row;
}

} // namespace component
} // namespace gpui

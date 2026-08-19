#include "base/link.h"

namespace gpui {

El* Link::New(Ctx* cx, Str id, bool disabled, Listener onActivate) {
    Arena* a = cx->a;
    int clickId = HashClickId(id);
    El* e = Div(a)->Id(id)->Click(clickId);
    if (disabled) {
        return e;
    }
    e->FocusId(clickId);
    if (onActivate.IsValid()) {
        e->OnClick(onActivate);
    }
    return e;
}
} // namespace gpui

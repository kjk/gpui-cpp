#include "base/link.h"

namespace gpui {

El* Link::New(Ctx* cx, Str id, bool disabled, Listener onActivate,
              const LinkStyles* styles) {
    Arena* a = cx->a;
    int clickId = HashClickId(id);
    El* e = Div(a)->Id(id)->Click(clickId);
    if (styles && disabled) {
        const StateStyle* active[1] = {&styles->disabled};
        ElRefine(e, StateStyleResolve(StateStyle{}, active, 1));
    }
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

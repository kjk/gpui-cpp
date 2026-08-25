#include "base/link.h"

namespace gpui {

El* Link::New(Ctx* cx, Str id, bool disabled, Listener onActivate,
              const LinkStyles* styles) {
    Arena* a = cx->a;
    El* e = Div(a)->PathClick(id);
    if (styles && disabled) {
        const StateStyle* active[1] = {&styles->disabled};
        ElRefine(e, StateStyleResolve(StateStyle{}, active, 1));
    }
    if (disabled) {
        return e;
    }
    e->PathId(id);
    if (onActivate.IsValid()) {
        e->OnClick(onActivate);
    }
    return e;
}
} // namespace gpui

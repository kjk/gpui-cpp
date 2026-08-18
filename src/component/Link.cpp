#include "component/Link.h"

namespace gpui {

namespace component {

Link* Link::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    Link* l = ArenaNew<Link>(a);
    l->a = a;
    l->cx = cx;
    l->id = id;
    return l;
}

Link* Link::Href(Str s) {
    href = s;
    return this;
}
Link* Link::Text(Str s) {
    text = s;
    return this;
}
Link* Link::Disabled(bool v) {
    disabled = v;
    return this;
}
Link* Link::OnOpen(Listener fn) {
    onOpen = fn;
    return this;
}

El* Link::IntoEl() {
    const Theme& th = cx->theme();
    El* e = gpui::Link::New(cx, id, disabled ? 0 : HashClickId(id));
    if (onOpen.IsValid() && !disabled) {
        e->OnClick(onOpen);
    }
    // text_decoration_1(): a link is underlined at rest, not only on hover.
    e->Child(TextEl(a, text.s ? text : href)
                 ->Font(14)
                 ->Underline()
                 ->Fg(disabled ? th.mutedFg : th.blue));
    return e;
}

} // namespace component
} // namespace gpui

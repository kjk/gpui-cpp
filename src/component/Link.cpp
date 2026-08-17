#include "component/Link.h"

namespace gpui {

namespace component {

struct LinkBind {
    Func1<Str> fn;
    Str href;
};

static void FireLink(LinkBind* b) {
    b->fn.Call(b->href);
}

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
Link* Link::OnOpen(Func1<Str> fn) {
    onOpen = fn;
    return this;
}

El* Link::IntoEl() {
    const Theme& th = ThemeNow();
    El* e = gpui::Link::New(cx, id, disabled ? 0 : HashClickId(id));
    if (onOpen.IsValid() && !disabled) {
        LinkBind* b = ArenaNew<LinkBind>(a);
        b->fn = onOpen;
        b->href = href;
        e->OnClick(MkFunc0(&FireLink, b));
    }
    e->Child(TextEl(a, text.s ? text : href)
                 ->Font(14)
                 ->Fg(disabled ? th.mutedFg : th.blue));
    return e;
}

} // namespace component
} // namespace gpui

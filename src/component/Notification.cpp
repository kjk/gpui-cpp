#include "component/Notification.h"
#include "component/Alert.h"

namespace gpui {

namespace component {

Notification* Notification::New(Ctx* cx, Str title, Str message) {
    Arena* a = cx->a;
    Notification* n = ArenaNew<Notification>(a);
    n->a = a;
    n->cx = cx;
    n->title = title;
    n->message = message;
    return n;
}
Notification* Notification::Kind(NotificationKind k) {
    kind = k;
    return this;
}
Notification* Notification::OnClose(Listener fn) {
    onClose = fn;
    return this;
}

El* Notification::IntoEl() {
    AlertVariant v = AlertVariant::Info;
    if (kind == NotificationKind::Success) {
        v = AlertVariant::Success;
    } else if (kind == NotificationKind::Warning) {
        v = AlertVariant::Warning;
    } else if (kind == NotificationKind::Error) {
        v = AlertVariant::Error;
    }
    Alert* al = Alert::New(cx, StrL("notification"), message)
                    ->Title(title)
                    ->OnClose(onClose);
    al->variant = v;
    return al->IntoEl();
}

} // namespace component
} // namespace gpui

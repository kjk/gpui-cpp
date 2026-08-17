#include "component/Notification.h"
#include "component/Alert.h"

namespace component {

Notification* Notification::New(Arena* a, Str title, Str message) {
    Notification* n = ::New<Notification>(a);
    n->a = a;
    n->title = title;
    n->message = message;
    return n;
}
Notification* Notification::Kind(NotificationKind k) {
    kind = k;
    return this;
}
Notification* Notification::OnClose(Func0 fn) {
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
    Alert* al = Alert::New(a, StrL("notification"), message)
                    ->Title(title)
                    ->OnClose(onClose);
    al->variant = v;
    return al->IntoEl();
}

} // namespace component

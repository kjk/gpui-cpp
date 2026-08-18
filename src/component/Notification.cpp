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
Notification* Notification::Action(El* e) {
    action = e;
    return this;
}
Notification* Notification::Content(El* e) {
    content = e;
    return this;
}
Notification* Notification::Placement(NotificationAnchor p) {
    anchor = p;
    return this;
}
Notification* Notification::OnClose(Listener fn) {
    onClose = fn;
    return this;
}
Notification* Notification::OnClick(Listener fn) {
    onClick = fn;
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
    // Alert's content slot stands in for the message, so an action has to
    // carry the message along with it: content() replaces the text, action()
    // only adds a button under it.
    if (content || action) {
        El* extra = Div(a)->FlexCol()->W(kFill)->Gap(8);
        if (content) {
            extra->Child(content);
        } else if (message.s && message.len > 0) {
            extra->Child(TextEl(a, message)
                             ->Font(14)
                             ->Fg(cx->theme().foreground)
                             ->Wrap()
                             ->W(kFill));
        }
        if (action) {
            extra->Child(Div(a)->FlexRow()->Child(action));
        }
        al->Content(extra);
    }
    El* card = al->IntoEl();
    // The body answers on_click; the x inside the card has its own listener,
    // and being painted later it wins the hit test.
    if (onClick.IsValid()) {
        BindClick(card, StrL("notification-body"), onClick);
    }
    if (anchor == NotificationAnchor::None) {
        return card;
    }
    // Rust hands the notification list to Root, which floats it in a corner
    // of the window. A fixed layer with the matching alignment does the same.
    card->W(width);
    El* layer = Div(a)
                    ->Fixed()
                    ->Top(0)
                    ->Left(0)
                    ->W(kFill)
                    ->H(kFill)
                    ->FlexCol()
                    ->Pad(16)
                    ->Child(card);
    switch (anchor) {
        case NotificationAnchor::TopLeft:
            layer->JustifyStart()->ItemsStart();
            break;
        case NotificationAnchor::TopCenter:
            layer->JustifyStart()->ItemsCenter();
            break;
        case NotificationAnchor::TopRight:
            layer->JustifyStart()->ItemsEnd();
            break;
        case NotificationAnchor::LeftCenter:
            layer->JustifyCenter()->ItemsStart();
            break;
        case NotificationAnchor::RightCenter:
            layer->JustifyCenter()->ItemsEnd();
            break;
        case NotificationAnchor::BottomLeft:
            layer->JustifyEnd()->ItemsStart();
            break;
        case NotificationAnchor::BottomCenter:
            layer->JustifyEnd()->ItemsCenter();
            break;
        default:
            layer->JustifyEnd()->ItemsEnd();
            break;
    }
    return layer;
}

} // namespace component
} // namespace gpui

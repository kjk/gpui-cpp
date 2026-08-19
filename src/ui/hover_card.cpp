#include "ui/hover_card.h"

namespace gpui {

namespace component {

HoverCard* HoverCard::New(Ctx* cx) {
    Arena* a = cx->a;
    HoverCard* h = ArenaNew<HoverCard>(a);
    h->a = a;
    h->cx = cx;
    return h;
}
HoverCard* HoverCard::Trigger(El* e) {
    trigger = e;
    return this;
}
HoverCard* HoverCard::Content(El* e) {
    content = e;
    return this;
}
HoverCard* HoverCard::Open(bool v) {
    open = v;
    return this;
}
HoverCard* HoverCard::New(Ctx* cx, Str id) {
    HoverCard* h = New(cx);
    h->id = id;
    return h;
}
HoverCard* HoverCard::Anchor(HoverCardAnchor v) {
    anchor = v;
    return this;
}

El* HoverCard::IntoEl() {
    El* card = open ? content : nullptr;
    if (card) {
        // The base card hangs bottom-left; the other five corners place
        // themselves. Deferred, so it draws over what follows it.
        const float kGap = 4.f;
        switch (anchor) {
            case HoverCardAnchor::BottomCenter:
                card->AnchorBelow(kGap)->AnchorCenterX();
                break;
            case HoverCardAnchor::BottomRight:
                card->AnchorBelow(kGap)->Right(0);
                break;
            case HoverCardAnchor::TopLeft:
                card->AnchorAbove(kGap)->Left(0);
                break;
            case HoverCardAnchor::TopCenter:
                card->AnchorAbove(kGap)->AnchorCenterX();
                break;
            case HoverCardAnchor::TopRight:
                card->AnchorAbove(kGap)->Right(0);
                break;
            default:
                card->AnchorBelow(kGap)->Left(0);
                break;
        }
        card->Deferred();
    }
    return gpui::HoverCard::New(cx, id.s ? id : StrL("hover-card"))
        ->Trigger(trigger)
        ->Content(card)
        ->IntoEl();
}

} // namespace component
} // namespace gpui

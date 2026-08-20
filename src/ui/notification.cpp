#include "ui/notification.h"
#include "base/motion.h"
#include "ui/alert.h"
#include "ui/title_bar.h"

namespace gpui {

namespace component {

// toast.rs: every offset the stack works out is transitioned rather than
// taken, over ToastMotion::duration and along CSS's `ease`.
static float ToastEase(float t) {
    return CubicBezier(0.25f, 0.1f, 0.25f, 1.f, t);
}

static Motion ToastMotionPolicy() {
    Motion m = MotionNew((float)kToastTransitionMs);
    m.ease = ToastEase;
    return m;
}

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

int NotificationIndexOf(const NotificationListState* s, int id) {
    for (int i = 0; i < s->n; i++) {
        if (s->items[i].id == id) {
            return i;
        }
    }
    return -1;
}

static void NotificationRemoveAt(NotificationListState* s, int ix) {
    ToastRemove(&s->stack, s->items[ix].id);
    for (int i = ix; i < s->n - 1; i++) {
        s->items[i] = s->items[i + 1];
    }
    s->n--;
}

int NotificationPush(NotificationListState* s, NotificationItem item,
                     int timeoutMs) {
    // A push with an id already in the list replaces that one: Rust keys its
    // notifications by NotificationId, so the same one never stacks twice.
    if (item.id != 0) {
        int at = NotificationIndexOf(s, item.id);
        if (at >= 0) {
            NotificationRemoveAt(s, at);
        }
    } else {
        item.id = s->nextId++;
    }
    // max_items: the oldest goes to make room.
    while (s->n >= s->maxItems || s->n >= kToastStackCap) {
        NotificationRemoveAt(s, 0);
    }
    s->items[s->n++] = item;
    ToastPush(&s->stack, item.id, timeoutMs);
    return item.id;
}

void NotificationDismiss(NotificationListState* s, int id) {
    for (int i = 0; i < s->stack.n; i++) {
        if (s->stack.entries[i].id == id) {
            // The card animates out first; advance drops it when it is done.
            s->stack.entries[i].status = ToastStatus::Ending;
            s->stack.entries[i].elapsedMs = 0;
            return;
        }
    }
}

void NotificationClear(NotificationListState* s) {
    for (int i = 0; i < s->n; i++) {
        NotificationDismiss(s, s->items[i].id);
    }
}

bool NotificationAdvance(NotificationListState* s, int deltaMs) {
    bool changed = ToastAdvance(&s->stack, deltaMs, s->stack.IsExpanded());
    if (!changed) {
        return false;
    }
    // Whatever the stack dropped goes from the list with it.
    for (int i = s->n - 1; i >= 0; i--) {
        bool alive = false;
        for (int j = 0; j < s->stack.n; j++) {
            if (s->stack.entries[j].id == s->items[i].id) {
                alive = true;
                break;
            }
        }
        if (!alive) {
            for (int k = i; k < s->n - 1; k++) {
                s->items[k] = s->items[k + 1];
            }
            s->n--;
        }
    }
    return true;
}

void NotificationListState::OnCloseClick(NotificationListState* self, Ctx* cx,
                                         const ClickEvent*, intptr_t id) {
    NotificationDismiss(self, (int)id);
    Notify(cx);
}

void NotificationListState::OnItemClick(NotificationListState* self, Ctx* cx,
                                        const ClickEvent* ev, intptr_t id) {
    int at = NotificationIndexOf(self, (int)id);
    if (at >= 0 && self->items[at].onClick.IsValid()) {
        ListenerCall(cx->app, cx->win, self->items[at].onClick, ev);
    }
    NotificationDismiss(self, (int)id);
    Notify(cx);
}

void NotificationListState::OnHover(NotificationListState* self, Ctx* cx,
                                    const HoverEvent* ev) {
    // is_expanded: the pointer over the stack opens it out, and holds every
    // timeout while it is there.
    if (self->stack.hovered == ev->hovered) {
        return;
    }
    self->stack.hovered = ev->hovered;
    Notify(cx);
}

void NotificationListState::OnTick(NotificationListState* self, Ctx* cx,
                                   const TickEvent*) {
    if (NotificationAdvance(self, kNotificationTickMs)) {
        Notify(cx);
    }
}

NotificationList* NotificationList::New(Ctx* cx,
                                        Entity<NotificationListState> state) {
    Arena* a = cx->a;
    NotificationList* l = ArenaNew<NotificationList>(a);
    l->a = a;
    l->cx = cx;
    l->state = state;
    return l;
}

El* NotificationList::IntoEl() {
    NotificationListState* s = state.Get(cx);
    if (!s || s->n == 0) {
        return Div(a);
    }
    WinSize win = WindowSize(cx->win);
    bool bottom = s->placement == NotificationAnchor::BottomLeft ||
                  s->placement == NotificationAnchor::BottomCenter ||
                  s->placement == NotificationAnchor::BottomRight;
    bool expanded = s->stack.IsExpanded();

    float heights[kToastStackCap];
    float collapsedOff[kToastStackCap];
    float expandedOff[kToastStackCap];
    for (int i = 0; i < s->n; i++) {
        heights[i] = s->itemH;
    }
    float expandedH = 0;
    float collapsedH = ToastStackGeometry(
        heights, s->n, kToastCollapsedPeek, kToastExpandedGap, bottom,
        collapsedOff, expandedOff, &expandedH);
    Motion policy = ToastMotionPolicy();
    // The stack's own height, which is what opens the space the cards move
    // into rather than snapping the whole layer taller.
    float stackH =
        MotionValue(cx, MotionId(StrL("notification-stack"), StrL("height")),
                    expanded ? expandedH : collapsedH, policy);

    // The stack floats over the window in the corner its placement names.
    El* layer = Div(a)->Absolute()->Fixed()->W(s->width)->H(stackH)->OnHover(
        ListenTo(state, &NotificationListState::OnHover));
    float margin = kNotificationMargin;
    bool right = s->placement == NotificationAnchor::TopRight ||
                 s->placement == NotificationAnchor::RightCenter ||
                 s->placement == NotificationAnchor::BottomRight;
    bool center = s->placement == NotificationAnchor::TopCenter ||
                  s->placement == NotificationAnchor::BottomCenter;
    float left = right ? win.dipW - s->width - margin
                       : (center ? (win.dipW - s->width) * 0.5f : margin);
    // The top margin clears the title bar, which is what Rust's default
    // margins do.
    float top = bottom ? win.dipH - stackH - margin : kTitleBarHeight + margin;
    if (s->placement == NotificationAnchor::LeftCenter ||
        s->placement == NotificationAnchor::RightCenter) {
        top = (win.dipH - stackH) * 0.5f;
    }
    layer->Left(left)->Top(top);
    // The stack needs a hit box of its own for the hover to reach it.
    layer->Id(StrL("notification-stack"))
        ->Click(HashClickId(StrL("notification-stack")));

    for (int i = 0; i < s->n; i++) {
        const NotificationItem& it = s->items[i];
        int rank = s->n - 1 - i;
        // collapsed_visible: only the front few show at all when the stack is
        // closed.
        if (!expanded && rank >= kToastCollapsedVisible) {
            continue;
        }
        // "offset" and "inset": where this card sits, and how much narrower
        // it is than the front one. Both are the card's own transitions, keyed
        // on its id, so a stack that opens moves each of them from wherever it
        // had got to.
        Str key = StrDup(a, fmt("%d", it.id));
        float off =
            MotionValue(cx, MotionId(StrL("toast-offset"), key),
                        expanded ? expandedOff[i] : collapsedOff[i], policy);
        float shrink = MotionValue(
            cx, MotionId(StrL("toast-inset"), key),
            expanded ? 0.f
                     : s->width * kToastCollapsedScaleStep *
                           (float)(rank < kToastCollapsedVisible
                                       ? rank
                                       : kToastCollapsedVisible - 1),
            policy);
        El* card =
            Notification::New(cx, it.title, it.message)
                ->Kind(it.kind)
                ->Content(it.content)
                ->OnClose(ListenTo(state, &NotificationListState::OnCloseClick,
                                   (intptr_t)it.id))
                ->OnClick(ListenTo(state, &NotificationListState::OnItemClick,
                                   (intptr_t)it.id))
                ->IntoEl();
        layer->Child(Div(a)
                         ->Absolute()
                         ->Top(off)
                         ->Left(shrink * 0.5f)
                         ->W(s->width - shrink)
                         ->Child(card));
    }
    return layer;
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

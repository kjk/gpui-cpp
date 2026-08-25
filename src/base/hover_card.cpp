#include "base/hover_card.h"

namespace gpui {

// cancel_tasks + next_epoch in one: there is at most one countdown in flight,
// so dropping it is the whole of what the epoch was protecting against.
static void HoverCardCancel(HoverCardState* self, Ctx* cx) {
    if (self->timer) {
        WindowCancelTimer(cx->win, self->timer);
        self->timer = 0;
    }
}

static void HoverCardSetOpen(HoverCardState* self, Ctx* cx, bool open) {
    if (self->open == open) {
        return;
    }
    self->open = open;
    Notify(cx);
}

void HoverCardState::OnOpen(HoverCardState* self, Ctx* cx, const TickEvent*) {
    self->timer = 0;
    HoverCardSetOpen(self, cx, true);
}

void HoverCardState::OnClose(HoverCardState* self, Ctx* cx, const TickEvent*) {
    self->timer = 0;
    // The countdown started because nothing was hovered; by the time it lands
    // the pointer may have come back, so ask again.
    if (self->hoveringTrigger || self->hoveringContent) {
        return;
    }
    HoverCardSetOpen(self, cx, false);
}

static void HoverCardScheduleOpen(HoverCardState* self, Ctx* cx) {
    HoverCardCancel(self, cx);
    self->timer = WindowSetTimeout(cx->win, self->openDelayMs,
                                   Listen(cx, &HoverCardState::OnOpen));
}

static void HoverCardScheduleClose(HoverCardState* self, Ctx* cx) {
    HoverCardCancel(self, cx);
    self->timer = WindowSetTimeout(cx->win, self->closeDelayMs,
                                   Listen(cx, &HoverCardState::OnClose));
}

void HoverCardTriggerHover(HoverCardState* self, Ctx* cx,
                           const HoverEvent* ev) {
    self->hoveringTrigger = ev->hovered;
    if (ev->hovered) {
        HoverCardScheduleOpen(self, cx);
    } else if (!self->hoveringContent) {
        HoverCardScheduleClose(self, cx);
    }
}

void HoverCardContentHover(HoverCardState* self, Ctx* cx,
                           const HoverEvent* ev) {
    self->hoveringContent = ev->hovered;
    if (ev->hovered) {
        // Reaching the content is what keeps the card up: whatever countdown
        // was running is dropped and none replaces it.
        HoverCardCancel(self, cx);
    } else if (!self->hoveringTrigger) {
        HoverCardScheduleClose(self, cx);
    }
}

void HoverCardSetDelays(Ctx* cx, Entity<HoverCardState> state, int openMs,
                        int closeMs) {
    HoverCardState* s = state.Get(cx);
    if (!s) {
        return;
    }
    s->openDelayMs = openMs;
    s->closeDelayMs = closeMs;
}

bool HoverCardIsOpen(Ctx* cx, Entity<HoverCardState> state) {
    HoverCardState* s = state.Get(cx);
    return s && s->open;
}

// use_keyed_state(id): one state per card id, made on the frame that first
// asks for it and kept by the app afterwards. Without it the delays would be
// thrown away with the tree that armed them.
Entity<HoverCardState> HoverCardStateFor(Ctx* cx, Str id) {
    return KeyedEntity<HoverCardState>(cx, (uint32_t)HashClickId(id));
}

bool HoverCardIsOpen(Ctx* cx, Str id) {
    return HoverCardIsOpen(cx, HoverCardStateFor(cx, id));
}

HoverCard* HoverCard::New(Ctx* cx, Str id, Entity<HoverCardState> state) {
    Arena* a = cx->a;
    HoverCard* h = ArenaNew<HoverCard>(a);
    h->a = a;
    h->cx = cx;
    h->id = id;
    h->state = state.IsValid() ? state : HoverCardStateFor(cx, id);
    h->root = Div(a)->Id(id);
    return h;
}

bool HoverCard::IsOpen() const {
    return HoverCardIsOpen(cx, state);
}

// A part only hovers if the frame can name it, so one that came without an
// identity is given the card's own with a suffix.
static void HoverPart(Arena* a, El* e, Str id, const char* suffix) {
    if (e->clickId) {
        return;
    }
    Str partId = StrDup(a, fmt("%s-%s", id, Str(suffix)));
    e->Id(partId)->Click(HashClickId(partId));
}

HoverCard* HoverCard::Trigger(El* trigger) {
    if (!trigger) {
        return this;
    }
    if (state.IsValid()) {
        HoverPart(a, trigger, id, "trigger");
        trigger->OnHover(ListenTo(state, &HoverCardTriggerHover));
    }
    root->Child(trigger);
    return this;
}

HoverCard* HoverCard::Content(El* content) {
    if (!content) {
        return this;
    }
    // Under the trigger unless the caller already placed it.
    if (!content->style.absolute) {
        content->Absolute()->Top(22)->Left(0);
    }
    if (state.IsValid()) {
        HoverPart(a, content, id, "content");
        content->OnHover(ListenTo(state, &HoverCardContentHover));
    }
    root->Child(content);
    return this;
}

El* HoverCard::IntoEl() {
    return root;
}
} // namespace gpui

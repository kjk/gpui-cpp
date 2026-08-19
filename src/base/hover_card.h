/* Unstyled hover card — crates/base/src/hover_card.rs */

#include "gpui/gpui.h"

namespace gpui {

// Rust's HoverCardState, the whole reason a hover card is not just
// "is the trigger hovered". A card waits out `openDelayMs` before it appears
// and `closeDelayMs` before it goes, and a close only lands if neither the
// trigger nor the content is hovered by the time it does — which is what lets
// the pointer travel from one to the other without the card vanishing
// underneath it.
//
// It is an entity because it owns a timer, the way BlinkCursor is. Rust hangs
// it off `window.use_keyed_state(id)`; KeyedEntity is the same hook, so a page
// gets one per card without declaring a field.
struct HoverCardState {
    bool open = false;
    bool hoveringTrigger = false;
    bool hoveringContent = false;
    int openDelayMs = 600;
    int closeDelayMs = 300;
    // The armed timer. Cancelling it is what Rust's epoch counter does: a
    // countdown that has been superseded must not fire.
    int timer = 0;

    static void OnOpen(HoverCardState* self, Ctx* cx, const TickEvent* ev);
    static void OnClose(HoverCardState* self, Ctx* cx, const TickEvent* ev);
};

// The trigger and the content each report their own hovering, and the state
// decides what that means. Rust's on_trigger_hover / on_content_hover.
void HoverCardTriggerHover(HoverCardState* self, Ctx* cx, const HoverEvent* ev);
void HoverCardContentHover(HoverCardState* self, Ctx* cx, const HoverEvent* ev);

// `sync`: the delays are the caller's every frame, not just the first one.
void HoverCardSetDelays(Ctx* cx, Entity<HoverCardState> state, int openMs,
                        int closeMs);
bool HoverCardIsOpen(Ctx* cx, Entity<HoverCardState> state);

// The card itself. `state` is the entity the two hover handlers run against;
// pass {} for a card the caller opens on its own.
//
// Hover is reported against an element identity, so the trigger and the
// content are each given one if they arrived without. Rust wraps the trigger
// in a `div().id("trigger")` instead; going onto the element the caller handed
// over keeps its box exactly as it was, which the six anchor corners depend
// on.
struct HoverCard {
    Arena* a = nullptr;
    El* root = nullptr;
    Str id = {};
    Entity<HoverCardState> state = {};

    static HoverCard* New(Ctx* cx, Str id, Entity<HoverCardState> state = {});
    HoverCard* Trigger(El* trigger);
    HoverCard* Content(El* content);
    El* IntoEl();
};
} // namespace gpui

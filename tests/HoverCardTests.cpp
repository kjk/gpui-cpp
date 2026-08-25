/* Ported from crates/base/src/hover_card.rs.
 *
 * A hover card is not "is the trigger hovered": it waits out an open delay
 * before it appears and a close delay before it goes, and the close only
 * lands if nothing is hovered by the time it arrives — which is what lets the
 * pointer travel from the trigger to the card. Rust's own case there drives a
 * window and advances a clock; these are the rules underneath, plus the keyed
 * state that is what makes them survive a frame at all. */

#include "Test.h"

// window.use_keyed_state(self.id, ..): the card makes its own state, so a
// page declares no field for one. Two cards are two states, and the same card
// asked twice is one — which is the whole point, since the tree that armed a
// countdown is thrown away before it fires.
static void EveryCardIdIsItsOwnStateAndKeepsIt() {
    App app;
    Window* win = new Window();
    win->app = &app;
    Arena* a = ArenaNew();
    Ctx cx = {};
    cx.app = &app;
    cx.win = win;
    cx.a = a;

    Entity<HoverCardState> one = HoverCardStateFor(&cx, StrL("card-1"));
    Entity<HoverCardState> two = HoverCardStateFor(&cx, StrL("card-2"));
    utassert(one.IsValid() && two.IsValid());
    utassert(one.id != two.id);

    HoverCardState* s = one.Get(&cx);
    utassert(s && !s->open);
    s->open = true;
    // The next frame asks again and gets the same state back, still open.
    utassert(HoverCardIsOpen(&cx, StrL("card-1")));
    utassert(!HoverCardIsOpen(&cx, StrL("card-2")));

    WindowKeyedFree(win);
    ArenaDelete(a);
    delete win;
    EntityDropAll(&app);
}

// schedule_close's closure asks again when it lands: `if state.epoch == epoch
// && !is_hovering_trigger && !is_hovering_content`. The countdown started
// because nothing was hovered, but by the time it fires the pointer may have
// arrived on the card.
static void ACloseThatLandsOnAHoveredCardDoesNothing() {
    App app;
    Window* win = new Window();
    win->app = &app;
    Ctx cx = {};
    cx.app = &app;
    cx.win = win;

    HoverCardState s;
    s.open = true;
    s.hoveringContent = true;
    HoverCardState::OnClose(&s, &cx, nullptr);
    utassert(s.open);

    // And with the pointer gone from both, the same landing closes it.
    s.hoveringContent = false;
    HoverCardState::OnClose(&s, &cx, nullptr);
    utassert(!s.open);

    delete win;
    EntityDropAll(&app);
}

void TestHoverCard() {
    TestSuite("hover_card");
    EveryCardIdIsItsOwnStateAndKeepsIt();
    ACloseThatLandsOnAHoveredCardDoesNothing();
}

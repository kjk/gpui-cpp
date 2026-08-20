/* Ported from crates/gpui's EventEmitter — cx.emit, cx.subscribe and the
 * Subscription they hand back.
 *
 * Rust marks what an entity emits with a trait and drops a subscription when
 * its guard drops. There is no trait to mark here and nothing drops on scope
 * exit, so what is worth pinning is the rest: every live subscriber hears an
 * emit in the order they subscribed, a subscription can be given up by
 * asking, and one whose end has gone away is swept rather than called. */

#include "Test.h"

namespace {

struct Counter {
    int seen = 0;
    int last = 0;

    static El* Render(Counter*, Ctx* cx) { return Div(cx->a); }

    static void OnTick(Counter* self, Ctx*, const ListEvent* ev) {
        self->seen++;
        self->last = ev->index;
    }
};

// The emitter is a state rather than a view, the way a ListState is.
struct Ticker {
    int unused = 0;
};

} // namespace

static void EverySubscriberHearsIt() {
    App app;
    Window* win = new Window();
    win->app = &app;
    Entity<Ticker> ticker = EntityNewState<Ticker>(&app);
    Entity<Counter> a = EntityNew<Counter>(&app);
    Entity<Counter> b = EntityNew<Counter>(&app);

    SubscribeTo(&app, ticker, a, &Counter::OnTick);
    SubscribeTo(&app, ticker, b, &Counter::OnTick);
    utassert(EntitySubscriberCount(&app, ticker.id) == 2);

    ListEvent ev = {ListEventKind::Confirm, 7, false};
    EntityEmit(&app, win, ticker.id, &ev);
    utassert(a.Get(&app)->seen == 1 && a.Get(&app)->last == 7);
    utassert(b.Get(&app)->seen == 1 && b.Get(&app)->last == 7);

    // Nothing else is listening to this one, so nothing hears it.
    Entity<Ticker> other = EntityNewState<Ticker>(&app);
    EntityEmit(&app, win, other.id, &ev);
    utassert(a.Get(&app)->seen == 1);

    delete win;
    EntityDropAll(&app);
}

static void ASubscriptionCanBeGivenUp() {
    App app;
    Window* win = new Window();
    win->app = &app;
    Entity<Ticker> ticker = EntityNewState<Ticker>(&app);
    Entity<Counter> a = EntityNew<Counter>(&app);
    Subscription sub = SubscribeTo(&app, ticker, a, &Counter::OnTick);
    utassert(sub.IsValid());

    ListEvent ev = {ListEventKind::Select, 1, false};
    EntityEmit(&app, win, ticker.id, &ev);
    utassert(a.Get(&app)->seen == 1);

    EntityUnsubscribe(&app, sub);
    utassert(EntitySubscriberCount(&app, ticker.id) == 0);
    EntityEmit(&app, win, ticker.id, &ev);
    utassert(a.Get(&app)->seen == 1);

    delete win;
    EntityDropAll(&app);
}

// A subscription is dead once either end of it is: Rust drops the whole list
// with the emitter, and the Subscription with the subscriber.
static void AStaleEndSweepsTheSubscription() {
    App app;
    Window* win = new Window();
    win->app = &app;
    Entity<Ticker> ticker = EntityNewState<Ticker>(&app);
    Entity<Counter> gone = EntityNew<Counter>(&app);
    Entity<Counter> alive = EntityNew<Counter>(&app);
    SubscribeTo(&app, ticker, gone, &Counter::OnTick);
    SubscribeTo(&app, ticker, alive, &Counter::OnTick);

    EntityDrop(&app, gone.id);
    utassert(EntitySubscriberCount(&app, ticker.id) == 1);

    ListEvent ev = {ListEventKind::Select, 2, false};
    EntityEmit(&app, win, ticker.id, &ev);
    utassert(alive.Get(&app)->seen == 1);

    // And the emitter going away takes the rest with it.
    EntityDrop(&app, ticker.id);
    utassert(EntitySubscriberCount(&app, ticker.id) == 0);
    utassert(app.subs.len == 0);

    delete win;
    EntityDropAll(&app);
}

void TestEventEmitter() {
    TestSuite("event_emitter");
    EverySubscriberHearsIt();
    ASubscriptionCanBeGivenUp();
    AStaleEndSweepsTheSubscription();
}

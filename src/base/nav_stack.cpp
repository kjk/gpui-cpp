#include "base/nav_stack.h"

namespace gpui {

// ─── state ────────────────────────────────────────────────────────────────

Entity<NavStackState> NavStackStateNew(App* app) {
    Entity<NavStackState> e = EntityNewState<NavStackState>(app);
    NavStackState* s = e.Get(app);
    if (s) {
        s->self = e.id;
    }
    return e;
}

// The top view with its position, before an operation moves it.
static bool NavTop(const NavStackState* s, EntityId* view, int* index) {
    int depth = s->Depth();
    if (depth <= 0) {
        return false;
    }
    *view = s->Current();
    *index = depth - 1;
    return true;
}

// Takes the top view back onto the forward side. Every push has its own
// version, so an undo step is one view.
static bool NavUndoOne(NavStackState* s, EntityId* view) {
    Vec<NavEntry> undone = s->history.Undo();
    if (undone.len <= 0) {
        return false;
    }
    *view = undone[0].view;
    return true;
}

// cx.emit(event) + cx.notify() inside `stack.update(..)`: what notifies is the
// stack, so its observers run, not whichever view called in.
static void NavEmit(NavStackState* s, Ctx* cx, NavStackEvent event) {
    EntityEmit(cx->app, cx->win, s->self, &event);
    if (s->self.IsValid()) {
        NotifyEntity(cx->app, s->self, cx->win);
    } else {
        Notify(cx);
    }
}

// Records the change: `outgoing` stays mounted until its exit transition
// finishes, or not at all for an immediate change. A change already in transit
// is superseded, so at most one outgoing view is ever mounted; the element
// reverses its presence from wherever it is.
static void NavFinish(NavStackState* s, Ctx* cx, bool hasOutgoing,
                      EntityId outgoing, int index, NavOperation operation,
                      NavMotion motion, NavStackEvent event) {
    s->hasTransit = hasOutgoing;
    if (hasOutgoing) {
        s->transit = {outgoing, index, operation, motion};
    } else {
        s->transit = {};
    }
    NavEmit(s, cx, event);
}

static NavEntry NavEntryNew(EntityId view) {
    NavEntry entry;
    entry.view = view;
    return entry;
}

void NavStackPush(NavStackState* s, Ctx* cx, EntityId view, NavMotion motion) {
    EntityId outgoing = {};
    int index = 0;
    bool hasOutgoing = NavTop(s, &outgoing, &index);
    s->history.Push(NavEntryNew(view));
    NavFinish(s, cx, hasOutgoing, outgoing, index, NavOperation::Push, motion,
              NavStackEvent::Pushed);
}

EntityId NavStackPop(NavStackState* s, Ctx* cx, NavMotion motion) {
    if (s->Depth() <= 1) {
        return {};
    }
    EntityId popped = {};
    int index = 0;
    bool hasPopped = NavTop(s, &popped, &index);
    EntityId undone = {};
    if (!NavUndoOne(s, &undone)) {
        return {};
    }
    NavFinish(s, cx, hasPopped, popped, index, NavOperation::Pop, motion,
              NavStackEvent::Popped);
    return popped;
}

Vec<EntityId> NavStackPopToRoot(NavStackState* s, Ctx* cx, NavMotion motion) {
    EntityId outgoing = {};
    int index = 0;
    bool hasOutgoing = NavTop(s, &outgoing, &index);
    Vec<EntityId> popped;
    while (s->Depth() > 1) {
        EntityId view = {};
        if (!NavUndoOne(s, &view)) {
            break;
        }
        VecAppend(popped, view);
    }
    if (popped.len <= 0) {
        return popped;
    }
    NavFinish(s, cx, hasOutgoing, outgoing, index, NavOperation::Pop, motion,
              NavStackEvent::Popped);
    // Rust reverses the vector before returning it: root-side first.
    for (int i = 0, j = popped.len - 1; i < j; i++, j--) {
        EntityId tmp = popped[i];
        popped[i] = popped[j];
        popped[j] = tmp;
    }
    return popped;
}

EntityId NavStackForward(NavStackState* s, Ctx* cx, NavMotion motion) {
    EntityId outgoing = {};
    int index = 0;
    bool hasOutgoing = NavTop(s, &outgoing, &index);
    Vec<NavEntry> redone = s->history.Redo();
    if (redone.len <= 0) {
        return {};
    }
    EntityId view = redone[0].view;
    NavFinish(s, cx, hasOutgoing, outgoing, index, NavOperation::Push, motion,
              NavStackEvent::Forwarded);
    return view;
}

EntityId NavStackReplace(NavStackState* s, Ctx* cx, EntityId view,
                         NavMotion motion) {
    EntityId replaced = {};
    int index = 0;
    if (!NavTop(s, &replaced, &index)) {
        NavStackPush(s, cx, view, motion);
        return {};
    }
    s->history.ReplaceCurrent(NavEntryNew(view));
    NavFinish(s, cx, true, replaced, index, NavOperation::Replace, motion,
              NavStackEvent::Replaced);
    return replaced;
}

void NavStackClear(NavStackState* s, Ctx* cx) {
    s->history.Clear();
    s->hasTransit = false;
    s->transit = {};
    NavEmit(s, cx, NavStackEvent::Cleared);
}

// ─── presence ─────────────────────────────────────────────────────────────
//
// This is crates/base/src/motion/presence.rs, written here because the C++
// motion core is being rewritten from upstream's layered motion in parallel
// and does not carry Presence yet. Once it does, this block goes and the two
// samples below call `motion::Presence::new(id, present).transition(t)
// .sample(window, cx)` instead; nothing outside this file reads it.
struct NavPresenceState {
    float from = 0;
    float target = 0;
    double startedAt = 0;
    float reversingFactor = 1.f;
    float durationMs = 0;
    bool init = false;
};

enum class NavMotionStatus : uint8_t {
    Delayed,
    Running,
    Finished
};

struct NavPresenceSample {
    motion::PresencePhase phase = motion::PresencePhase::Absent;
    float progress = 0;
    NavMotionStatus status = NavMotionStatus::Finished;

    bool ShouldRender() const { return phase != motion::PresencePhase::Absent; }
};

// Transition::progress, against the duration the state carries rather than the
// policy's — a reversal shortens the run it is reversing.
static float NavProgress(const motion::Transition& t, float elapsedMs,
                         float durationMs, NavMotionStatus* status) {
    if (elapsedMs < t.delayMs) {
        *status = NavMotionStatus::Delayed;
        return 0.f;
    }
    float active = elapsedMs - t.delayMs;
    if (durationMs <= 0 || active >= durationMs) {
        *status = NavMotionStatus::Finished;
        return 1.f;
    }
    *status = NavMotionStatus::Running;
    return active / durationMs;
}

static NavPresenceSample NavStableSample(bool present) {
    NavPresenceSample out;
    out.phase = present ? motion::PresencePhase::Present
                        : motion::PresencePhase::Absent;
    out.progress = present ? 1.f : 0.f;
    out.status = NavMotionStatus::Finished;
    return out;
}

static float NavLerp(float from, float to, float progress) {
    return from + (to - from) * progress;
}

static float NavClamp01(float v) {
    return v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
}

static NavPresenceSample NavSamplePresence(Ctx* cx, uint32_t key, bool present,
                                           const motion::Transition& t) {
    auto* st =
        (NavPresenceState*)MotionSlot(cx, key, (int)sizeof(NavPresenceState));
    if (!st) {
        return NavStableSample(present);
    }
    double now = MotionNow(cx);
    float target = present ? 1.f : 0.f;
    if (!st->init) {
        st->init = true;
        st->from = 0.f;
        st->target = target;
        st->startedAt = now;
        st->reversingFactor = 1.f;
        st->durationMs = t.durationMs;
    }
    if (MotionReduced() || t.durationMs <= 0) {
        if (st->from != target || st->target != target) {
            st->from = target;
            st->target = target;
            st->startedAt = now;
            st->reversingFactor = 1.f;
            st->durationMs = t.durationMs;
        }
        return NavStableSample(present);
    }

    float elapsedMs = (float)((now - st->startedAt) * 1000.0);
    NavMotionStatus status = NavMotionStatus::Finished;
    // The time fraction of the run, which the eased value is read at. What the
    // sample reports is the presence *value* — 1 present, 0 absent — the way
    // the settled samples do.
    float fraction = NavProgress(t, elapsedMs, st->durationMs, &status);
    float sampled = NavLerp(st->from, st->target, MotionSample(t, fraction));
    float progress = sampled;

    if (st->target != target) {
        // The target moved. Reversing a run that is part way through shortens
        // the return by however far it had got, so an interrupted change comes
        // back at the pace it left at rather than taking a full duration.
        bool reversing = target == st->from;
        float reversingFactor =
            reversing
                ? NavClamp01(MotionSample(t, fraction) * st->reversingFactor +
                             (1.f - st->reversingFactor))
                : 1.f;
        float duration = t.durationMs * reversingFactor;
        st->from = sampled;
        st->target = target;
        st->startedAt = now;
        st->reversingFactor = reversingFactor;
        st->durationMs = duration;
        float initial = NavProgress(t, 0, duration, &status);
        progress = NavLerp(sampled, target, MotionSample(t, initial));
    }

    if (status == NavMotionStatus::Delayed ||
        status == NavMotionStatus::Running) {
        MotionWantsFrame(cx);
    }
    if (status == NavMotionStatus::Finished) {
        return NavStableSample(present);
    }
    NavPresenceSample out;
    out.phase = present ? motion::PresencePhase::Entering
                        : motion::PresencePhase::Exiting;
    out.progress = progress;
    out.status = status;
    return out;
}

// fn page_id(view): ("nav-stack", view.entity_id()), then presence.rs's
// ElementId::NamedChild(id, "__presence").
static uint32_t NavPageKey(EntityId view) {
    uint32_t id = (uint32_t)HashClickId(StrL("nav-stack")) * 31u +
                  ((uint32_t)view.index * 2654435761u ^ view.gen);
    return id * 31u + (uint32_t)HashClickId(StrL("__presence"));
}

// ─── element ──────────────────────────────────────────────────────────────

NavStack* NavStack::New(Ctx* cx, Entity<NavStackState> state) {
    NavStack* n = ArenaNew<NavStack>(cx->a);
    n->cx = cx;
    n->state = state;
    return n;
}

NavStack* NavStack::Transition(const motion::Transition& value) {
    transition = value;
    hasTransition = true;
    return this;
}

NavStack* NavStack::Item(NavItemFn fn, void* userData) {
    item = fn;
    user = userData;
    return this;
}

// NavPage's own render: absolute, inset 0, with the view as its child. The
// item renderer refines what comes back.
static El* NavBuildPage(NavStack* stack, EntityId view, int index,
                        motion::PresencePhase phase, bool hasOperation,
                        NavOperation operation, float progress) {
    Ctx* cx = stack->cx;
    El* box = Div(cx->a)->SizeFull()->Absolute()->Top(0)->Left(0)->Child(
        EntityRender(cx->app, cx->win, cx->a, view));
    NavPage page;
    page.view = view;
    page.index = index;
    page.phase = phase;
    page.operation = operation;
    page.hasOperation = hasOperation;
    page.progress = progress;
    page.el = box;
    if (!stack->item) {
        return box;
    }
    El* refined = stack->item(stack->user, cx, page);
    return refined ? refined : box;
}

El* NavStack::IntoEl() {
    Arena* a = cx->a;
    // Rust's `.relative()`: an El here is relative unless it says Absolute, so
    // the two views of a transition already overlap inside this box.
    El* root = Div(a);
    NavStackState* s = state.Get(cx);
    if (!s) {
        return root;
    }
    s->self = state.id;

    EntityId current = s->Current();
    int depth = s->Depth();
    NavTransit transit = s->transit;
    bool hasTransit = s->hasTransit;

    bool immediate = MotionReduced() || !hasTransition ||
                     (hasTransit && transit.motion == NavMotion::Immediate);
    motion::Transition timing =
        immediate ? motion::Transition::New(0) : transition;

    // The outgoing view's presence is the change's clock: its exit runs 1 → 0,
    // reversing if the change is interrupted, and both pages of the change
    // read their progress from it.
    bool changing = false;
    float progress = 1.f;
    if (hasTransit) {
        NavPresenceSample sample =
            NavSamplePresence(cx, NavPageKey(transit.outgoing), false, timing);
        if (sample.ShouldRender()) {
            changing = true;
            progress = 1.f - sample.progress;
        } else {
            s->hasTransit = false;
            hasTransit = false;
        }
    }

    // The current view's presence is sampled too, so a view brought back by
    // Forward or revealed by Pop starts its next exit from present. With
    // nothing changing it settles on the spot; the root does not animate in.
    if (current.IsValid()) {
        motion::Transition enter =
            changing ? timing : motion::Transition::New(0);
        NavSamplePresence(cx, NavPageKey(current), true, enter);
    }

    if (!current.IsValid()) {
        return root;
    }

    int index = depth - 1;
    if (!changing) {
        root->Child(NavBuildPage(this, current, index,
                                 motion::PresencePhase::Present, false,
                                 NavOperation::Push, 1.f));
        return root;
    }

    // A pushed or replacing view paints over what it covers; a popped view
    // paints over what it reveals. The item renderer is called in that order,
    // which is the order Rust fills its `items` vector in.
    if (transit.operation == NavOperation::Pop) {
        root->Child(NavBuildPage(this, current, index,
                                 motion::PresencePhase::Entering, true,
                                 transit.operation, progress));
        root->Child(NavBuildPage(this, transit.outgoing, transit.index,
                                 motion::PresencePhase::Exiting, true,
                                 transit.operation, progress));
    } else {
        root->Child(NavBuildPage(this, transit.outgoing, transit.index,
                                 motion::PresencePhase::Exiting, true,
                                 transit.operation, progress));
        root->Child(NavBuildPage(this, current, index,
                                 motion::PresencePhase::Entering, true,
                                 transit.operation, progress));
    }
    // Neither page takes pointer input while the change runs: the outgoing one
    // is on its way out, and the incoming one is not yet where it will be.
    // gpui's occlude() is StopMouseDown here.
    root->Child(
        Div(a)->SizeFull()->Absolute()->Top(0)->Left(0)->StopMouseDown());
    return root;
}

} // namespace gpui

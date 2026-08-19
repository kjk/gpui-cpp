/* Ported from crates/base/src/toast.rs advance.
 *
 * A toast's life is Starting, Present, Ending, gone, and the one subtlety is
 * that `paused` guards only the middle: a pointer resting on the stack stops
 * the countdown but not the animations. */

#include "Test.h"

static const ToastEntry* Find(const ToastStackState& s, int id) {
    for (int i = 0; i < s.n; i++) {
        if (s.entries[i].id == id) {
            return &s.entries[i];
        }
    }
    return nullptr;
}

static void AToastAnimatesInThenCountsDownThenLeaves() {
    ToastStackState s;
    utassert(ToastPush(&s, 1, 1000));
    utassert(Find(s, 1)->status == ToastStatus::Starting);

    // 400 ms of transition, and it is up.
    utassert(ToastAdvance(&s, 400, false));
    utassert(Find(s, 1)->status == ToastStatus::Present);

    // Most of the timeout passes with nothing to report.
    utassert(!ToastAdvance(&s, 900, false));
    utassert(Find(s, 1)->status == ToastStatus::Present);

    // The rest of it starts it leaving.
    utassert(ToastAdvance(&s, 200, false));
    utassert(Find(s, 1)->status == ToastStatus::Ending);

    // 200 ms of exit, and it is gone.
    utassert(ToastAdvance(&s, 200, false));
    utassert(Find(s, 1) == nullptr);
    utassert(s.n == 0);
}

static void PauseStopsTheCountdownButNotTheAnimations() {
    ToastStackState s;
    ToastPush(&s, 1, 1000);
    // Starting still finishes while paused: Rust guards only the Present arm.
    utassert(ToastAdvance(&s, 400, true));
    utassert(Find(s, 1)->status == ToastStatus::Present);

    // Now the countdown is frozen however long the pointer rests.
    utassert(!ToastAdvance(&s, 5000, true));
    utassert(Find(s, 1)->status == ToastStatus::Present);
    utassert(Find(s, 1)->timeoutRemainingMs == 1000);

    // And picks up exactly where it was when the pointer leaves.
    utassert(!ToastAdvance(&s, 999, false));
    utassert(ToastAdvance(&s, 1, false));
    utassert(Find(s, 1)->status == ToastStatus::Ending);
    // The exit runs while paused too.
    utassert(ToastAdvance(&s, 200, true));
    utassert(s.n == 0);
}

static void AToastWithoutATimeoutStays() {
    ToastStackState s;
    ToastPush(&s, 1, 0);
    ToastAdvance(&s, 400, false);
    utassert(Find(s, 1)->status == ToastStatus::Present);
    utassert(!ToastAdvance(&s, 100000, false));
    utassert(Find(s, 1)->status == ToastStatus::Present);
    // It goes when it is dismissed, and not before.
    utassert(ToastRemove(&s, 1));
    utassert(s.n == 0);
    utassert(!ToastRemove(&s, 1));
}

static void TheStackAdvancesEveryToastAtOnce() {
    ToastStackState s;
    ToastPush(&s, 1, 500);
    ToastPush(&s, 2, 1500);
    ToastAdvance(&s, 400, false);
    // The first times out and leaves while the second is still up.
    ToastAdvance(&s, 500, false);
    ToastAdvance(&s, 200, false);
    utassert(Find(s, 1) == nullptr);
    utassert(Find(s, 2)->status == ToastStatus::Present);
    utassert(s.n == 1);
}

static void TheStackHasABound() {
    ToastStackState s;
    for (int i = 0; i < kToastStackCap; i++) {
        utassert(ToastPush(&s, i, 1000));
    }
    utassert(!ToastPush(&s, 999, 1000));
    utassert(s.n == kToastStackCap);
}

void TestToast() {
    TestSuite("toast");
    AToastAnimatesInThenCountsDownThenLeaves();
    PauseStopsTheCountdownButNotTheAnimations();
    AToastWithoutATimeoutStays();
    TheStackAdvancesEveryToastAtOnce();
    TheStackHasABound();
}

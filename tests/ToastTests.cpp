/* Ported from crates/base/src/toast.rs advance.
 *
 * A toast's life is Starting, Present, Ending, gone, and the one subtlety is
 * that `paused` guards only the middle: a pointer resting on the stack stops
 * the countdown but not the animations. */

#include "Test.h"

static const ToastEntry* Find(const ToastStackState& s, int id) {
    for (int i = 0; i < s.entries.len; i++) {
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
    utassert(s.entries.len == 0);
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
    utassert(s.entries.len == 0);
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
    utassert(s.entries.len == 0);
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
    utassert(s.entries.len == 1);
}

static void TheStackGrowsLikeRustsVecDeque() {
    ToastStackState s;
    for (int i = 0; i < 100; i++) {
        utassert(ToastPush(&s, i, 1000));
    }
    utassert(s.entries.len == 100);
}

static void TheStackIsAsTallAsItsFrontPlusASliverOfEachOther() {
    // Three cards of eighty, peeking fourteen apart.
    const float heights[3] = {80, 80, 80};
    float collapsedOff[3];
    float expandedOff[3];
    float expanded = 0;
    float collapsed =
        ToastStackGeometry(heights, 3, kToastCollapsedPeek, kToastExpandedGap,
                           false, collapsedOff, expandedOff, &expanded);
    // Open: every card plus the gaps between them.
    utassertnear(expanded, 80 * 3 + 14 * 2);
    // Closed: the front card plus a sliver of each one behind it.
    utassertnear(collapsed, 80 + 14 * 2);
    // The newest is at the front, so it sits at the top of the stack and the
    // older ones fall behind it.
    utassertnear(collapsedOff[2], 0.f);
    utassertnear(collapsedOff[1], 14.f);
    utassertnear(collapsedOff[0], 28.f);
    utassertnear(expandedOff[2], 0.f);
    utassertnear(expandedOff[1], 94.f);
    utassertnear(expandedOff[0], 188.f);
}

static void ABottomStackGrowsUpwards() {
    const float heights[2] = {60, 100};
    float collapsedOff[2];
    float expandedOff[2];
    float expanded = 0;
    float collapsed = ToastStackGeometry(heights, 2, 14, 14, true, collapsedOff,
                                         expandedOff, &expanded);
    utassertnear(expanded, 60 + 100 + 14);
    // The taller card behind is what the closed stack has to fit.
    utassertnear(collapsed, 100 + 14);
    // Anchored at the bottom, the front card is the lowest one.
    utassertnear(collapsedOff[1], collapsed - 100);
    utassertnear(collapsedOff[0], collapsed - 60 - 14);
    utassertnear(expandedOff[1], expanded - 100);
    utassertnear(expandedOff[0], 0.f);
}

static void AnEmptyStackHasNoGeometry() {
    float expanded = 1;
    utassertnear(ToastStackGeometry(nullptr, 0, 14, 14, false, nullptr, nullptr,
                                    &expanded),
                 0.f);
    utassertnear(expanded, 0.f);
}

void TestToast() {
    TestSuite("toast");
    AToastAnimatesInThenCountsDownThenLeaves();
    PauseStopsTheCountdownButNotTheAnimations();
    AToastWithoutATimeoutStays();
    TheStackAdvancesEveryToastAtOnce();
    TheStackGrowsLikeRustsVecDeque();
    TheStackIsAsTallAsItsFrontPlusASliverOfEachOther();
    ABottomStackGrowsUpwards();
    AnEmptyStackHasNoGeometry();
}

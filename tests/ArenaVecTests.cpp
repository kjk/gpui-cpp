/* `ArenaVec` in src/base.h — the segmented array the markdown parser and the
 * widget builders fill. Not a port: the Rust side has no equivalent, and the
 * tests the tree already has only ever exercise vecs small enough to sit in
 * one segment. These are the cases past that edge — the segment boundary, the
 * pop that hands an earlier segment back the appends, and the iteration cache
 * that has to be right in both directions. */

#include "Test.h"

// Enough elements to cross the first three segment sizes (4, 16, 64) and land
// well inside the doubling that follows.
static const int kMany = 300;

static void AnEmptyVecHasNothing() {
    ArenaVec<int> v = {};
    utassert(v.len == 0);
    utassert(v.first == nullptr && v.last == nullptr);
    utassert(v.Flatten(nullptr) == nullptr);
}

static void AppendsReadBackAcrossSegments() {
    Arena* a = ArenaNew();
    ArenaVec<int> v = {};
    for (int i = 0; i < kMany; i++) {
        utassert(v.Append(a, i * 3));
        utassert(v.len == i + 1);
    }
    // Forward, which is what the cache is for.
    for (int i = 0; i < kMany; i++) {
        utassert(v[i] == i * 3);
    }
    // Backwards, which restarts the walk at every segment boundary and has to
    // land in the same places.
    for (int i = kMany - 1; i >= 0; i--) {
        utassert(v[i] == i * 3);
    }
    // Jumping around.
    utassert(v[0] == 0);
    utassert(v[kMany - 1] == (kMany - 1) * 3);
    utassert(v[17] == 51);
    utassert(v[4] == 12);
    // More than one segment, or the rest of this is testing nothing.
    utassert(v.first != v.last);
    ArenaDelete(a);
}

static void ElementsDoNotMoveWhenTheVecGrows() {
    Arena* a = ArenaNew();
    ArenaVec<int> v = {};
    v.Append(a, 1);
    int* firstEl = &v[0];
    for (int i = 1; i < kMany; i++) {
        v.Append(a, i + 1);
    }
    // The flat version reallocated and this pointer would be into an
    // abandoned block.
    utassert(firstEl == &v[0]);
    utassert(*firstEl == 1);
    ArenaDelete(a);
}

static void AppendManyFillsTheSameOrder() {
    Arena* a = ArenaNew();
    int src[kMany];
    for (int i = 0; i < kMany; i++) {
        src[i] = i;
    }
    ArenaVec<int> v = {};
    // Two runs, so the second one starts partway into a segment.
    utassert(v.AppendMany(a, src, 7));
    utassert(v.AppendMany(a, src + 7, kMany - 7));
    utassert(v.len == kMany);
    for (int i = 0; i < kMany; i++) {
        utassert(v[i] == i);
    }
    ArenaDelete(a);
}

static void ReserveSkipsTheClimb() {
    Arena* a = ArenaNew();
    ArenaVec<int> v = {};
    utassert(v.Reserve(a, kMany));
    for (int i = 0; i < kMany; i++) {
        v.Append(a, i);
    }
    // One segment took all of it, so `Flatten` is free and the fast path in
    // `operator[]` is the one being read.
    utassert(v.first == v.last);
    utassert(v.len == kMany);
    utassert(v[kMany - 1] == kMany - 1);
    ArenaDelete(a);
}

static void PopAndTruncateGiveTheRoomBack() {
    Arena* a = ArenaNew();
    ArenaVec<int> v = {};
    for (int i = 0; i < kMany; i++) {
        v.Append(a, i);
    }
    v.Pop();
    utassert(v.len == kMany - 1);
    utassert(v[v.len - 1] == kMany - 2);

    v.Truncate(10);
    utassert(v.len == 10);
    utassert(v[9] == 9);
    // Past the end of the first segment, so an earlier segment is the active
    // one again and the ones after it are empty.
    utassert(v.first != v.last);

    // Truncating to nothing keeps the segments, and refilling reuses them:
    // no arena is spent on the second pass.
    v.Truncate(0);
    utassert(v.len == 0);
    uint64_t before = a->pos;
    for (int i = 0; i < kMany; i++) {
        v.Append(a, i * 2);
    }
    utassert(a->pos == before);
    utassert(v.len == kMany);
    for (int i = 0; i < kMany; i++) {
        utassert(v[i] == i * 2);
    }

    // A truncate longer than the vec is not a resize.
    v.Truncate(kMany + 100);
    utassert(v.len == kMany);
    ArenaDelete(a);
}

static void PopAtASegmentBoundaryDoesNotAllocate() {
    Arena* a = ArenaNew();
    ArenaVec<int> v = {};
    // kArenaVecCap0 is 4, so this sits exactly on the boundary.
    for (int i = 0; i < 4; i++) {
        v.Append(a, i);
    }
    v.Append(a, 4);
    utassert(v.first != v.last);
    uint64_t before = a->pos;
    for (int i = 0; i < 50; i++) {
        v.Pop();
        v.Append(a, 4);
    }
    utassert(a->pos == before);
    utassert(v.len == 5);
    utassert(v[4] == 4);
    ArenaDelete(a);
}

static void FlattenIsOneArrayEitherWay() {
    Arena* a = ArenaNew();
    ArenaVec<int> few = {};
    for (int i = 0; i < 3; i++) {
        few.Append(a, i);
    }
    // One segment: the elements are already an array, so this is a pointer
    // into it and not a copy.
    int* flat = few.Flatten(a);
    utassert(flat == &few[0]);
    utassert(flat[2] == 2);

    ArenaVec<int> many = {};
    for (int i = 0; i < kMany; i++) {
        many.Append(a, i);
    }
    utassert(many.first != many.last);
    int* copy = many.Flatten(a);
    utassert(copy != nullptr);
    for (int i = 0; i < kMany; i++) {
        utassert(copy[i] == i);
    }
    ArenaDelete(a);
}

static void ACopyOfTheHandleSeesTheSameElements() {
    Arena* a = ArenaNew();
    ArenaVec<int> v = {};
    for (int i = 0; i < kMany; i++) {
        v.Append(a, i);
    }
    // The parser passes these around by value — a copy is a handle onto the
    // same segments, and reading through one is what the other reads.
    ArenaVec<int> copy = v;
    utassert(copy.len == v.len);
    for (int i = 0; i < kMany; i++) {
        utassert(copy[i] == i);
    }
    v[5] = 500;
    utassert(copy[5] == 500);
    ArenaDelete(a);
}

void TestArenaVec() {
    TestSuite("arena_vec");
    AnEmptyVecHasNothing();
    AppendsReadBackAcrossSegments();
    ElementsDoNotMoveWhenTheVecGrows();
    AppendManyFillsTheSameOrder();
    ReserveSkipsTheClimb();
    PopAndTruncateGiveTheRoomBack();
    PopAtASegmentBoundaryDoesNotAllocate();
    FlattenIsOneArrayEitherWay();
    ACopyOfTheHandleSeesTheSameElements();
}

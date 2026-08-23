/* `Vec`'s borrowed storage in src/base.h — `VecUseInline`, which lends a vec
 * an array on the caller's stack to start in so that a vec which never
 * outgrows it costs no allocation at all. The capacity rides in the sign of
 * `cap`, and the two things that have to tell owned storage from borrowed are
 * growing (the block cannot be realloc'd, so it is copied out of) and freeing
 * (it must not be), which is what these check. Taffy's flex line list is the
 * caller it was written for. */

#include "Test.h"

static void AnInlineVecStartsInTheBufferAndDoesNotAllocate() {
    int buf[4] = {};
    Vec<int> v;
    VecUseInline(v, buf);
    utassert(v.len == 0);
    utassert(v.Cap() == 4);
    utassert(v.els == buf);

    for (int i = 0; i < 4; i++) {
        utassert(v.Append(i * 7));
    }
    utassert(v.len == 4);
    // Still the caller's array: the elements were written straight into it,
    // and nothing was allocated to hold them.
    utassert(v.els == buf);
    for (int i = 0; i < 4; i++) {
        utassert(v[i] == i * 7);
        utassert(buf[i] == i * 7);
    }
}

static void TheAppendPastTheBufferMovesToTheHeapWithTheElements() {
    int buf[4] = {};
    Vec<int> v;
    VecUseInline(v, buf);
    for (int i = 0; i < 4; i++) {
        v.Append(i * 7);
    }
    utassert(v.Append(99));

    // Off the buffer and onto a block of its own, carrying everything that
    // was already there.
    utassert(v.els != buf);
    utassert(v.cap > 0);
    utassert(v.Cap() >= 5);
    utassert(v.len == 5);
    for (int i = 0; i < 4; i++) {
        utassert(v[i] == i * 7);
    }
    utassert(v[4] == 99);
    // The array it was lent is the caller's and was left as it was.
    for (int i = 0; i < 4; i++) {
        utassert(buf[i] == i * 7);
    }

    // And it goes on growing the ordinary way from there.
    for (int i = 0; i < 200; i++) {
        v.Append(1000 + i);
    }
    utassert(v.len == 205);
    utassert(v[204] == 1199);
    utassert(v[0] == 0);
}

static void AReserveStraightPastTheBufferAlsoCarries() {
    int buf[4] = {};
    Vec<int> v;
    VecUseInline(v, buf);
    v.Append(1);
    v.Append(2);
    // One jump, rather than an append at a time.
    utassert(VecReserve(v, 64) != nullptr);
    utassert(v.els != buf);
    utassert(v.Cap() >= 64);
    utassert(v.len == 2);
    utassert(v[0] == 1 && v[1] == 2);
}

static void ResetGivesTheBufferBackWithoutFreeingIt() {
    int buf[4] = {7, 7, 7, 7};
    Vec<int> v;
    VecUseInline(v, buf);
    v.Append(1);
    // The interesting half: `Reset` must not hand a stack address to free().
    // Reaching the end of this test at all is the assertion; the rest says
    // the vec is empty afterwards and the array is untouched.
    v.Reset();
    utassert(v.len == 0);
    utassert(v.Cap() == 0);
    utassert(v.els == nullptr);
    utassert(buf[0] == 1);

    // Empty and owning nothing, it allocates the way any other vec does.
    v.Append(5);
    utassert(v.len == 1 && v[0] == 5);
    utassert(v.els != buf);
}

static void ADestructorOnABorrowedVecFreesNothing() {
    int buf[4] = {};
    {
        Vec<int> v;
        VecUseInline(v, buf);
        v.Append(3);
        v.Append(4);
    }
    // Same: the point is that the scope closed without free() seeing the
    // stack array.
    utassert(buf[0] == 3 && buf[1] == 4);
}

static void ACopyOfABorrowedVecOwnsItsOwnElements() {
    int buf[4] = {};
    Vec<int> v;
    VecUseInline(v, buf);
    v.Append(11);
    v.Append(22);

    Vec<int> copy = v;
    utassert(copy.len == 2);
    utassert(copy[0] == 11 && copy[1] == 22);
    // The copy borrows nothing — it allocated — so writing through one is not
    // seen by the other, and the copy's own destructor has a block to free.
    utassert(copy.els != buf);
    utassert(copy.cap > 0);
    v[0] = 99;
    utassert(copy[0] == 11);
}

static void ClearOnABorrowedVecZeroesTheBuffer() {
    int buf[4] = {};
    Vec<int> v;
    VecUseInline(v, buf);
    for (int i = 0; i < 4; i++) {
        v.Append(i + 1);
    }
    // Clear zeroes the whole capacity, and the capacity here is the array —
    // the size of it is what the sign of `cap` has to be read through.
    v.Clear();
    utassert(v.len == 0);
    for (int i = 0; i < 4; i++) {
        utassert(buf[i] == 0);
    }
}

static void AnOrdinaryVecIsUnaffected() {
    Vec<int> v;
    utassert(v.Cap() == 0);
    for (int i = 0; i < 100; i++) {
        v.Append(i);
    }
    utassert(v.len == 100);
    utassert(v.cap >= 100 && v.Cap() == v.cap);
    utassert(v[99] == 99);
    v.Reset();
    utassert(v.len == 0 && v.cap == 0 && v.els == nullptr);
}

// The caller `VecUseInline` was written for, driven past its buffer. Taffy's
// flex line list is the only one in the tree, and no layout anywhere in the
// tree — the taffy suite, the story gallery, the showcase, the benchmarks —
// produces more than one flex line, so nothing else would ever move it off
// the stack. A wrapping row twelve items wide makes six lines, and what is
// checked is that the lines the buffer already held came through the move
// with the rest.
static void TaffyFlexLinesSurviveOutgrowingTheBuffer() {
    taffy::TaffyTree tree;
    tree.Init(16);

    taffy::Style childStyle;
    childStyle.size = taffy::SizeDim::FromLengths(50.0f, 10.0f);

    const int kItems = 12; // two to a line, so six lines
    taffy::NodeId kids[kItems];
    for (int i = 0; i < kItems; i++) {
        kids[i] = tree.NewLeaf(childStyle);
    }

    taffy::Style rootStyle;
    rootStyle.size = taffy::SizeDim::FromLengths(100.0f, 60.0f);
    rootStyle.flexWrap = taffy::FlexWrap::Wrap;
    taffy::NodeId root = tree.NewWithChildren(rootStyle, kids, kItems);

    tree.ComputeLayout(root, taffy::SizeAvail::MaxContent());

    for (int i = 0; i < kItems; i++) {
        const taffy::Layout& l = tree.GetLayout(kids[i]);
        utassert(l.size.w == 50.0f);
        utassert(l.size.h == 10.0f);
        utassert(l.location.x == (i % 2 == 0 ? 0.0f : 50.0f));
        utassert(l.location.y == (float)(i / 2) * 10.0f);
    }
}

void TestVec() {
    TestSuite("vec");
    AnInlineVecStartsInTheBufferAndDoesNotAllocate();
    TheAppendPastTheBufferMovesToTheHeapWithTheElements();
    AReserveStraightPastTheBufferAlsoCarries();
    ResetGivesTheBufferBackWithoutFreeingIt();
    ADestructorOnABorrowedVecFreesNothing();
    ACopyOfABorrowedVecOwnsItsOwnElements();
    ClearOnABorrowedVecZeroesTheBuffer();
    AnOrdinaryVecIsUnaffected();
    TaffyFlexLinesSurviveOutgrowingTheBuffer();
}

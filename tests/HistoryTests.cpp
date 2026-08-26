/* The string specialization of crates/base/src/history.rs. */

#include "Test.h"

static void HistoryUsesGrowingStorageAndDropsTheRedoTail() {
    History h;
    for (int i = 0; i < 100; i++) {
        h.Push(StrL("value"));
    }
    utassert(h.items.len == 100);
    utassert(h.cursor == 99);
    h.Undo();
    h.Undo();
    h.Push(StrL("replacement"));
    utassert(h.items.len == 99);
    utassert(h.cursor == 98);
    utassert(!h.CanRedo());
}

static void HistoryUsesRustsDefaultMaximum() {
    History h;
    for (int i = 0; i < 1100; i++) {
        h.Push(StrL("value"));
    }
    utassert(h.items.len == 1000);
    utassert(h.cursor == 999);
}

void TestHistory() {
    TestSuite("history");
    HistoryUsesGrowingStorageAndDropsTheRedoTail();
    HistoryUsesRustsDefaultMaximum();
}

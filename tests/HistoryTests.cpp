/* Port of crates/base/src/history.rs tests. */

#include "Test.h"

struct TabIndex {
    int tabIndex = 0;
    uint64_t version = 0;

    uint64_t Version() const { return version; }
    void SetVersion(uint64_t value) { version = value; }
    bool operator==(const TabIndex& other) const {
        return tabIndex == other.tabIndex;
    }
};

static TabIndex TabAt(int index) {
    TabIndex item;
    item.tabIndex = index;
    return item;
}

static void HistoryMatchesUpstreamUndoRedoOrder() {
    History<TabIndex> history;
    history.MaxUndos(100);
    history.Push(TabAt(0));
    history.Push(TabAt(3));
    history.Push(TabAt(2));
    history.Push(TabAt(1));

    utassert(history.Version() == 4);
    Vec<TabIndex> changes = history.Undo();
    utassert(changes.len == 1 && changes[0].tabIndex == 1);

    changes = history.Undo();
    utassert(changes.len == 1 && changes[0].tabIndex == 2);

    // This is intentionally unlike a conventional cursor history: push does
    // not clear the redo stack in the Rust implementation.
    history.Push(TabAt(5));
    changes = history.Redo();
    utassert(changes.len == 1 && changes[0].tabIndex == 2);
    changes = history.Redo();
    utassert(changes.len == 1 && changes[0].tabIndex == 1);

    const int expected[] = {1, 2, 5, 3, 0};
    for (int want : expected) {
        changes = history.Undo();
        utassert(changes.len == 1 && changes[0].tabIndex == want);
    }
    utassert(history.Undo().len == 0);
}

static void UniqueHistoryRetainsOnlyTheNewestEqualItem() {
    History<TabIndex> history;
    history.MaxUndos(100).Unique();
    history.Push(TabAt(0));
    history.Push(TabAt(1));
    history.Push(TabAt(1));
    history.Push(TabAt(2));
    history.Push(TabAt(1));

    utassert(history.Version() == 5);
    utassert(history.Undos().len == 3);
    utassert(history.Undos()[2].tabIndex == 1);

    Vec<TabIndex> changes = history.Undo();
    utassert(changes.len == 1 && changes[0].tabIndex == 1);
    utassert(history.Redos().len == 1);

    history.Push(TabAt(2));
    utassert(history.Undos().len == 2);
    utassert(history.Redos().len == 1);
    changes = history.Redo();
    utassert(changes.len == 1 && changes[0].tabIndex == 1);

    history.Push(TabAt(3));
    utassert(history.Version() == 7);
    utassert(history.Undos().len == 4);
    for (int i = 0; i < 4; i++) {
        history.Undo();
    }
    utassert(history.Undos().len == 0);
    utassert(history.Redos().len == 4);
}

static void GroupingAndIntervalsShareVersions() {
    History<TabIndex> grouped;
    grouped.StartGrouping();
    grouped.Push(TabAt(1));
    grouped.Push(TabAt(2));
    grouped.EndGrouping();
    utassert(grouped.Version() == 0);
    Vec<TabIndex> changes = grouped.Undo();
    utassert(changes.len == 2);
    utassert(changes[0].tabIndex == 2 && changes[1].tabIndex == 1);

    History<TabIndex> timed;
    timed.GroupInterval(10);
    timed.Push(TabAt(1));
    timed.Push(TabAt(2));
    utassert(timed.Version() == 0);
    timed.lastChangedAt = TimeNow() - 11;
    timed.Push(TabAt(3));
    utassert(timed.Version() == 1);
    changes = timed.Undo();
    utassert(changes.len == 1 && changes[0].tabIndex == 3);
    changes = timed.Undo();
    utassert(changes.len == 2);
}

static void MaximumIgnoreAndClearMatchTheRustState() {
    History<TabIndex> history;
    history.MaxUndos(3);
    for (int i = 0; i < 5; i++) {
        history.Push(TabAt(i));
    }
    utassert(history.Undos().len == 3);
    utassert(history.Undos()[0].tabIndex == 2);

    history.SetIgnoring(true);
    utassert(history.IsIgnoring());
    // Rust exposes the flag for the caller; push itself does not consult it.
    history.Push(TabAt(5));
    utassert(history.Undos()[2].tabIndex == 5);

    history.Undo();
    utassert(history.Redos().len == 1);
    history.Clear();
    utassert(!history.CanUndo() && !history.CanRedo());

    History<TabIndex> defaults;
    for (int i = 0; i < 1100; i++) {
        defaults.Push(TabAt(i));
    }
    utassert(defaults.Undos().len == 1000);
    utassert(defaults.Undos()[0].tabIndex == 100);
}

void TestHistory() {
    TestSuite("history");
    HistoryMatchesUpstreamUndoRedoOrder();
    UniqueHistoryRetainsOnlyTheNewestEqualItem();
    GroupingAndIntervalsShareVersions();
    MaximumIgnoreAndClearMatchTheRustState();
}

/* Port of crates/base/src/input/base/undo_manager.rs and change.rs.

   Each edit first makes a transaction. Compatible adjacent transactions then
   coalesce until an explicit boundary — a cursor move, a paste, a blur — so a
   run of typing undoes as one step rather than a character at a time. A caller
   that performs one logical edit through several callbacks (IME composition)
   brackets them with UndoBeginTransaction / UndoCommitTransaction.

   Rust clones changes in and out of the stacks; ownership is explicit here, so
   a Change moves and the stack that holds it frees its two strings. */

#include "gpui/Gpui.h"

namespace gpui {

static const int kMaxUndoTransactions = 1000;
static const int kMaxChangesPerTransaction = 1000;

static void ChangeFree(Change* c) {
    StrFree(c->oldText);
    StrFree(c->newText);
    c->oldText = {};
    c->newText = {};
}

static void TransactionFree(UndoTransaction* t) {
    for (int i = 0; i < t->len; i++) {
        ChangeFree(&t->changes[i]);
    }
    free(t->changes);
    t->changes = nullptr;
    t->len = 0;
    t->cap = 0;
}

static void TransactionPush(UndoTransaction* t, Change c) {
    if (t->len == t->cap) {
        int cap = t->cap ? t->cap * 2 : 4;
        auto* p = (Change*)realloc(t->changes, (size_t)cap * sizeof(Change));
        if (!p) {
            ChangeFree(&c);
            return;
        }
        t->changes = p;
        t->cap = cap;
    }
    t->changes[t->len++] = c;
}

static void StackClear(Vec<UndoTransaction>& v) {
    for (int i = 0; i < v.len; i++) {
        TransactionFree(&v[i]);
    }
    v.len = 0;
}

UndoManager::~UndoManager() {
    StackClear(undos);
    StackClear(redos);
    if (hasPending) {
        ChangeFree(&pending);
    }
}

// is_adjacent: whether the change coming in continues the one before it, which
// is what lets a run of the same intent stay one undo step.
static bool IsAdjacent(EditIntent intent, const Change& prev,
                       const Change& cur) {
    auto hasNewline = [](Str s) {
        for (int i = 0; i < s.len; i++) {
            if (s.s[i] == '\n' || s.s[i] == '\r') {
                return true;
            }
        }
        return false;
    };
    switch (intent) {
        case EditIntent::Typing:
            return prev.oldRange.IsEmpty() && cur.oldRange.IsEmpty() &&
                   !hasNewline(prev.newText) && !hasNewline(cur.newText) &&
                   prev.newRange.end == cur.oldRange.start;
        case EditIntent::Backspace:
            return prev.newText.len == 0 && cur.newText.len == 0 &&
                   cur.oldRange.end == prev.oldRange.start;
        case EditIntent::DeleteForward:
            return prev.newText.len == 0 && cur.newText.len == 0 &&
                   cur.oldRange.start == prev.oldRange.start;
        case EditIntent::Atomic:
            return false;
    }
    return false;
}

static bool StrSame(Str a, Str b) {
    if (a.len != b.len) {
        return false;
    }
    return a.len == 0 || memcmp(a.s, b.s, (size_t)a.len) == 0;
}

static bool RangeSame(Selection a, Selection b) {
    return a.start == b.start && a.end == b.end;
}

static void PushTransaction(UndoManager* m, Change change, EditIntent intent) {
    StackClear(m->redos);
    bool canCoalesce = false;
    if (!m->coalescingBoundary && intent != EditIntent::Atomic &&
        m->undos.len > 0) {
        UndoTransaction& prev = m->undos[m->undos.len - 1];
        canCoalesce = prev.intent == intent &&
                      prev.len < kMaxChangesPerTransaction && prev.len > 0 &&
                      IsAdjacent(intent, prev.changes[prev.len - 1], change);
    }
    if (canCoalesce) {
        TransactionPush(&m->undos[m->undos.len - 1], change);
        return;
    }
    if (m->undos.len >= kMaxUndoTransactions) {
        TransactionFree(&m->undos[0]);
        memmove(m->undos.els, m->undos.els + 1,
                (size_t)(m->undos.len - 1) * sizeof(UndoTransaction));
        m->undos.len--;
    }
    UndoTransaction t = {};
    t.intent = intent;
    TransactionPush(&t, change);
    m->undos.Append(t);
    m->coalescingBoundary = intent == EditIntent::Atomic;
}

void UndoRecordTransaction(UndoManager* m, Change change, EditIntent intent) {
    if (m->ignoring) {
        ChangeFree(&change);
        return;
    }
    // A no-op edit records nothing, but still ends the run before it, so the
    // undo history keeps whatever it already had.
    if (RangeSame(change.oldRange, change.newRange) &&
        StrSame(change.oldText, change.newText)) {
        ChangeFree(&change);
        UndoBreakCoalescing(m);
        return;
    }
    if (m->transactionOpen) {
        if (m->hasPending) {
            // The bracket keeps the first change's old side and takes the
            // latest new side, so the whole composition undoes at once.
            StrFree(m->pending.newText);
            m->pending.newRange = change.newRange;
            m->pending.newText = change.newText;
            m->pending.selAfter = change.selAfter;
            StrFree(change.oldText);
        } else {
            m->pending = change;
            m->hasPending = true;
        }
        return;
    }
    PushTransaction(m, change, intent);
}

void UndoBeginTransaction(UndoManager* m) {
    if (m->transactionOpen) {
        return;
    }
    m->transactionOpen = true;
    if (m->hasPending) {
        ChangeFree(&m->pending);
        m->hasPending = false;
    }
}

void UndoCommitTransaction(UndoManager* m) {
    if (!m->transactionOpen) {
        return;
    }
    m->transactionOpen = false;
    if (!m->hasPending) {
        return;
    }
    Change c = m->pending;
    m->hasPending = false;
    m->pending = {};
    if (!RangeSame(c.oldRange, c.newRange) || !StrSame(c.oldText, c.newText)) {
        PushTransaction(m, c, EditIntent::Atomic);
    } else {
        ChangeFree(&c);
    }
}

void UndoBreakCoalescing(UndoManager* m) {
    UndoCommitTransaction(m);
    m->coalescingBoundary = true;
}

bool UndoIsIgnoring(const UndoManager* m) {
    return m->ignoring;
}

void UndoSetIgnoring(UndoManager* m, bool ignoring) {
    m->ignoring = ignoring;
    if (ignoring) {
        UndoCommitTransaction(m);
    }
}

void UndoClear(UndoManager* m) {
    StackClear(m->undos);
    StackClear(m->redos);
    m->transactionOpen = false;
    if (m->hasPending) {
        ChangeFree(&m->pending);
        m->hasPending = false;
    }
    m->hasPendingIntent = false;
    m->coalescingBoundary = false;
}

const UndoTransaction* UndoPopUndo(UndoManager* m) {
    UndoCommitTransaction(m);
    if (m->undos.len == 0) {
        return nullptr;
    }
    UndoTransaction t = m->undos[m->undos.len - 1];
    m->undos.len--;
    m->redos.Append(t);
    m->coalescingBoundary = true;
    // The caller applies the changes in reverse, which is what Rust's
    // `.iter().rev()` hands it.
    return &m->redos[m->redos.len - 1];
}

const UndoTransaction* UndoPopRedo(UndoManager* m) {
    UndoCommitTransaction(m);
    if (m->redos.len == 0) {
        return nullptr;
    }
    UndoTransaction t = m->redos[m->redos.len - 1];
    m->redos.len--;
    m->undos.Append(t);
    m->coalescingBoundary = true;
    return &m->undos[m->undos.len - 1];
}

} // namespace gpui

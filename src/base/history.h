#ifndef GPUI_BASE_HISTORY_H_
#define GPUI_BASE_HISTORY_H_
/* Generic undo/redo history — crates/base/src/history.rs.
   (`crates/ui/src/history.rs` is a re-export of this one, not a second copy.)

   I is the C++ HistoryItem convention: a POD-friendly value with

       uint64_t Version() const;
       void SetVersion(uint64_t);
       bool operator==(const I&) const;

   Rust expresses those three operations as Clone + PartialEq and the
   HistoryItem trait. Vec copies the value bytes here, so the repository's
   usual explicit-ownership rule applies to any pointers an item carries. */

#include "gpui/gpui.h"

namespace gpui {

template <typename I>
struct History {
    Vec<I> undos;
    Vec<I> redos;
    double lastChangedAt = TimeNow();
    uint64_t version = 0;
    bool ignore = false;
    int maxUndos = 1000;
    double groupInterval = 0;
    bool hasGroupInterval = false;
    bool grouping = false;
    bool unique = false;

    History& MaxUndos(int n) {
        maxUndos = n;
        return *this;
    }

    History& Unique(bool on = true) {
        unique = on;
        return *this;
    }

    // Rust takes an instant::Duration. Seconds are the runtime clock's unit;
    // the millisecond spelling keeps ordinary callers out of conversions.
    History& GroupInterval(double seconds) {
        groupInterval = seconds >= 0 ? seconds : 0;
        hasGroupInterval = true;
        return *this;
    }

    History& GroupIntervalMs(int64_t ms) {
        return GroupInterval(ms > 0 ? (double)ms / 1000.0 : 0);
    }

    void StartGrouping() { grouping = true; }
    void EndGrouping() { grouping = false; }

    uint64_t Version() const { return version; }
    bool IsIgnoring() const { return ignore; }
    void SetIgnoring(bool on) { ignore = on; }

    const Vec<I>& Undos() const { return undos; }
    const Vec<I>& Redos() const { return redos; }
    bool CanUndo() const { return undos.len > 0; }
    bool CanRedo() const { return redos.len > 0; }

    void Clear() {
        undos.Clear();
        redos.Clear();
    }

    void Push(I item) {
        uint64_t nextVersion = IncVersion();
        if (maxUndos <= 0) {
            return;
        }
        if (undos.len >= maxUndos) {
            RemoveAt(&undos, 0);
        }
        if (unique) {
            RetainDifferent(&undos, item);
            RetainDifferent(&redos, item);
        }
        item.SetVersion(nextVersion);
        undos.Append(item);
        // Deliberately do not clear redos. Upstream keeps them when a new
        // item is pushed after undo; its own test then redoes the older path.
    }

    // Empty means there was nothing to undo/redo. Otherwise every returned
    // item shares the version of the first one, in pop order.
    Vec<I> Undo() { return MoveVersion(&undos, &redos); }
    Vec<I> Redo() { return MoveVersion(&redos, &undos); }

  private:
    uint64_t IncVersion() {
        double now = TimeNow();
        if (!grouping &&
            (!hasGroupInterval || now - lastChangedAt > groupInterval)) {
            version++;
        }
        lastChangedAt = now;
        return version;
    }

    static void RemoveAt(Vec<I>* values, int at) {
        if (!values || at < 0 || at >= values->len) {
            return;
        }
        for (int i = at + 1; i < values->len; i++) {
            (*values)[i - 1] = (*values)[i];
        }
        values->len--;
    }

    static void RetainDifferent(Vec<I>* values, const I& item) {
        int out = 0;
        for (int i = 0; i < values->len; i++) {
            if ((*values)[i] == item) {
                continue;
            }
            if (out != i) {
                (*values)[out] = (*values)[i];
            }
            out++;
        }
        values->len = out;
    }

    static bool ContainsVersion(const Vec<I>& values, uint64_t version) {
        for (int i = 0; i < values.len; i++) {
            if (values[i].Version() == version) {
                return true;
            }
        }
        return false;
    }

    static Vec<I> MoveVersion(Vec<I>* from, Vec<I>* to) {
        Vec<I> changes;
        if (!from || from->len <= 0) {
            return changes;
        }
        I first = (*from)[from->len - 1];
        from->len--;
        changes.Append(first);
        uint64_t pickedVersion = first.Version();
        // This is deliberately the Rust implementation's whole-stack test,
        // followed by a pop, rather than merely looking at the last item.
        while (ContainsVersion(*from, pickedVersion)) {
            I change = (*from)[from->len - 1];
            from->len--;
            changes.Append(change);
        }
        for (int i = 0; i < changes.len; i++) {
            to->Append(changes[i]);
        }
        return changes;
    }
};

} // namespace gpui
#endif // GPUI_BASE_HISTORY_H_

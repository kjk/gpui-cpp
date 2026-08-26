/* Undo/redo history — crates/base/src/history.rs.
   (`crates/ui/src/history.rs` is a re-export of this one, not a second copy.)

   Rust's `History<I: HistoryItem>` is generic over the item and keeps two
   stacks plus a version counter and a grouping interval. This specialization
   keeps the string/cursor API the C++ tree uses, with Rust's default maximum
   of 1000 rather than a fixed storage array. */

#include "gpui/gpui.h"

namespace gpui {

struct History {
    Vec<Str> items;
    int cursor = -1;
    int maxItems = 1000;

    void Push(Str s);
    bool CanUndo() const;
    bool CanRedo() const;
    Str Undo();
    Str Redo();
};

} // namespace gpui

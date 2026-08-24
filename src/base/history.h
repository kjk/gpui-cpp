/* Undo/redo history — crates/base/src/history.rs.
   (`crates/ui/src/history.rs` is a re-export of this one, not a second copy.)

   Rust's `History<I: HistoryItem>` is generic over the item and keeps two
   stacks plus a version counter and a grouping interval. This is the shape the
   tree actually uses: one array of strings and a cursor into it, which is what
   a text field's undo needs and all it needs. */

#include "gpui/gpui.h"

namespace gpui {

struct History {
    Str items[32] = {};
    int n = 0;
    int cursor = -1;

    void Push(Str s);
    bool CanUndo() const;
    bool CanRedo() const;
    Str Undo();
    Str Redo();
};

} // namespace gpui

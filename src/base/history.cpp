#include "base/history.h"

namespace gpui {

void History::Push(Str s) {
    // A new edit after undo drops the redo tail.
    if (cursor + 1 < items.len) {
        items.len = cursor + 1;
    }
    if (items.len >= maxItems && items.len > 0) {
        for (int i = 1; i < items.len; i++) {
            items[i - 1] = items[i];
        }
        items.len--;
        cursor--;
    }
    if (items.Append(s)) {
        cursor = items.len - 1;
    }
}
bool History::CanUndo() const {
    return cursor > 0;
}
bool History::CanRedo() const {
    return cursor + 1 < items.len;
}
Str History::Undo() {
    if (CanUndo()) {
        cursor--;
    }
    return cursor >= 0 ? items[cursor] : Str{};
}
Str History::Redo() {
    if (CanRedo()) {
        cursor++;
    }
    return cursor >= 0 ? items[cursor] : Str{};
}

} // namespace gpui

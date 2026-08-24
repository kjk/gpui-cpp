#include "base/history.h"

namespace gpui {

void History::Push(Str s) {
    if (n < 32) {
        cursor++;
        n = cursor + 1;
        items[cursor] = s;
    }
}
bool History::CanUndo() const {
    return cursor > 0;
}
bool History::CanRedo() const {
    return cursor + 1 < n;
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

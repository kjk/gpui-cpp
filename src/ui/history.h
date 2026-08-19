/* History helper — crates/ui/src/history.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

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

} // namespace component
} // namespace gpui

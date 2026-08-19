/* Unstyled tree — crates/base/src/tree.rs */

#include "gpui/gpui.h"

namespace gpui {

struct Tree {
    static El* New(Ctx* cx);
};
struct TreeItem {
    static El* New(Ctx* cx, int clickId = 0);
};
} // namespace gpui

/* Unstyled tree — crates/base/src/tree.rs */

#include "gpui/Gpui.h"

namespace gpui {

struct Tree {
    static El* New(Arena* a);
};
struct TreeItem {
    static El* New(Arena* a, int clickId = 0);
};
} // namespace gpui

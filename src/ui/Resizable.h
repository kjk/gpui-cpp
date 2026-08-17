/* Unstyled resizable — crates/base/src/resizable */

#include "gpui/Gpui.h"

namespace gpui {

struct Resizable {
    static El* New(Arena* a, Str id);
};
struct ResizablePanel {
    static El* New(Arena* a);
};
} // namespace gpui

/* Unstyled resizable — crates/base/src/resizable */

#include "gpui/Gpui.h"

namespace gpui {

struct Resizable {
    static El* New(Ctx* cx, Str id);
};
struct ResizablePanel {
    static El* New(Ctx* cx);
};
} // namespace gpui

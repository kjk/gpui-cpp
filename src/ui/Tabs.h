/* Unstyled tabs — crates/base/src/tabs.rs */

#include "gpui/Gpui.h"

namespace gpui {

struct Tabs {
    static El* New(Ctx* cx, Str id);
};
struct Tab {
    static El* New(Ctx* cx, Str id, int clickId = 0);
};
} // namespace gpui

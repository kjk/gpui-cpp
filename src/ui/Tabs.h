/* Unstyled tabs — crates/base/src/tabs.rs */

#pragma once

#include "gpui/Gpui.h"

namespace gpui {

struct Tabs {
    static El* New(Arena* a, Str id);
};
struct Tab {
    static El* New(Arena* a, Str id, int clickId = 0);
};
} // namespace gpui

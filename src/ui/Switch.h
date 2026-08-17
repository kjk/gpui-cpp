/* Unstyled switch — crates/base/src/switch.rs */

#include "gpui/Gpui.h"

namespace gpui {

struct Switch {
    static El* New(Arena* a, Str id, int clickId = 0);
};

struct SwitchTrack {
    static El* New(Arena* a, Str id);
};

struct SwitchThumb {
    static El* New(Arena* a);
};
} // namespace gpui

/* Unstyled switch — crates/base/src/switch.rs */

#include "gpui/gpui.h"

namespace gpui {

struct Switch {
    static El* New(Ctx* cx, Str id, int clickId = 0);
};

struct SwitchTrack {
    static El* New(Ctx* cx, Str id);
};

struct SwitchThumb {
    static El* New(Ctx* cx);
};
} // namespace gpui

/* Unstyled radio / radio group — crates/base/src/radio.rs */

#include "gpui/gpui.h"

namespace gpui {

struct Radio {
    static El* New(Ctx* cx, Str id, int clickId = 0);
};

struct RadioGroup {
    static El* New(Ctx* cx, Str id);
};
} // namespace gpui

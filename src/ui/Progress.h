/* Unstyled progress — crates/base/src/progress.rs */

#include "gpui/Gpui.h"

namespace gpui {

struct Progress {
    static El* New(Ctx* cx, Str id);
};

struct ProgressTrack {
    static El* New(Ctx* cx);
};

struct ProgressIndicator {
    static El* New(Ctx* cx);
};
} // namespace gpui

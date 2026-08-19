/* Unstyled progress — crates/base/src/progress.rs */

#include "gpui/gpui.h"

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

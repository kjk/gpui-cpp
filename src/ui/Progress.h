/* Unstyled progress — crates/base/src/progress.rs */

#include "gpui/Gpui.h"

namespace gpui {

struct Progress {
    static El* New(Arena* a, Str id);
};

struct ProgressTrack {
    static El* New(Arena* a);
};

struct ProgressIndicator {
    static El* New(Arena* a);
};
} // namespace gpui

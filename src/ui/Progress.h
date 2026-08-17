/* Unstyled progress — crates/base/src/progress.rs */

#pragma once

#include "gpui/Gpui.h"

struct Progress {
    static El* New(Arena* a, Str id);
};

struct ProgressTrack {
    static El* New(Arena* a);
};

struct ProgressIndicator {
    static El* New(Arena* a);
};

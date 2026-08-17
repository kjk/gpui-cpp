/* Unstyled toast — crates/base/src/toast.rs */

#pragma once

#include "gpui/Gpui.h"

struct Toast {
    static El* New(Arena* a, Str id);
};

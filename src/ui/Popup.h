/* Unstyled popup — crates/base/src/popup.rs */

#pragma once

#include "gpui/Gpui.h"

struct Popup {
    El* root = nullptr;

    static Popup* New(Arena* a, Str id, El* trigger);
    Popup* Content(El* content);
    El* IntoEl();
};

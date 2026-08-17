/* Unstyled sheet — crates/base/src/sheet.rs */

#pragma once

#include "gpui/Gpui.h"

struct Sheet {
    El* root = nullptr;

    static Sheet* New(Arena* a);
    Sheet* Overlay(El* overlay);
    Sheet* Surface(El* surface);
    El* IntoEl();
};

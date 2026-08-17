/* Unstyled sheet — crates/base/src/sheet.rs */

#include "gpui/Gpui.h"

namespace gpui {

struct Sheet {
    El* root = nullptr;

    static Sheet* New(Ctx* cx);
    Sheet* Overlay(El* overlay);
    Sheet* Surface(El* surface);
    El* IntoEl();
};
} // namespace gpui

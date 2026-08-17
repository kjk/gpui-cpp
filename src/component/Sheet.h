/* Themed sheet — crates/ui/src/sheet.rs */

#pragma once

#include "component/Common.h"

namespace gpui {

namespace component {

struct Sheet {
    Arena* a = nullptr;
    Str title = {};
    bool open = false;
    El* body = nullptr;
    Func0 onClose;

    static Sheet* New(Arena* a);
    Sheet* Title(Str s);
    Sheet* Open(bool v);
    Sheet* Body(El* e);
    Sheet* OnClose(Func0 fn);
    El* IntoEl(WinSize size);
};

} // namespace component
} // namespace gpui

/* Themed dialog — crates/ui/src/dialog */

#pragma once

#include "component/Common.h"

namespace component {

struct Dialog {
    Arena* a = nullptr;
    Str title = {};
    Str description = {};
    bool open = false;
    El* body = nullptr;
    Func0 onClose;
    Func0 onOk;

    static Dialog* New(Arena* a);
    Dialog* Title(Str s);
    Dialog* Description(Str s);
    Dialog* Open(bool v);
    Dialog* Body(El* e);
    Dialog* OnClose(Func0 fn);
    Dialog* OnOk(Func0 fn);
    El* IntoEl(WinSize size);
};

} // namespace component

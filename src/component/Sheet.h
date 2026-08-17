/* Themed sheet — crates/ui/src/sheet.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Sheet {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str title = {};
    bool open = false;
    El* body = nullptr;
    Func0 onClose;

    static Sheet* New(Ctx* cx);
    Sheet* Title(Str s);
    Sheet* Open(bool v);
    Sheet* Body(El* e);
    Sheet* OnClose(Func0 fn);
    El* IntoEl(WinSize size);
};

} // namespace component
} // namespace gpui

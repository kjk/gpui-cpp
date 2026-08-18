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
    Listener onClose;

    static Sheet* New(Ctx* cx);
    Sheet* Title(Str s);
    Sheet* Open(bool v);
    Sheet* Body(El* e);
    Sheet* OnClose(Listener fn);
    El* IntoEl(WinSize size);
};

} // namespace component
} // namespace gpui

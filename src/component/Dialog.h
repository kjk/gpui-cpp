/* Themed dialog — crates/ui/src/dialog */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Dialog {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str title = {};
    Str description = {};
    bool open = false;
    El* body = nullptr;
    Listener onClose;
    Listener onOk;

    static Dialog* New(Ctx* cx);
    Dialog* Title(Str s);
    Dialog* Description(Str s);
    Dialog* Open(bool v);
    Dialog* Body(El* e);
    Dialog* OnClose(Listener fn);
    Dialog* OnOk(Listener fn);
    El* IntoEl(WinSize size);
};

} // namespace component
} // namespace gpui

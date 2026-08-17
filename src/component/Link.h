/* Themed link — crates/ui/src/link.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Link {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Str href = {};
    Str text = {};
    bool disabled = false;
    Func1<Str> onOpen;

    static Link* New(Ctx* cx, Str id);
    Link* Href(Str s);
    Link* Text(Str s);
    Link* Disabled(bool v);
    Link* OnOpen(Func1<Str> fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui

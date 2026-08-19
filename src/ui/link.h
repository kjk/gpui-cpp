/* Themed link — crates/ui/src/link.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct Link {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Str href = {};
    Str text = {};
    bool disabled = false;
    Listener onOpen;

    static Link* New(Ctx* cx, Str id);
    Link* Href(Str s);
    Link* Text(Str s);
    Link* Disabled(bool v);
    Link* OnOpen(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui

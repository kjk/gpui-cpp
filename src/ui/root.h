/* Themed Root — crates/ui/src/root.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct Root {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    El* child = nullptr;
    bool bordered = true;

    static Root* New(Ctx* cx);
    Root* Bordered(bool v);
    Root* Child(El* e);
    El* IntoEl();
};

} // namespace component
} // namespace gpui

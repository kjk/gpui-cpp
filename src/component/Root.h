/* Themed Root — crates/ui/src/root.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Root {
    Arena* a = nullptr;
    El* child = nullptr;
    bool bordered = true;

    static Root* New(Arena* a);
    Root* Bordered(bool v);
    Root* Child(El* e);
    El* IntoEl();
};

} // namespace component
} // namespace gpui

/* Themed dock (simplified) — crates/ui/src/dock */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Dock {
    Arena* a = nullptr;
    El* left = nullptr;
    El* center = nullptr;
    El* right = nullptr;

    static Dock* New(Arena* a);
    Dock* Left(El* e);
    Dock* Center(El* e);
    Dock* Right(El* e);
    El* IntoEl();
};

} // namespace component
} // namespace gpui

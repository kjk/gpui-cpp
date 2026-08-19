/* Themed dock (simplified) — crates/ui/src/dock */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct Dock {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    El* left = nullptr;
    El* center = nullptr;
    El* right = nullptr;

    static Dock* New(Ctx* cx);
    Dock* Left(El* e);
    Dock* Center(El* e);
    Dock* Right(El* e);
    El* IntoEl();
};

} // namespace component
} // namespace gpui

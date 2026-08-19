/* Themed label — crates/ui/src/label.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct Label {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str text = {};
    Str secondary = {};
    bool masked = false;
    bool semibold = false;
    float font = 14;

    static Label* New(Ctx* cx, Str text);
    Label* Secondary(Str s);
    Label* Masked(bool v);
    Label* Semibold();
    Label* Font(float px);
    El* IntoEl();
};

} // namespace component
} // namespace gpui

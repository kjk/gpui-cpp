/* Themed rating — crates/ui/src/rating.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct Rating {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    int value = 0;
    int max = 5;
    bool disabled = false;
    Rgba color = {};
    bool hasColor = false;
    UiSize size = UiSize::Medium;
    Listener onChange;

    static Rating* New(Ctx* cx);
    Rating* Value(int v);
    Rating* Max(int v);
    Rating* Disabled(bool v);
    Rating* Color(Rgba c);
    Rating* WithSize(UiSize s);
    Rating* OnChange(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui

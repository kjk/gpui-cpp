/* Themed title bar — crates/ui/src/title_bar.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct TitleBar {
    Arena* a = nullptr;
    Str title = {};
    El* right = nullptr;

    static TitleBar* New(Arena* a, Str title);
    TitleBar* Right(El* e);
    El* IntoEl();
};

} // namespace component
} // namespace gpui

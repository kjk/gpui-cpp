/* Themed spinner — crates/ui/src/spinner.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Spinner {
    Arena* a = nullptr;
    UiSize size = UiSize::Medium;
    IconName icon = IconName::Loader;
    Rgba color = {};
    bool hasColor = false;

    static Spinner* New(Arena* a);
    Spinner* WithSize(UiSize s);
    Spinner* Icon(IconName n);
    Spinner* Color(Rgba c);
    El* IntoEl();
};

} // namespace component
} // namespace gpui

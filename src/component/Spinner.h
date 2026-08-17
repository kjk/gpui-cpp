/* Themed spinner — crates/ui/src/spinner.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Spinner {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    UiSize size = UiSize::Medium;
    float px = 0;
    IconName icon = IconName::Loader;
    Rgba color = {};
    bool hasColor = false;

    static Spinner* New(Ctx* cx);
    Spinner* WithSize(UiSize s);
    Spinner* Size(float v);
    Spinner* Icon(IconName n);
    Spinner* Color(Rgba c);
    El* IntoEl();
};

} // namespace component
} // namespace gpui

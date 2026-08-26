/* Themed breadcrumb — crates/ui/src/breadcrumb.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

// One level of the trail. Rust's BreadcrumbItem is an element of its own, so
// the click and the disabled flag belong to the level that has them rather
// than to the trail around it.
struct BreadcrumbItem {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str label = {};
    Listener onClick;
    bool disabled = false;
    // Filled in by Breadcrumb, which is the only thing that knows.
    bool isLast = false;
    int ix = 0;

    static BreadcrumbItem* New(Ctx* cx, Str label);
    BreadcrumbItem* Disabled(bool v);
    BreadcrumbItem* OnClick(Listener fn);
    El* IntoEl();
};

struct Breadcrumb {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<BreadcrumbItem*> items;

    static Breadcrumb* New(Ctx* cx);
    Breadcrumb* Child(BreadcrumbItem* item);
    // Rust's `impl From<&'static str> for BreadcrumbItem`: a level that does
    // nothing but name itself.
    Breadcrumb* Child(Str label);
    El* IntoEl();
};

} // namespace component
} // namespace gpui

#ifndef GPUI_UI_ICON_H_
#define GPUI_UI_ICON_H_
/* Themed icon wrapper — crates/ui/src/icon.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

// Rust uses an IconNamed trait so application enums can supply paths. The
// POD port represents the trait's one return value directly; any custom icon
// set can return one of these without inheritance, RTTI or retained objects.
struct IconNamed {
    Str path = {};

    static IconNamed From(IconName name);
};

struct Icon {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    IconName name = IconName::None;
    Str path = {};
    float size = 0;
    float rotation = 0;
    Rgba color = {};
    bool hasSize = false;
    bool hasColor = false;

    static Icon* New(Ctx* cx, IconName name);
    static Icon* New(Ctx* cx, IconNamed named);
    static Icon* Empty(Ctx* cx);
    Icon* Path(Str assetPath);
    Icon* Size(float v);
    Icon* Size(UiSize v);
    Icon* Color(Rgba c);
    // GPUI rotations are turns: 0.25 is ninety degrees clockwise.
    Icon* Transform(float turns);
    Icon* Rotate(float turns);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
#endif // GPUI_UI_ICON_H_

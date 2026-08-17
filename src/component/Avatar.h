/* Themed avatar — crates/ui/src/avatar */

#include "component/Common.h"

namespace gpui {

namespace component {

// crates/ui/src/avatar/mod.rs avatar_size()
float AvatarSizePx(UiSize s);

struct Avatar {
    Arena* a = nullptr;
    Str initials = {};
    Rgba bg = {};
    bool hasBg = false;
    float size = 48;
    // Set by WithSize; drives the fallback text size the way avatar_text_size
    // does. -1 means the caller gave an explicit pixel size.
    float textPx = -1;
    float radius = -1;
    float borderW = 1;
    Rgba borderC = {};
    bool hasBorderC = false;
    IconName placeholder = IconName::User;

    static Avatar* New(Arena* a);
    Avatar* Initials(Str s);
    Avatar* Bg(Rgba c);
    Avatar* Size(float v);
    Avatar* WithSize(UiSize s);
    Avatar* Radius(float v);
    Avatar* Border(float w, Rgba c);
    Avatar* Placeholder(IconName n);
    El* IntoEl();
};

} // namespace component
} // namespace gpui

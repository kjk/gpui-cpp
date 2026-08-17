/* Themed tag — crates/ui/src/tag.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

enum class TagVariant : uint8_t {
    Primary,
    Secondary,
    Danger,
    Success,
    Warning,
    Info
};

struct Tag {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    TagVariant variant = TagVariant::Secondary;
    bool outline = false;
    UiSize size = UiSize::Medium;
    float radius = -1;
    Str text = {};
    Rgba customBg = {};
    Rgba customFg = {};
    bool hasCustom = false;

    static Tag* New(Ctx* cx, Str text);
    Tag* Primary();
    Tag* Secondary();
    Tag* Danger();
    Tag* Success();
    Tag* Warning();
    Tag* Info();
    Tag* Outline();
    Tag* WithSize(UiSize s);
    Tag* Radius(float v);
    Tag* Custom(Rgba bg, Rgba fg);
    El* IntoEl();
};

} // namespace component
} // namespace gpui

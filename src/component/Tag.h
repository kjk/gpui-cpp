/* Themed tag — crates/ui/src/tag.rs */

#pragma once

#include "component/Common.h"

namespace component {

enum class TagVariant : u8 { Primary, Secondary, Danger, Success, Warning, Info };

struct Tag {
    Arena* a = nullptr;
    TagVariant variant = TagVariant::Secondary;
    bool outline = false;
    UiSize size = UiSize::Medium;
    Str text = {};

    static Tag* New(Arena* a, Str text);
    Tag* Primary();
    Tag* Secondary();
    Tag* Danger();
    Tag* Success();
    Tag* Warning();
    Tag* Info();
    Tag* Outline();
    Tag* WithSize(UiSize s);
    El* IntoEl();
};

} // namespace component

/* Themed accordion — crates/ui/src/accordion.rs */

#pragma once

#include "component/Common.h"

namespace gpui {

namespace component {

struct AccordionItem {
    Str title = {};
    Str body = {};
    bool open = false;
    IconName icon = IconName::None;
    Str tag = {};
    bool settings = false;
};

struct Accordion {
    Arena* a = nullptr;
    Str id = {};
    bool multiple = false;
    bool bordered = true;
    bool disabled = false;
    UiSize size = UiSize::Medium;
    AccordionItem items[8] = {};
    int nItems = 0;
    Func1<int> onToggle;

    static Accordion* New(Arena* a, Str id);
    Accordion* Multiple(bool v);
    Accordion* Bordered(bool v);
    Accordion* Disabled(bool v);
    Accordion* WithSize(UiSize s);
    Accordion* Item(Str title, Str body, bool open);
    Accordion* SettingsItem(Str title, Str body, bool open, IconName icon,
                            Str tag);
    Accordion* OnToggle(Func1<int> fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui

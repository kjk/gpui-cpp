/* Themed accordion — crates/ui/src/accordion.rs */

#pragma once

#include "component/Common.h"

namespace component {

struct AccordionItem {
    Str title = {};
    Str body = {};
    bool open = false;
};

struct Accordion {
    Arena* a = nullptr;
    Str id = {};
    bool multiple = false;
    bool bordered = true;
    bool disabled = false;
    AccordionItem items[8] = {};
    int nItems = 0;
    Func1<int> onToggle;

    static Accordion* New(Arena* a, Str id);
    Accordion* Multiple(bool v);
    Accordion* Bordered(bool v);
    Accordion* Disabled(bool v);
    Accordion* Item(Str title, Str body, bool open);
    Accordion* OnToggle(Func1<int> fn);
    El* IntoEl();
};

} // namespace component

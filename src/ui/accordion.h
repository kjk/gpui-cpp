/* Themed accordion — crates/ui/src/accordion.rs */

#include "ui/sizing.h"

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
    Ctx* cx = nullptr;
    Str id = {};
    bool multiple = false;
    bool bordered = true;
    bool disabled = false;
    UiSize size = UiSize::Medium;
    AccordionItem items[8] = {};
    int nItems = 0;
    Listener onToggle;

    static Accordion* New(Ctx* cx, Str id);
    Accordion* Multiple(bool v);
    Accordion* Bordered(bool v);
    Accordion* Disabled(bool v);
    Accordion* WithSize(UiSize s);
    Accordion* Item(Str title, Str body, bool open);
    Accordion* SettingsItem(Str title, Str body, bool open, IconName icon,
                            Str tag);
    Accordion* OnToggle(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui

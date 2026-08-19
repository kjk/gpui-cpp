/* Themed select — crates/ui/src/select.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct Select {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Str options[24] = {};
    int n = 0;
    // -1: nothing picked, so the placeholder shows.
    int selected = -1;
    Str placeholder = {};
    Str titlePrefix = {};
    Str empty = {};
    float width = kFill;
    float menuWidth = 0; // menu_width(px(..)): wider than the trigger
    float menuMaxH = 0;
    UiSize size = UiSize::Medium;
    IconName icon = IconName::None; // replaces the caret when set
    bool disabled = false;
    bool cleanable = false;
    bool appearance = true;
    bool open = false;
    Listener onChange;
    Listener onToggle;
    Listener onClear;

    static Select* New(Ctx* cx, Str id);
    Select* Option(Str s);
    Select* Options(const char* const* items, int count);
    Select* Selected(int i);
    Select* Placeholder(Str s);
    Select* TitlePrefix(Str s);
    Select* Empty(Str s);
    Select* W(float v);
    Select* MenuWidth(float v);
    Select* MenuMaxH(float v);
    Select* WithSize(UiSize s);
    Select* Icon(IconName n);
    Select* Disabled(bool v);
    Select* Cleanable(bool v = true);
    Select* Appearance(bool v);
    Select* Open(bool v);
    Select* OnChange(Listener fn);
    Select* OnToggle(Listener fn);
    Select* OnClear(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui

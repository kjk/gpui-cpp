/* Themed combobox — crates/ui/src/combobox.rs

   A ComboBox is a Select whose list is always searchable: the same
   SearchableList underneath, with a query field in the dropdown. */

#include "ui/select.h"

namespace gpui {

namespace component {

struct Combobox {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<SearchableListState> state = {};
    const SearchableItem* items = nullptr;
    int nItems = 0;
    const Str* sections = nullptr;
    int nSections = 0;
    Str placeholder = {};
    Str searchPlaceholder = {};
    Str empty = {};
    // An optional icon before the title, as the icons story shows.
    IconName icon = IconName::None;
    float width = 280;
    float menuMaxH = 0;
    bool disabled = false;
    bool cleanable = false;
    InputState* query = nullptr;
    Listener onQueryFocus = {};
    Listener onToggle;
    Listener onClear;

    static Combobox* New(Ctx* cx, Str id, Entity<SearchableListState> state,
                         InputState* query);
    Combobox* Items(const SearchableItem* items, int n);
    Combobox* Sections(const Str* titles, int n);
    Combobox* Placeholder(Str s);
    Combobox* SearchPlaceholder(Str s);
    Combobox* Empty(Str s);
    Combobox* Icon(IconName n);
    Combobox* W(float v);
    Combobox* MenuMaxH(float v);
    Combobox* Disabled(bool v);
    Combobox* Cleanable(bool v = true);
    Combobox* Multiple(bool v = true);
    Combobox* OnQueryFocus(Listener fn);
    Combobox* OnToggle(Listener fn);
    Combobox* OnClear(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui

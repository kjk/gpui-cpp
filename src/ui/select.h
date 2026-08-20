/* Themed select — crates/ui/src/select.rs

   Rust's Select is a trigger over a SearchableList: the list holds the items,
   the query and the selection, and the Select is what shows the selection and
   opens the list. So is this one. */

#include "ui/searchable_list.h"

namespace gpui {

namespace component {

struct Select {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<SearchableListState> state = {};
    // The items, which are the caller's: they have to outlive the frame, so a
    // static array rather than one built on the frame arena.
    const SearchableItem* items = nullptr;
    int nItems = 0;
    const Str* sections = nullptr;
    int nSections = 0;
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
    bool focusRing = true;
    // searchable(true): a query field at the top of the list, which is what
    // makes a Select a ComboBox in all but name.
    InputState* query = nullptr;
    Listener onQueryFocus = {};
    Listener onToggle;
    Listener onClear;

    static Select* New(Ctx* cx, Str id, Entity<SearchableListState> state);
    Select* Items(const SearchableItem* items, int n);
    Select* Sections(const Str* titles, int n);
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
    // FocusableExt::focus_ring: no focus appearance on this control.
    Select* FocusRing(bool v);
    Select* Searchable(InputState* query, Listener onFocus);
    // Multiple: the list toggles rather than replaces, and the trigger says
    // how many are picked.
    Select* Multiple(bool v = true);
    Select* OnToggle(Listener fn);
    Select* OnClear(Listener fn);
    El* IntoEl();
};

// What the trigger shows: the one selected title, "N selected" for several,
// or the placeholder for none. Rust builds the same three cases.
Str SelectTriggerTitle(const SearchableListState* s, Str placeholder,
                       Str titlePrefix, Arena* a);

// SelectState::toggle / clear, as handlers a trigger can bind straight to.
void SelectToggleOpen(SearchableListState* s, Ctx* cx);
void SelectClear(SearchableListState* s, Ctx* cx);

} // namespace component
} // namespace gpui

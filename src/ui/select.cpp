#include "ui/select.h"
#include "ui/button.h"

namespace gpui {

namespace component {

Select* Select::New(Ctx* cx, Str id, Entity<SearchableListState> state) {
    Arena* a = cx->a;
    Select* s = ArenaNew<Select>(a);
    s->a = a;
    s->cx = cx;
    s->id = id;
    s->state = state;
    return s;
}
Select* Select::Items(const SearchableItem* it, int n) {
    items = it;
    nItems = n;
    return this;
}
Select* Select::Sections(const Str* titles, int n) {
    sections = titles;
    nSections = n;
    return this;
}
Select* Select::Placeholder(Str s) {
    placeholder = s;
    return this;
}
Select* Select::TitlePrefix(Str s) {
    titlePrefix = s;
    return this;
}
Select* Select::Empty(Str s) {
    empty = s;
    return this;
}
Select* Select::W(float v) {
    width = v;
    return this;
}
Select* Select::MenuWidth(float v) {
    menuWidth = v;
    return this;
}
Select* Select::MenuMaxH(float v) {
    menuMaxH = v;
    return this;
}
Select* Select::WithSize(UiSize s) {
    size = s;
    return this;
}
Select* Select::Icon(IconName i) {
    icon = i;
    return this;
}
Select* Select::CheckIcon(IconName n) {
    checkIcon = n;
    return this;
}
Select* Select::Disabled(bool v) {
    disabled = v;
    return this;
}
Select* Select::Cleanable(bool v) {
    cleanable = v;
    return this;
}
Select* Select::Appearance(bool v) {
    appearance = v;
    return this;
}
Select* Select::FocusRing(bool v) {
    focusRing = v;
    return this;
}
Select* Select::Searchable(InputState* q, Listener onFocus) {
    query = q;
    onQueryFocus = onFocus;
    return this;
}
Select* Select::Multiple(bool v) {
    SearchableListState* s = state.Get(cx);
    if (s) {
        s->mode = v ? SearchableListMode::Multi : SearchableListMode::Single;
        s->closeOnSelect = !v;
    }
    return this;
}
Select* Select::OnToggle(Listener fn) {
    onToggle = fn;
    return this;
}
Select* Select::OnClear(Listener fn) {
    onClear = fn;
    return this;
}

Str SelectTriggerTitle(const SearchableListState* s, Str placeholder,
                       Str titlePrefix, Arena* a) {
    Str none = placeholder.s ? placeholder : StrL("Please select");
    if (!s || s->nSelected == 0 || !s->items) {
        return none;
    }
    if (s->nSelected > 1) {
        // Rust shows the picked items as tags; the trigger says how many when
        // there is no room for that.
        return StrDup(a, fmt("%d selected", s->nSelected));
    }
    int ix = s->selected[0];
    if (ix < 0 || ix >= s->nItems) {
        return none;
    }
    Str title = s->items[ix].title;
    if (titlePrefix.s) {
        return StrDup(a, fmt("%s%s", titlePrefix, title));
    }
    return title;
}

void SelectToggleOpen(SearchableListState* s, Ctx* cx) {
    if (!s) {
        return;
    }
    s->open = !s->open;
    // Opening starts the keyboard on whatever is already picked, so the first
    // arrow steps from there rather than from the top.
    s->list.selected = -1;
    if (s->open && s->nSelected > 0) {
        for (int m = 0; m < s->nMatches; m++) {
            if (s->matches[m] == s->selected[0]) {
                s->list.selected = m;
                break;
            }
        }
    }
    Notify(cx);
}

void SelectClear(SearchableListState* s, Ctx* cx) {
    if (!s) {
        return;
    }
    s->nSelected = 0;
    Notify(cx);
}

Select* Select::Trigger(El* e) {
    trigger = e;
    return this;
}
Select* Select::Footer(El* e) {
    footer = e;
    return this;
}

El* Select::IntoEl() {
    const Theme& th = cx->theme();
    SearchableListState* s = state.Get(cx);
    // input_size / input_text_size, by size.
    float h = 32, padX = 10, font = 14, caret = 16;
    if (size == UiSize::Large) {
        h = 44;
        padX = 12;
        font = 16;
    } else if (size == UiSize::Small) {
        h = 24;
        padX = 8;
        caret = 14;
    } else if (size == UiSize::XSmall) {
        h = 20;
        padX = 4;
        font = 12;
        caret = 12;
    }
    bool open = s && s->open && !disabled;
    bool hasValue = s && s->nSelected > 0;
    Str title = SelectTriggerTitle(s, placeholder, titlePrefix, a);
    El* box = Div(a)
                  ->FlexRow()
                  ->W(width)
                  ->H(h)
                  ->PadX(padX)
                  ->Gap(4)
                  ->ItemsCenter()
                  ->JustifyBetween();
    if (appearance) {
        box->Radius(th.radius)
            ->Bg(disabled ? th.muted : th.inputBg)
            ->Border(1, open ? th.ring : th.inputBorder);
        // select.rs: a disabled trigger is the whole control at half
        // strength, over and above the muted surface it already takes.
        if (disabled) {
            box->Opacity(0.5f);
        }
    }
    Rgba fg = disabled ? th.mutedFg : th.foreground;
    if (this->trigger) {
        // render_trigger: the caller's element is the whole of the trigger's
        // content, caret included, so nothing else goes in beside it.
        box->Child(this->trigger->W(kFill)->MinW(0));
    } else {
        box->Child(
            TextEl(a, title)->Font(font)->Fg(hasValue ? fg : th.mutedFg));
        if (cleanable && hasValue && !disabled) {
            box->Child(Button::New(cx, StrDup(a, fmt("%s-clean", id)))
                           ->Text()
                           ->WithSize(UiSize::XSmall)
                           ->Icon(IconName::X)
                           ->OnClick(onClear)
                           ->IntoEl());
        } else if (icon != IconName::None) {
            // A custom icon replaces the caret, at xsmall.
            box->Child(IconEl(a, icon, 12)->Fg(th.mutedFg));
        } else {
            box->Child(IconEl(a, IconName::ChevronDown, caret)->Fg(th.mutedFg));
        }
    }
    if (!disabled) {
        BindClick(box, id, onToggle);
        box->FocusRing(focusRing);
    }

    El* menu = nullptr;
    if (open) {
        // The list is the whole dropdown: the query, the sections, the checks
        // and the empty state are all its own.
        SearchableList* list =
            SearchableList::New(cx, StrDup(a, fmt("%s-list", id)), state, query)
                ->Items(items, nItems)
                ->W(menuWidth > 0 ? menuWidth : (width > 0 ? width : 240))
                ->CheckIcon(checkIcon);
        if (sections) {
            list->Sections(sections, nSections);
        }
        if (query) {
            list->OnQueryFocus(onQueryFocus);
        }
        if (menuMaxH > 0) {
            list->MaxH(menuMaxH);
        }
        if (footer) {
            list->Footer(footer);
        }
        if (empty.s) {
            list->Empty(
                Div(a)->H(96)->W(kFill)->ItemsCenter()->JustifyCenter()->Child(
                    TextEl(a, empty)->Font(font)->Fg(th.mutedFg)));
        }
        menu = list->IntoEl();
    } else if (s) {
        // A closed list still has to know its items and what the query left,
        // so the trigger can name the selection and the keys can move it.
        SearchableListSearch(s, items, nItems,
                             query ? InputValue(query) : Str{});
    }
    El* root = gpui::Select::New(cx, id)->W(width)->Child(box);
    El* wrap = Popup::New(cx, StrDup(a, fmt("%s-popup", id)), root)
                   ->Content(menu)
                   ->IntoEl();
    // The five bindings, on the element that holds both the trigger and the
    // popup: the trigger is focusable and so is the query field inside the
    // list, and the context is above whichever of them has the focus.
    if (!disabled) {
        SelectBindKeys(cx, wrap, state);
    }
    return wrap;
}

} // namespace component
} // namespace gpui

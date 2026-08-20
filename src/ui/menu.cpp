#include "ui/menu.h"
#include "ui/kbd.h"

namespace gpui {

namespace component {

Entity<PopupMenuState> PopupMenuStateFor(Ctx* cx, Str id) {
    return KeyedEntity<PopupMenuState>(cx, (uint32_t)HashClickId(id));
}

PopupMenu* PopupMenu::New(Ctx* cx, Str id) {
    return New(cx, id, PopupMenuStateFor(cx, id));
}

PopupMenu* PopupMenu::New(Ctx* cx, Str id, Entity<PopupMenuState> state) {
    Arena* a = cx->a;
    PopupMenu* m = ArenaNew<PopupMenu>(a);
    m->a = a;
    m->cx = cx;
    m->id = id;
    m->state = state;
    return m;
}

static MenuItem* MenuAdd(PopupMenu* m, MenuItemKind kind) {
    if (m->n >= 32) {
        return nullptr;
    }
    MenuItem* it = &m->items[m->n++];
    *it = MenuItem{};
    it->kind = kind;
    return it;
}

PopupMenu* PopupMenu::Menu(Str label, IconName icon) {
    MenuItem* it = MenuAdd(this, MenuItemKind::Item);
    if (it) {
        it->label = label;
        it->icon = icon;
    }
    return this;
}
PopupMenu* PopupMenu::MenuWithCheck(Str label, bool checked) {
    MenuItem* it = MenuAdd(this, MenuItemKind::Item);
    if (it) {
        it->label = label;
        it->checked = checked;
    }
    return this;
}
PopupMenu* PopupMenu::MenuWithKbd(Str label, Str kbd) {
    MenuItem* it = MenuAdd(this, MenuItemKind::Item);
    if (it) {
        it->label = label;
        it->kbd = kbd;
    }
    return this;
}
PopupMenu* PopupMenu::Separator() {
    MenuAdd(this, MenuItemKind::Separator);
    return this;
}
PopupMenu* PopupMenu::Label(Str label) {
    MenuItem* it = MenuAdd(this, MenuItemKind::Label);
    if (it) {
        it->label = label;
    }
    return this;
}
PopupMenu* PopupMenu::Element(El* el) {
    MenuItem* it = MenuAdd(this, MenuItemKind::Item);
    if (it) {
        it->element = el;
    }
    return this;
}
PopupMenu* PopupMenu::Submenu(Str label, PopupMenu* menu) {
    MenuItem* it = MenuAdd(this, MenuItemKind::Item);
    if (it) {
        it->label = label;
        it->submenu = menu;
    }
    return this;
}
PopupMenu* PopupMenu::Disabled(bool v) {
    if (n > 0) {
        items[n - 1].disabled = v;
    }
    return this;
}
PopupMenu* PopupMenu::WithSize(UiSize s) {
    size = s;
    return this;
}
PopupMenu* PopupMenu::MinW(float v) {
    minW = v;
    return this;
}
PopupMenu* PopupMenu::CheckSide(Side s) {
    checkSide = s;
    return this;
}

void PopupMenu::Masks(bool* clickable, bool* hasSubmenu) const {
    for (int i = 0; i < n; i++) {
        // is_clickable(): a separator and a label are stepped over, and so is
        // a disabled row.
        clickable[i] = items[i].kind == MenuItemKind::Item && !items[i]
                                                                   .disabled;
        hasSubmenu[i] = items[i].submenu != nullptr;
    }
}

El* PopupMenu::IntoEl() {
    const Theme& th = cx->theme();
    PopupMenuState* s = state.Get(cx);
    int selected = s ? s->selected : -1;
    int openSubmenu = s ? s->openSubmenu : -1;
    // has_left_icon: the gutter exists only if some row needs it, so a menu
    // of plain labels is not indented for nothing.
    bool leftGutter = false;
    for (int i = 0; i < n; i++) {
        if (items[i].icon != IconName::None ||
            (SideIsLeft(checkSide) && items[i].checked)) {
            leftGutter = true;
        }
    }
    float itemH = size == UiSize::Small ? 20.f : 26.f;
    float radius = size == UiSize::Small ? th.radius * 0.5f : th.radius;

    // The menu is placed out of flow, so it has no parent width to fill: its
    // own is the width Rust's min_w asks for.
    El* root = Div(a)
                   ->FlexCol()
                   ->W(minW)
                   ->Pad(4)
                   ->Bg(th.background)
                   ->Border(1, th.border)
                   ->Radius(th.radius);
    Listener click = ListenTo(state, &PopupMenuState::OnItemClick, 0);
    Listener hover = ListenTo(state, &PopupMenuState::OnItemHover, 0);
    for (int i = 0; i < n; i++) {
        const MenuItem& it = items[i];
        if (it.kind == MenuItemKind::Separator) {
            // my_0p5 border_b(2): a rule with a little air around it.
            root->Child(Div(a)->W(kFill)->PadY(2)->Child(
                Div(a)->W(kFill)->H(1)->Bg(th.border)));
            continue;
        }
        bool lit =
            selected == i && !it.disabled && it.kind == MenuItemKind::Item;
        El* row = Div(a)
                      ->FlexRow()
                      ->W(kFill)
                      ->MinH(itemH)
                      ->PadX(8)
                      ->Gap(4)
                      ->ItemsCenter()
                      ->JustifyBetween()
                      ->Radius(radius);
        if (lit) {
            row->Bg(th.accent);
        }
        Rgba fg = it.disabled ? th.mutedFg : th.foreground;
        El* left = Div(a)->FlexRow()->Grow()->Gap(4)->ItemsCenter();
        if (leftGutter) {
            // The gutter is the icon's, or the check's, or empty — but it is
            // always the same width, so the labels line up.
            if (it.icon != IconName::None) {
                left->Child(IconEl(a, it.icon, 14)->Fg(fg));
            } else if (SideIsLeft(checkSide) && it.checked) {
                left->Child(IconEl(a, IconName::Check, 14)->Fg(fg));
            } else {
                left->Child(Div(a)->W(14)->H(14)->Shrink0());
            }
        }
        if (it.element) {
            left->Child(it.element);
        } else {
            left->Child(TextEl(a, it.label)->Font(14)->Fg(fg));
        }
        row->Child(left);
        if (it.kbd.s) {
            row->Child(Kbd::New(cx, it.kbd)->IntoEl());
        }
        if (!SideIsLeft(checkSide) && it.checked) {
            row->Child(IconEl(a, IconName::Check, 14)->Fg(fg));
        }
        if (it.submenu) {
            row->Child(IconEl(a, IconName::ChevronRight, 14)->Fg(fg));
        }
        if (it.kind == MenuItemKind::Item && !it.disabled) {
            BindClick(row, StrDup(a, fmt("%s-%d", id, i)),
                      ListenerArg(click, i));
            row->OnHover(ListenerArg(hover, i));
        }
        if (it.submenu && openSubmenu == i) {
            // The submenu hangs off the row it belongs to, on the side the
            // menu opens towards.
            El* sub = it.submenu->IntoEl();
            sub->Absolute()->Top(-4);
            if (s && SideIsLeft(s->side)) {
                sub->Right(minW - 8);
            } else {
                sub->Left(minW - 8);
            }
            sub->Deferred();
            row->Child(sub);
        }
        root->Child(row);
    }
    return root;
}

DropdownMenu* DropdownMenu::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    DropdownMenu* d = ArenaNew<DropdownMenu>(a);
    d->a = a;
    d->cx = cx;
    d->id = id;
    return d;
}
DropdownMenu* DropdownMenu::Trigger(El* e) {
    trigger = e;
    return this;
}
DropdownMenu* DropdownMenu::Menu(PopupMenu* m) {
    menu = m;
    return this;
}
DropdownMenu* DropdownMenu::AnchorRight(bool v) {
    anchorRight = v;
    return this;
}

El* DropdownMenu::IntoEl() {
    El* wrap = Div(a)->FlexCol();
    PopupMenuState* st = menu ? menu->state.Get(cx) : nullptr;
    if (trigger) {
        // The trigger opens and closes the menu it holds; a caller that wants
        // to know can subscribe to the menu itself.
        if (st) {
            BindClick(trigger, id,
                      ListenTo(menu->state, &PopupMenuState::OnTriggerClick));
        }
        wrap->Child(trigger);
    }
    if (menu && st && st->open) {
        El* el = menu->IntoEl()->AnchorBelow(gap)->Deferred();
        if (anchorRight) {
            el->Right(0);
        } else {
            el->Left(0);
        }
        wrap->Child(el);
    }
    return wrap;
}

ContextMenu* ContextMenu::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    ContextMenu* c = ArenaNew<ContextMenu>(a);
    c->a = a;
    c->cx = cx;
    c->id = id;
    return c;
}
ContextMenu* ContextMenu::Child(El* e) {
    child = e;
    return this;
}
ContextMenu* ContextMenu::Menu(PopupMenu* m) {
    menu = m;
    return this;
}

El* ContextMenu::IntoEl() {
    El* box = child ? child : Div(a);
    PopupMenuState* st = menu ? menu->state.Get(cx) : nullptr;
    if (!st) {
        return box;
    }
    // The element needs identity for the press to reach it.
    box->Id(id)
        ->Click(HashClickId(id))
        ->OnMouseDown(ListenTo(menu->state, &PopupMenuState::OnContextDown));
    if (st->open) {
        box->Child(
            menu->IntoEl()->Absolute()->Left(st->x)->Top(st->y)->Deferred());
    }
    return box;
}

int AppMenuBarNextIndex(int selected, int count) {
    if (count <= 0 || selected < 0) {
        return selected;
    }
    return selected + 1 >= count ? 0 : selected + 1;
}

int AppMenuBarPrevIndex(int selected, int count) {
    if (count <= 0 || selected < 0) {
        return selected;
    }
    return selected == 0 ? count - 1 : selected - 1;
}

void AppMenuBarSelect(AppMenuBarState* s, Ctx* cx, int ix) {
    if (!s) {
        return;
    }
    s->selected = ix;
    Notify(cx);
}

void AppMenuBarState::OnMenuClick(AppMenuBarState* self, Ctx* cx,
                                  const ClickEvent*, intptr_t ix) {
    // A second click on the open menu closes it.
    AppMenuBarSelect(self, cx, self->selected == (int)ix ? -1 : (int)ix);
}

void AppMenuBarState::OnMenuHover(AppMenuBarState* self, Ctx* cx,
                                  const HoverEvent* ev, intptr_t ix) {
    if (!ev->hovered || self->selected < 0 || self->selected == (int)ix) {
        return;
    }
    AppMenuBarSelect(self, cx, (int)ix);
}

AppMenuBar* AppMenuBar::New(Ctx* cx, Str id, Entity<AppMenuBarState> state) {
    Arena* a = cx->a;
    AppMenuBar* b = ArenaNew<AppMenuBar>(a);
    b->a = a;
    b->cx = cx;
    b->id = id;
    b->state = state;
    return b;
}
AppMenuBar* AppMenuBar::Menu(Str title, PopupMenu* menu) {
    if (n < 12) {
        titles[n] = title;
        menus[n] = menu;
        n++;
    }
    return this;
}

El* AppMenuBar::IntoEl() {
    const Theme& th = cx->theme();
    AppMenuBarState* s = state.Get(cx);
    El* bar = Div(a)->FlexRow()->ItemsCenter()->Gap(2)->H(28)->W(kFill);
    Listener click = ListenTo(state, &AppMenuBarState::OnMenuClick, 0);
    Listener hover = ListenTo(state, &AppMenuBarState::OnMenuHover, 0);
    for (int i = 0; i < n; i++) {
        bool on = s && s->selected == i;
        El* wrap = Div(a)->FlexCol();
        El* item = Div(a)
                       ->FlexRow()
                       ->H(24)
                       ->PadX(8)
                       ->ItemsCenter()
                       ->Radius(th.radius)
                       ->HoverBg(th.secondary);
        if (on) {
            item->Bg(th.secondary);
        }
        item->Child(TextEl(a, titles[i])->Font(14)->Fg(th.foreground));
        BindClick(item, StrDup(a, fmt("%s-%d", id, i)), ListenerArg(click, i));
        item->OnHover(ListenerArg(hover, i));
        wrap->Child(item);
        if (on && menus[i]) {
            // The menu of the open title hangs under it, over everything the
            // frame drew after it.
            wrap->Child(
                menus[i]->IntoEl()->AnchorBelow(2)->Left(0)->Deferred());
        }
        bar->Child(wrap);
    }
    return bar;
}

} // namespace component
} // namespace gpui

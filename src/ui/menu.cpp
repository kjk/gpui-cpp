#include "ui/menu.h"
#include "base/actions.h"
#include "base/focus_trap.h"
#include "ui/kbd.h"

namespace gpui {

namespace component {

Entity<PopupMenuState> PopupMenuStateFor(Ctx* cx, Str id) {
    return KeyedEntity<PopupMenuState>(cx, KeyedName(cx, id));
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
    MenuItem fresh;
    fresh.kind = kind;
    if (!m->items.Append(m->a, fresh)) {
        return nullptr;
    }
    return &m->items[m->items.len - 1];
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
PopupMenu* PopupMenu::Link(Str label, Str href, IconName icon) {
    MenuItem* it = MenuAdd(this, MenuItemKind::Item);
    if (it) {
        it->label = label;
        it->icon = icon;
        it->isLink = true;
        it->href = href;
    }
    return this;
}
PopupMenu* PopupMenu::Separator() {
    // Rust ignores a leading separator and coalesces consecutive ones.
    if (items.len == 0 || items[items.len - 1]
                                  .kind == MenuItemKind::Separator) {
        return this;
    }
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
        PopupMenuState* child = menu ? menu->state.Get(cx) : nullptr;
        if (child) {
            child->parent = state;
        }
    }
    return this;
}
PopupMenu* PopupMenu::Disabled(bool v) {
    if (items.len > 0) {
        items[items.len - 1].disabled = v;
    }
    return this;
}
PopupMenu* PopupMenu::Checked(bool v) {
    if (items.len > 0) {
        items[items.len - 1].checked = v;
    }
    return this;
}
PopupMenu* PopupMenu::Icon(IconName v) {
    if (items.len > 0) {
        items[items.len - 1].icon = v;
    }
    return this;
}
PopupMenu* PopupMenu::ActionContext(const char* ctx) {
    actionContext = ctx;
    return this;
}

PopupMenu* PopupMenu::MenuWithAction(Str label, uint32_t action, intptr_t arg) {
    Menu(label);
    return Action(action, arg);
}

PopupMenu* PopupMenu::Action(uint32_t action, intptr_t arg) {
    if (items.len > 0) {
        MenuItem* it = &items[items.len - 1];
        it->action = action;
        it->actionArg = arg;
    }
    return this;
}

PopupMenu* PopupMenu::Kbd(Str v) {
    if (items.len > 0) {
        items[items.len - 1].kbd = v;
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
PopupMenu* PopupMenu::MaxH(float v) {
    maxH = v;
    return this;
}
PopupMenu* PopupMenu::Scrollable(bool v) {
    scrollable = v;
    return this;
}
PopupMenu* PopupMenu::CheckSide(Side s) {
    checkSide = s;
    return this;
}
PopupMenu* PopupMenu::ExternalLinkIcon(bool v) {
    externalLinkIcon = v;
    return this;
}

static void OnPopupLinkClick(PopupMenuState* state, Ctx* cx, const ClickEvent*,
                             intptr_t hrefPtr) {
    const Str* href = (const Str*)hrefPtr;
    if (href && href->s) {
        OpenUrl(*href);
    }
    // A link has its own handler rather than an action. Dismiss the menu
    // chain without reporting one of the caller's action indexes.
    PopupMenuDismissAll(state, cx);
}

El* PopupMenu::IntoEl() {
    const Theme& th = cx->theme();
    PopupMenuState* s = state.Get(cx);
    int selected = s ? s->selected : -1;
    int openSubmenu = s ? s->openSubmenu : -1;
    // has_left_icon: the gutter exists only if some row needs it, so a menu
    // of plain labels is not indented for nothing.
    bool leftGutter = false;
    for (const MenuItem& it : items) {
        if (it.icon != IconName::None ||
            (SideIsLeft(checkSide) && it.checked)) {
            leftGutter = true;
        }
    }
    // min_w is a floor, not a fixed menu width. Shape the labels that are
    // present and let a context menu or submenu grow up to Rust's default
    // 500px maximum. Custom rows in the story already carry an explicit
    // 250px minimum, so their own content fits within that floor.
    float menuW = minW;
    if (cx->win) {
        for (const MenuItem& it : items) {
            if (!it.label.s || it.kind == MenuItemKind::Separator) {
                continue;
            }
            Size label = MeasureText(&cx->win->paint, it.label, 14, 0);
            // Border + item-list padding + row padding.
            float need = 26 + label.w;
            if (leftGutter) {
                need += 18;
            }
            if (it.kbd.s) {
                Size key = MeasureText(&cx->win->paint, it.kbd, 12, 0);
                need += (key.w + 8 > 20 ? key.w + 8 : 20) + 4;
            }
            if ((!SideIsLeft(checkSide) && it.checked) || it.submenu ||
                (it.isLink && externalLinkIcon)) {
                need += 18;
            }
            if (need > menuW) {
                menuW = need;
            }
        }
    }
    if (menuW > 500) {
        menuW = 500;
    }
    float itemH = size == UiSize::Small ? 20.f : 26.f;
    float radius = size == UiSize::Small ? th.radius * 0.5f : th.radius;

    // The menu is placed out of flow, so it has no parent width to fill: its
    // own is the width Rust's min_w asks for. It does not clip: a submenu
    // hangs off the row it belongs to and reaches past the right edge, and a
    // box that clipped would cut it down to the eight pixels the two overlap
    // by. Rust clips the item list instead, and only when the menu scrolls,
    // which is where the clip is below.
    // `popover_style`: a menu floats over the page, so it takes the one popup
    // surface rather than the window's. Both default themes give them the
    // same colour.
    El* root = PopoverSurface(cx, Div(a)->FlexCol()->W(menuW));
    // The menu's own name, so a row is `("item", ix)` and two menus on one
    // page do not have to be told apart by their rows' spelling.
    root->Id(id);
    // .key_context(CONTEXT).track_focus(&self.focus_handle) and the six
    // on_action handlers behind it. The menu is its own focus trap and the
    // only focusable inside it, so arming the trap while it is the deepest
    // menu on screen is `focus_handle.focus(window)` — and a submenu that
    // opens takes the focus off its parent, which is what makes escape close
    // one level at a time without anything walking the tree.
    PopupMenuInitKeys();
    // The handle the state owns — asked for once and kept, which is what lets
    // dismissal put focus back without the menu having to be found by name.
    // The trap is keyed on the same number, since a trap is the container the
    // focus is held inside.
    if (s && !s->focus.IsValid()) {
        s->focus = FocusHandleNew(cx);
    }
    FocusHandle focus = s ? s->focus : FocusHandle{};
    root->KeyContext(PopupMenuContext())->TrackFocus(focus)->TrapId(focus.id);
    if (s) {
        // .on_mouse_down_out(Self::on_mouse_down_out), and the box it is
        // measured against.
        root->BoundsOut(&s->bounds)
            ->OnMouseUpOut(ListenTo(state, &PopupMenuState::OnPressOutside));
    }
    if (s && s->openSubmenu < 0) {
        FocusTrapArm(cx->win, focus.id);
    }
    Listener onAction = ListenTo(state, &PopupMenuState::OnAction);
    root->OnAction(action::Confirm(), onAction)
        ->OnAction(action::Cancel(), onAction)
        ->OnAction(action::SelectUp(), onAction)
        ->OnAction(action::SelectDown(), onAction)
        ->OnAction(action::SelectLeft(), onAction)
        ->OnAction(action::SelectRight(), onAction);
    // The rows, as the keyboard sees them. Rust's menu owns its items; here
    // they are the caller's, so what an action needs is copied across.
    PopupMenuBeginRows(s);
    for (const MenuItem& it : items) {
        PopupMenuRow row;
        row.clickable = it.kind == MenuItemKind::Item && !it.disabled;
        row.submenu = it.submenu != nullptr;
        row.link = it.isLink;
        row.href = it.href;
        PopupMenuAddRow(s, row);
    }
    El* rows = Div(a)->FlexCol()->W(kFill)->Pad(4)->Gap(2);
    if (scrollable) {
        rows->ClipY()
            ->MaxH(maxH)
            ->ScrollY(s ? s->scrollY : 0)
            ->ScrollId(HashClickId(id))
            ->OnScroll(ListenTo(state, &PopupMenuState::OnScroll));
    }
    Listener click = ListenTo(state, &PopupMenuState::OnItemClick, 0);
    Listener linkClick = ListenTo(state, &OnPopupLinkClick, 0);
    Listener hover = ListenTo(state, &PopupMenuState::OnItemHover, 0);
    Listener submenuClick = ListenTo(state, &PopupMenuState::OnSubmenuClick, 0);
    Listener submenuHover = ListenTo(state, &PopupMenuState::OnSubmenuHover, 0);
    int i = -1;
    for (const MenuItem& it : items) {
        i++;
        if (it.kind == MenuItemKind::Separator) {
            if (i + 1 == items.len) {
                continue;
            }
            // my_0p5 border_b(2): a rule with a little air around it.
            rows->Child(Div(a)->W(kFill)->PadY(2)->Child(
                Div(a)->W(kFill)->H(2)->Bg(th.border)));
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
            row->Bg(th.tokens.accent);
        }
        Rgba fg = it.disabled ? th.mutedFg : th.foreground;
        El* left = Div(a)->FlexRow()->Flex1()->Gap(4)->ItemsCenter();
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
        // What the row shows on the right: the string a caller set, or —
        // for a row that names an action — whatever the keymap has bound to
        // it, which is `Kbd::binding_for_action_in`.
        El* kbdEl = nullptr;
        if (it.kbd.s) {
            kbdEl = Kbd::New(cx, it.kbd)->Appearance(false)->IntoEl();
        } else if (it.action) {
            component::Kbd* k = Kbd::ForAction(cx, it.action, actionContext);
            kbdEl = k ? k->Appearance(false)->IntoEl() : nullptr;
        }
        if (kbdEl) {
            // PopupMenu clears Kbd's background and border while retaining
            // its compact padding and minimum width.
            row->Child(Div(a)
                           ->PadX(4)
                           ->PadY(2)
                           ->MinW(20)
                           ->ItemsCenter()
                           ->JustifyCenter()
                           ->Child(kbdEl));
        }
        if (it.isLink && externalLinkIcon) {
            row->Child(IconEl(a, IconName::ExternalLink, 12)->Fg(th.mutedFg));
        }
        if (!SideIsLeft(checkSide) && it.checked) {
            row->Child(IconEl(a, IconName::Check, 14)->Fg(fg));
        }
        if (it.submenu) {
            row->Child(IconEl(a, IconName::ChevronRight, 14)->Fg(fg));
        }
        if (it.kind == MenuItemKind::Item && !it.disabled) {
            if (it.submenu) {
                BindClick(row, StrDup(a, fmt("%d", i)),
                          ListenerArg(submenuClick, i));
                row->OnHover(ListenerArg(submenuHover, i));
            } else if (it.isLink && it.href.s) {
                BindClick(row, StrDup(a, fmt("%d", i)),
                          ListenerArg(linkClick, (intptr_t)&it.href));
                row->OnHover(ListenerArg(hover, i));
            } else {
                BindClick(row, StrDup(a, fmt("%d", i)), ListenerArg(click, i));
                // `window.dispatch_action(action.boxed_clone(), cx)`, beside
                // the click the menu itself needs to close on.
                if (it.action) {
                    row->OnClickAction(it.action, it.actionArg);
                }
                row->OnHover(ListenerArg(hover, i));
            }
        }
        if (it.submenu && openSubmenu == i) {
            // The submenu hangs off the row it belongs to, on the side the
            // menu opens towards. It is open because this row says so, so it
            // is this menu that escape inside it has to reach — Rust keeps
            // the same link as parent_menu.
            PopupMenuState* subState = it.submenu->state.Get(cx);
            if (subState && s) {
                subState->parent = state;
                subState->open = true;
                subState->side = s->side;
            }
            El* sub = it.submenu->IntoEl();
            sub->Absolute()->Top(-4);
            if (s && SideIsLeft(s->side)) {
                sub->Right(menuW - 8);
            } else {
                sub->Left(menuW - 8);
            }
            sub->Deferred();
            row->Child(sub);
        }
        rows->Child(row);
    }
    root->Child(rows);
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
    // The trigger and the open transition are named inside the dropdown, so
    // two dropdowns with the same local name are still two dropdowns.
    IdScope scope(cx, id);
    El* wrap = Div(a)->Id(id)->FlexCol();
    PopupMenuState* st = menu ? menu->state.Get(cx) : nullptr;
    if (trigger) {
        // The trigger opens and closes the menu it holds; a caller that wants
        // to know can subscribe to the menu itself.
        if (st) {
            // The menu as this frame has it goes with the handler, the way
            // Rust's Popover captures `open` at render time and hands it to
            // the trigger's press.
            BindClick(trigger, StrL("trigger"),
                      ListenTo(menu->state, &PopupMenuState::OnTriggerClick,
                               (intptr_t)st->open));
        }
        wrap->Child(trigger);
    }
    if (menu && st && st->open) {
        // `dropdown_positioner`: side placement, so a menu with no room
        // under its trigger opens above it rather than being clamped.
        El* el = DropdownOpen(cx, menu->IntoEl(), MotionName(cx, StrL("open")))
                     ->AnchorBelow(gap)
                     ->AnchorFlip()
                     ->Deferred();
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
    box->PathClick(id)
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
                       ->HoverBg(th.tokens.secondary);
        if (on) {
            item->Bg(th.tokens.secondary);
        }
        item->Child(TextEl(a, titles[i])->Font(14)->Fg(th.foreground));
        BindClick(item, StrDup(a, fmt("%s-%d", id, i)), ListenerArg(click, i));
        item->OnHover(ListenerArg(hover, i));
        wrap->Child(item);
        if (on && menus[i]) {
            // The menu of the open title hangs under it, over everything the
            // frame drew after it.
            wrap->Child(menus[i]
                            ->IntoEl()
                            ->AnchorBelow(2)
                            ->AnchorFlip()
                            ->Left(0)
                            ->Deferred());
        }
        bar->Child(wrap);
    }
    return bar;
}

} // namespace component
} // namespace gpui

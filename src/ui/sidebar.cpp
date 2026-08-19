#include "ui/sidebar.h"
#include "ui/button.h"

namespace gpui {

namespace component {

// DEFAULT_WIDTH is the caller's; COLLAPSED_WIDTH is not.
static const float kCollapsedWidth = 48;

void SidebarMenuState::OnItemClick(SidebarMenuState* self, Ctx* cx,
                                   const ClickEvent* ev) {
    // click_to_open opens and leaves it open; click_to_toggle flips it. The
    // caller's handler runs either way, which is what Rust's closure does
    // after it has dealt with the submenu.
    if (self->clickToOpen) {
        self->open = true;
    } else if (self->clickToToggle) {
        self->open = !self->open;
    }
    Notify(cx);
    if (self->onClick.IsValid()) {
        ListenerCall(cx->app, cx->win, self->onClick, ev);
    }
}

void SidebarMenuState::OnCaretClick(SidebarMenuState* self, Ctx* cx,
                                    const ClickEvent*) {
    // stop_propagation: the caret expands the submenu and is not a click on
    // the item. Here the caret is the innermost hit rect, so the item never
    // hears it in the first place.
    self->open = !self->open;
    Notify(cx);
}

SidebarMenuItem* SidebarMenuItem::New(Ctx* cx, Str label) {
    Arena* a = cx->a;
    SidebarMenuItem* it = ArenaNew<SidebarMenuItem>(a);
    it->a = a;
    it->cx = cx;
    it->label = label;
    return it;
}
SidebarMenuItem* SidebarMenuItem::Icon(IconName v) {
    icon = v;
    return this;
}
SidebarMenuItem* SidebarMenuItem::Active(bool v) {
    active = v;
    return this;
}
SidebarMenuItem* SidebarMenuItem::Disabled(bool v) {
    disabled = v;
    return this;
}
SidebarMenuItem* SidebarMenuItem::DefaultOpen(bool v) {
    defaultOpen = v;
    return this;
}
SidebarMenuItem* SidebarMenuItem::ClickToOpen(bool v) {
    clickToOpen = v;
    return this;
}
SidebarMenuItem* SidebarMenuItem::ClickToToggle(bool v) {
    clickToToggle = v;
    return this;
}
SidebarMenuItem* SidebarMenuItem::Suffix(El* e) {
    suffix = e;
    return this;
}
SidebarMenuItem* SidebarMenuItem::Child(SidebarMenuItem* item) {
    if (nChildren < 16 && item) {
        children[nChildren++] = item;
    }
    return this;
}
SidebarMenuItem* SidebarMenuItem::OnClick(Listener fn) {
    onClick = fn;
    return this;
}

El* SidebarMenuItem::IntoEl(Str id) {
    const Theme& th = cx->theme();
    bool isSubmenu = nChildren > 0;
    Entity<SidebarMenuState> st = {};
    bool isOpen = false;
    if (isSubmenu) {
        st = KeyedEntity<SidebarMenuState>(cx, (uint32_t)HashClickId(id));
        SidebarMenuState* s = st.Get(cx);
        if (s) {
            if (!s->seeded) {
                s->seeded = true;
                s->open = defaultOpen;
            }
            s->clickToOpen = clickToOpen;
            s->clickToToggle = clickToToggle;
            s->onClick = onClick;
            isOpen = !collapsed && s->open;
        }
    }

    El* root = Div(a)->FlexCol()->W(kFill);
    El* row = Div(a)
                  ->FlexRow()
                  ->W(kFill)
                  ->Shrink0()
                  ->Pad(8)
                  ->Gap(8)
                  ->ItemsCenter()
                  ->Radius(th.radius)
                  ->Font(14);
    bool hoverable = !active && !disabled;
    if (hoverable) {
        row->HoverBg(th.sidebarAccent)->HoverFg(th.sidebarAccentFg);
    }
    if (active) {
        row->Bg(th.sidebarAccent)->Fg(th.sidebarAccentFg);
    }
    Rgba fg =
        disabled ? th.mutedFg : (active ? th.sidebarAccentFg : th.sidebarFg);
    if (icon != IconName::None) {
        row->Child(IconEl(a, icon, 16)->Fg(fg));
    }
    if (collapsed) {
        row->JustifyCenter();
        // The label has nowhere to go, so it becomes the tooltip. Rust places
        // it to the right of the item; a tip here sits where the window puts
        // it.
        row->Tip(label);
    } else {
        row->H(28);
        El* mid = Div(a)->FlexRow()->Grow()->Gap(8)->JustifyBetween();
        mid->Child(Div(a)->FlexRow()->Grow()->Child(
            TextEl(a, label)->Font(14)->Fg(fg)));
        if (suffix) {
            mid->Child(suffix);
        }
        row->Child(mid);
        if (isSubmenu) {
            // The caret is its own button: it opens the submenu without
            // being a click on the item.
            row->Child(
                Button::New(cx, StrDup(a, fmt("%s-caret", id)))
                    ->Icon(isOpen ? IconName::ChevronDown
                                  : IconName::ChevronRight)
                    ->Ghost()
                    ->WithSize(UiSize::XSmall)
                    ->OnClick(ListenTo(st, &SidebarMenuState::OnCaretClick))
                    ->IntoEl());
        }
    }
    if (!disabled) {
        // A submenu item's click goes through the state, which applies the
        // open rules before handing over to the caller.
        Listener l =
            isSubmenu ? ListenTo(st, &SidebarMenuState::OnItemClick) : onClick;
        BindClick(row, id, l);
    }
    root->Child(row);

    if (isOpen) {
        El* sub = Div(a)->FlexCol()->Gap(4)->PadY(2)->PadL(10)->BorderL(
            1, th.sidebarBorder);
        for (int i = 0; i < nChildren; i++) {
            children[i]->collapsed = collapsed;
            sub->Child(children[i]->IntoEl(StrDup(a, fmt("%s-%d", id, i))));
        }
        root->Child(sub);
    }
    return root;
}

SidebarMenu* SidebarMenu::New(Ctx* cx) {
    Arena* a = cx->a;
    SidebarMenu* m = ArenaNew<SidebarMenu>(a);
    m->a = a;
    m->cx = cx;
    return m;
}
SidebarMenu* SidebarMenu::Child(SidebarMenuItem* item) {
    if (n < 24 && item) {
        items[n++] = item;
    }
    return this;
}

El* SidebarMenu::IntoEl(Str id) {
    El* col = Div(a)->FlexCol()->W(kFill)->Gap(8);
    for (int i = 0; i < n; i++) {
        items[i]->collapsed = collapsed;
        col->Child(items[i]->IntoEl(StrDup(a, fmt("%s-%d", id, i))));
    }
    return col;
}

SidebarGroup* SidebarGroup::New(Ctx* cx, Str label) {
    Arena* a = cx->a;
    SidebarGroup* g = ArenaNew<SidebarGroup>(a);
    g->a = a;
    g->cx = cx;
    g->label = label;
    return g;
}
SidebarGroup* SidebarGroup::Child(SidebarMenu* menu) {
    if (n < 8 && menu) {
        menus[n++] = menu;
    }
    return this;
}

El* SidebarGroup::IntoEl(Str id) {
    const Theme& th = cx->theme();
    El* col = Div(a)->FlexCol()->W(kFill);
    if (!collapsed && label.s) {
        col->Child(Div(a)
                       ->FlexRow()
                       ->Shrink0()
                       ->H(32)
                       ->PadX(8)
                       ->ItemsCenter()
                       ->Radius(th.radius)
                       ->Child(TextEl(a, label)->Font(12)->Fg(
                           RgbaOpacity(th.sidebarFg, 0.7f))));
    }
    El* inner = Div(a)->FlexCol()->W(kFill)->Gap(8);
    for (int i = 0; i < n; i++) {
        menus[i]->collapsed = collapsed;
        inner->Child(menus[i]->IntoEl(StrDup(a, fmt("%s-%d", id, i))));
    }
    col->Child(inner);
    return col;
}

static El* SidebarBand(Ctx* cx, El* child, bool selected, Listener onClick,
                       Str id) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* row = Div(a)
                  ->FlexRow()
                  ->W(kFill)
                  ->Gap(8)
                  ->Pad(8)
                  ->ItemsCenter()
                  ->JustifyBetween()
                  ->Radius(th.radius)
                  ->HoverBg(th.sidebarAccent)
                  ->HoverFg(th.sidebarAccentFg);
    if (selected) {
        row->Bg(th.sidebarAccent)->Fg(th.sidebarAccentFg);
    }
    if (child) {
        row->Child(child);
    }
    if (onClick.IsValid()) {
        BindClick(row, id, onClick);
    }
    return row;
}

El* SidebarHeader(Ctx* cx, El* child, bool selected, Listener onClick) {
    return SidebarBand(cx, child, selected, onClick, StrL("sidebar-header"));
}

El* SidebarFooter(Ctx* cx, El* child, bool selected, Listener onClick) {
    return SidebarBand(cx, child, selected, onClick, StrL("sidebar-footer"));
}

SidebarToggleButton* SidebarToggleButton::New(Ctx* cx) {
    Arena* a = cx->a;
    SidebarToggleButton* b = ArenaNew<SidebarToggleButton>(a);
    b->a = a;
    b->cx = cx;
    return b;
}
SidebarToggleButton* SidebarToggleButton::Collapsed(bool v) {
    collapsed = v;
    return this;
}
SidebarToggleButton* SidebarToggleButton::WithSide(Side v) {
    side = v;
    return this;
}
SidebarToggleButton* SidebarToggleButton::OnClick(Listener fn) {
    onClick = fn;
    return this;
}

El* SidebarToggleButton::IntoEl() {
    IconName icon;
    if (collapsed) {
        icon = SideIsLeft(side) ? IconName::PanelLeftOpen
                                : IconName::PanelRightOpen;
    } else {
        icon = SideIsLeft(side) ? IconName::PanelLeftClose
                                : IconName::PanelRightClose;
    }
    return Button::New(cx, StrL("collapse"))
        ->Icon(icon)
        ->Ghost()
        ->WithSize(UiSize::Small)
        ->OnClick(onClick)
        ->IntoEl();
}

Sidebar* Sidebar::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    Sidebar* s = ArenaNew<Sidebar>(a);
    s->a = a;
    s->cx = cx;
    s->id = id;
    return s;
}
Sidebar* Sidebar::WithSide(Side v) {
    side = v;
    return this;
}
Sidebar* Sidebar::Collapsible(SidebarCollapsible v) {
    collapsible = v;
    return this;
}
Sidebar* Sidebar::Collapsed(bool v) {
    collapsed = v;
    return this;
}
Sidebar* Sidebar::Header(El* e) {
    header = e;
    return this;
}
Sidebar* Sidebar::Footer(El* e) {
    footer = e;
    return this;
}
Sidebar* Sidebar::Child(SidebarGroup* group) {
    if (n < 8 && group) {
        groups[n++] = group;
    }
    return this;
}
Sidebar* Sidebar::W(float px) {
    width = px;
    return this;
}

El* Sidebar::IntoEl() {
    const Theme& th = cx->theme();
    // SidebarLayout::new: a collapsible of None ignores the flag, Icon
    // narrows to the icon width, Offcanvas takes the whole thing out of the
    // layout. Rust animates the width between the two; there is no animation
    // here, so it snaps.
    bool isCollapsed = collapsed && collapsible != SidebarCollapsible::None;
    bool iconCollapsed = isCollapsed && collapsible == SidebarCollapsible::Icon;
    bool offcanvas =
        isCollapsed && collapsible == SidebarCollapsible::Offcanvas;
    if (offcanvas) {
        return Div(a)->W(0)->H(kFill)->Shrink0();
    }

    El* root = Div(a)
                   ->FlexCol()
                   ->Shrink0()
                   ->W(iconCollapsed ? kCollapsedWidth : width)
                   ->H(kFill)
                   ->Bg(th.sidebar)
                   ->Fg(th.sidebarFg);
    if (SideIsLeft(side)) {
        root->BorderR(1, th.sidebarBorder);
    } else {
        root->BorderL(1, th.sidebarBorder);
    }
    if (iconCollapsed) {
        root->Gap(8);
    }
    if (header) {
        El* box = Div(a)->FlexRow()->W(kFill)->Gap(8);
        if (iconCollapsed) {
            box->PadT(8)->PadX(8);
        } else {
            box->PadT(12)->PadX(12);
        }
        box->Child(header);
        root->Child(box);
    }
    El* content = Div(a)->FlexCol()->W(kFill)->Grow();
    El* inner = Div(a)->FlexCol()->W(kFill)->Gap(12);
    if (iconCollapsed) {
        inner->Pad(8);
    } else {
        inner->PadX(12)->PadY(12);
    }
    for (int i = 0; i < n; i++) {
        groups[i]->collapsed = iconCollapsed;
        inner->Child(groups[i]->IntoEl(StrDup(a, fmt("%s-%d", id, i))));
    }
    content->Child(inner);
    root->Child(content);
    if (footer) {
        El* box = Div(a)->FlexRow()->W(kFill)->PadX(iconCollapsed ? 8.f : 12.f);
        box->PadB(12);
        box->Child(footer);
        root->Child(box);
    }
    return root;
}

} // namespace component
} // namespace gpui

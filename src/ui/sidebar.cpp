#include "ui/sidebar.h"
#include "base/motion.h"
#include "ui/button.h"

namespace gpui {

namespace component {

// sidebar/mod.rs: SIDEBAR_TRANSITION_DURATION.
static const float kSidebarMotionMs = 200.f;

// DEFAULT_WIDTH is the caller's; COLLAPSED_WIDTH is not.

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
    if (item) {
        children.Append(a, item);
    }
    return this;
}
SidebarMenuItem* SidebarMenuItem::OnClick(Listener fn) {
    onClick = fn;
    return this;
}

El* SidebarMenuItem::IntoEl(Str id) {
    const Theme& th = cx->theme();
    bool isSubmenu = children.len > 0;
    Entity<SidebarMenuState> st = {};
    bool isOpen = false;
    if (isSubmenu) {
        st = KeyedEntity<SidebarMenuState>(cx, KeyedName(cx, id));
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

    // The item names itself, so the row, its caret, the context menu over it
    // and every child under it are named by their place in the item.
    IdScope scope(cx, id);
    El* root = Div(a)->Id(id)->FlexCol()->W(kFill);
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
        row->HoverBg(th.tokens.sidebarAccent)->HoverFg(th.sidebarAccentFg);
    }
    if (active) {
        row->Bg(th.tokens.sidebarAccent)->Fg(th.sidebarAccentFg);
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
        El* mid = Div(a)->FlexRow()->Flex1()->Gap(8)->JustifyBetween();
        mid->Child(Div(a)->FlexRow()->Flex1()->Child(
            TextEl(a, label)->Font(14)->Fg(fg)));
        if (suffix) {
            mid->Child(suffix);
        }
        row->Child(mid);
        if (isSubmenu) {
            // The caret is its own button: it opens the submenu without
            // being a click on the item.
            row->Child(
                Button::New(cx, StrL("caret"))
                    ->Icon(isOpen ? IconName::ChevronDown
                                  : IconName::ChevronRight)
                    ->Ghost()
                    ->WithSize(UiSize::XSmall)
                    ->OnClick(ListenTo(st, &SidebarMenuState::OnCaretClick))
                    ->IntoEl()
                    // "without being a click on the item" is what
                    // stop_propagation says now that a click bubbles.
                    ->StopClick());
        }
    }
    if (!disabled) {
        // A submenu item's click goes through the state, which applies the
        // open rules before handing over to the caller.
        Listener l =
            isSubmenu ? ListenTo(st, &SidebarMenuState::OnItemClick) : onClick;
        BindClick(row, StrL("row"), l);
    }
    // context_menu(..): a right press on the row opens the caller's menu
    // where the pointer is.
    if (contextMenu) {
        row = ContextMenu::New(cx, StrL("ctx"))
                  ->Child(row)
                  ->Menu(contextMenu)
                  ->IntoEl();
    }
    root->Child(row);

    if (isOpen) {
        El* sub = Div(a)->FlexCol()->Gap(4)->PadY(2)->PadL(10)->BorderL(
            1, th.sidebarBorder);
        for (int i = 0; i < children.len; i++) {
            children[i]->collapsed = collapsed;
            sub->Child(children[i]->IntoEl(StrDup(a, fmt("%d", i))));
        }
        // ml_3p5: the rule down the submenu sits in from the parent's edge,
        // under the icon column rather than beside it.
        root->Child(Div(a)->PadL(14)->W(kFill)->Child(sub));
    }
    return root;
}

SidebarMenuItem* SidebarMenuItem::ContextMenu(PopupMenu* menu) {
    contextMenu = menu;
    return this;
}

SidebarMenu* SidebarMenu::New(Ctx* cx) {
    Arena* a = cx->a;
    SidebarMenu* m = ArenaNew<SidebarMenu>(a);
    m->a = a;
    m->cx = cx;
    return m;
}
SidebarMenu* SidebarMenu::Child(SidebarMenuItem* item) {
    if (item) {
        items.Append(a, item);
    }
    return this;
}

El* SidebarMenu::IntoEl(Str id) {
    IdScope scope(cx, id);
    El* col = Div(a)->Id(id)->FlexCol()->W(kFill)->Gap(8);
    for (int i = 0; i < items.len; i++) {
        items[i]->collapsed = collapsed;
        col->Child(items[i]->IntoEl(StrDup(a, fmt("%d", i))));
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
    if (menu) {
        menus.Append(a, menu);
    }
    return this;
}

El* SidebarGroup::IntoEl(Str id) {
    const Theme& th = cx->theme();
    IdScope scope(cx, id);
    El* col = Div(a)->Id(id)->FlexCol()->W(kFill);
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
    for (int i = 0; i < menus.len; i++) {
        menus[i]->collapsed = collapsed;
        inner->Child(menus[i]->IntoEl(StrDup(a, fmt("%d", i))));
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
                  ->HoverBg(th.tokens.sidebarAccent)
                  ->HoverFg(th.sidebarAccentFg);
    if (selected) {
        row->Bg(th.tokens.sidebarAccent)->Fg(th.sidebarAccentFg);
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
    if (group) {
        groups.Append(a, group);
    }
    return this;
}
Sidebar* Sidebar::W(float px) {
    width = px;
    return this;
}

SidebarLayout SidebarLayoutFor(SidebarCollapsible collapsible, bool collapsed,
                               float expandedWidth, Side side) {
    SidebarLayout out;
    // A collapsible of None ignores the flag entirely.
    bool isCollapsed = collapsed && collapsible != SidebarCollapsible::None;
    bool hasWidth = expandedWidth > 0;
    switch (collapsible) {
        case SidebarCollapsible::None:
            break;
        case SidebarCollapsible::Icon:
            if (hasWidth) {
                out.wrapper = SidebarWrapperKind::Animated;
                out.wrapperWidth =
                    isCollapsed ? kSidebarCollapsedWidth : expandedWidth;
            }
            break;
        case SidebarCollapsible::Offcanvas:
            if (hasWidth) {
                out.wrapper = SidebarWrapperKind::Animated;
                out.wrapperWidth = isCollapsed ? 0.f : expandedWidth;
            } else if (isCollapsed) {
                out.wrapper = SidebarWrapperKind::Static;
                out.wrapperWidth = 0;
            }
            break;
    }
    // Offcanvas on the left and everything else on the right: the side the
    // content is pinned to while the width changes under it.
    out.alignChildToEnd = collapsible == SidebarCollapsible::Offcanvas
                              ? SideIsLeft(side)
                              : !SideIsLeft(side);
    out.iconCollapsed = isCollapsed && collapsible == SidebarCollapsible::Icon;
    out.offcanvasCollapsed =
        isCollapsed && collapsible == SidebarCollapsible::Offcanvas;
    return out;
}

El* Sidebar::IntoEl() {
    const Theme& th = cx->theme();
    // SidebarLayout::new says what the mode and the flag come to: the width
    // the wrapper takes, which rendering the rows use, and which end the
    // content is pinned to.
    SidebarLayout layout =
        SidebarLayoutFor(collapsible, collapsed, width, side);
    bool iconCollapsed = layout.iconCollapsed;
    // EffectTransition::width over SIDEBAR_TRANSITION_DURATION: the box around
    // the sidebar takes the width and clips, while the sidebar inside keeps
    // its own — which is what slides the content out of view rather than
    // squeezing it. The end the content is pinned to is what decides which way
    // it goes.
    float target = layout.wrapper == SidebarWrapperKind::None
                       ? width
                       : layout.wrapperWidth;
    Motion motion = MotionNew(kSidebarMotionMs);
    motion.ease = EaseInOutCubic;
    float wrapW =
        MotionValue(cx, MotionId(id, StrL("sidebar-width")), target, motion);
    // render_child: the sidebar is still built while it is on its way out, and
    // only dropped once there is no room left to show it in.
    if (layout.offcanvasCollapsed && wrapW <= 0.5f) {
        return Div(a)->W(0)->H(kFill)->Shrink0();
    }
    // The sidebar's own width is the one it is heading for, so its rows are
    // laid out at their final size while the wrapper reveals them.
    float natural = layout.wrapper == SidebarWrapperKind::None
                        ? width
                        : (iconCollapsed ? kSidebarCollapsedWidth : width);

    // The sidebar names itself, and the groups under it are named by their
    // place in it.
    IdScope scope(cx, id);
    El* root = Div(a)
                   ->Id(id)
                   ->FlexCol()
                   ->Shrink0()
                   ->W(natural)
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
    El* content = Div(a)->FlexCol()->W(kFill)->Flex1();
    El* inner = Div(a)->FlexCol()->W(kFill);
    if (iconCollapsed) {
        inner->Pad(8);
    } else {
        inner->PadX(12);
    }
    for (int i = 0; i < groups.len; i++) {
        groups[i]->collapsed = iconCollapsed;
        // The groups are rows of a `list(..)` in Rust, which has no gap of
        // its own: `pt_3` on the first and `pb_3` on the last are the whole
        // of the spacing around them, and two groups touch. `inner`'s
        // `gap_y_3` never applies, since the list is its only child.
        El* box = Div(a)->FlexCol()->W(kFill)->Child(
            groups[i]->IntoEl(StrDup(a, fmt("%d", i))));
        if (i == 0) {
            box->PadT(12);
        }
        if (i + 1 == groups.len) {
            box->PadB(12);
        }
        inner->Child(box);
    }
    content->Child(inner);
    root->Child(content);
    if (footer) {
        El* box = Div(a)->FlexRow()->W(kFill)->PadX(iconCollapsed ? 8.f : 12.f);
        box->PadB(12);
        box->Child(footer);
        root->Child(box);
    }
    // sidebar_wrapper: a clipping box of the animated width, with the sidebar
    // pinned to whichever end it slides from. At rest it is exactly the
    // sidebar's own width, so nothing moves that was not moving anyway.
    El* wrapper = Div(a)->FlexRow()->W(wrapW)->H(kFill)->Shrink0()->ClipX();
    if (layout.alignChildToEnd) {
        wrapper->JustifyEnd();
    }
    return wrapper->Child(root);
}

} // namespace component
} // namespace gpui

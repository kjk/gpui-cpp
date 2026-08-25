#include "ui/i18n.h"
#include "ui/dock.h"
#include "ui/menu.h"

namespace gpui {

namespace component {

El* DockInvalidPanelRender(Ctx* cx, void* data) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    auto* info = (DockInvalidPanel*)data;
    Str name = info ? info->name : StrL("");
    // my_6, centred, muted: Rust's sentence, with the name it was asked for.
    return Div(a)
        ->SizeFull()
        ->PadY(24)
        ->FlexCol()
        ->ItemsCenter()
        ->JustifyCenter()
        ->Child(TextEl(a, StrDup(a, fmt("The `%s` panel type is not "
                                        "registered in PanelRegistry.",
                                        name)))
                    ->Fg(th.mutedFg));
}

DockArea* DockArea::New(Ctx* cx, Str id, Entity<DockState> state) {
    Arena* a = cx->a;
    DockArea* d = ArenaNew<DockArea>(a);
    d->a = a;
    d->cx = cx;
    d->id = id;
    d->state = state;
    return d;
}

// ---------------------------------------------------------------------------
// The theme's skin.
//
// Everything below is one implementation of base's `DockRenderer` — the
// `crates/ui` half of upstream's split, where `crates/base` owns the tree,
// the drag, the drop and the resize and has no opinion at all about how any
// of it looks. The base showcase is the other implementation.

// The three toggle buttons Rust hangs off the tab panel it picked for each
// edge (DockArea::toggle_button_panels).
static El* ToggleButton(const DockTabGroup* g, DockPlacement p, IconName icon) {
    Ctx* cx = g->cx;
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    return DockBindToggle(g, p,
                          Div(a)
                              ->Pad(4)
                              ->Radius(th.radius * 0.5f)
                              ->HoverBg(th.tokens.secondary)
                              ->Child(IconEl(a, icon, 14)->Fg(th.mutedFg)));
}

// The leading pair — left and bottom — or the trailing one, right.
static El* RenderToggles(const DockTabGroup* g, bool trailing) {
    Arena* a = g->cx->a;
    DockState* s = g->state.Get(g->cx);
    El* row = Div(a)->FlexRow()->ItemsCenter()->Shrink0()->Gap(4);
    if (trailing) {
        if (DockGroupHasToggle(g, DockPlacement::Right)) {
            row->Child(ToggleButton(g, DockPlacement::Right,
                                    s->right.open ? IconName::PanelRight
                                                  : IconName::PanelRightOpen));
        }
        return row;
    }
    if (DockGroupHasToggle(g, DockPlacement::Left)) {
        row->Child(ToggleButton(
            g, DockPlacement::Left,
            s->left.open ? IconName::PanelLeft : IconName::PanelLeftOpen));
    }
    if (DockGroupHasToggle(g, DockPlacement::Bottom)) {
        row->Child(ToggleButton(g, DockPlacement::Bottom,
                                s->bottom.open ? IconName::PanelBottom
                                               : IconName::PanelBottomOpen));
    }
    return row;
}

static bool HasLeadingToggles(const DockTabGroup* g) {
    return DockGroupHasToggle(g, DockPlacement::Left) ||
           DockGroupHasToggle(g, DockPlacement::Bottom);
}

// TabPanel::render_toolbar: the panel's zoom button — only where its
// PanelControl says the toolbar is one of the places it shows — and the menu
// beside it. A collapsed group draws neither.
static El* RenderTools(const DockTabGroup* g) {
    Ctx* cx = g->cx;
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    DockState* s = g->state.Get(cx);
    El* row = Div(a)->FlexRow()->ItemsCenter()->Shrink0()->Gap(4);
    int activeIx = DockGroupActiveIx(g);
    if (g->collapsed || activeIx < 0) {
        return row;
    }
    int panelIx = s->nodes[g->node].panel[activeIx];
    const DockPanelDef& def = s->panels[panelIx];
    bool zoomed = s->zoomPanel == panelIx;
    // `zoomable_toolbar_visible`: a zoomed panel always shows the way back
    // out, and Zoom In is on the bar only for Toolbar and Both.
    if (zoomed || DockPanelControlToolbar(def.zoomable)) {
        row->Child(DockBindZoom(
            g, panelIx,
            Div(a)
                ->Pad(4)
                ->Radius(th.radius * 0.5f)
                ->HoverBg(th.tokens.secondary)
                ->Child(IconEl(a,
                               zoomed ? IconName::Minimize : IconName::Maximize,
                               14)
                            ->Fg(th.mutedFg))));
    }
    // The menu button: the same two actions, where a narrow bar can still
    // reach them.
    Str menuId = StrDup(a, fmt("%s-menu-%d", g->id, g->node));
    component::PopupMenu* menu = component::PopupMenu::New(cx, menuId);
    menu->Menu(zoomed ? Tr("Dock.Zoom Out") : Tr("Dock.Zoom In"));
    if (!DockPanelControlMenu(def.zoomable) && !zoomed) {
        menu->Disabled(true);
    }
    // `closable`: the last panel of a dock has no Close, so a dock cannot be
    // emptied by hand.
    if (def.closable && !DockIsLastPanel(s, g->node) &&
        !DockNodeLocked(s, g->node)) {
        menu->Separator()->Menu(Tr("Dock.Close"));
    }
    if (PopupMenuState* ms = menu->state.Get(cx)) {
        // The menu hands its listener the row that was taken, so the node
        // travels the only other way it can: the group that has the menu open
        // says so as it builds it.
        ms->onConfirm = ListenTo(g->state, &DockState::OnMenuItem);
        DockGroupOpenMenu(g, ms->open);
    }
    row->Child(component::DropdownMenu::New(cx, menuId)
                   ->Trigger(component::Button::New(cx, menuId)
                                 ->Icon(IconName::Ellipsis)
                                 ->Ghost()
                                 ->Compact()
                                 ->WithSize(UiSize::XSmall)
                                 ->TabStop(false)
                                 ->IntoEl())
                   ->Menu(menu)
                   ->AnchorRight()
                   ->IntoEl());
    return row;
}

// The single-panel branch of render_title_bar: with PanelStyle::Auto — the
// default — a group showing one panel is a plain 30-DIP title row with no tab
// chrome at all, and the title itself is what a drag picks up.
static El* RenderTitleRow(const DockTabGroup* g) {
    Ctx* cx = g->cx;
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    int activeIx = DockGroupActiveIx(g);
    const DockPanelDef* def = DockGroupPanel(g, activeIx);
    if (!def) {
        return Div(a);
    }
    bool leading = HasLeadingToggles(g);
    bool trailing = DockGroupHasToggle(g, DockPlacement::Right);
    El* row = Div(a)
                  ->FlexRow()
                  ->ItemsCenter()
                  ->JustifyBetween()
                  ->W(kFill)
                  ->H(30)
                  ->PadY(8)
                  ->PadL(leading ? 8.f : 12.f)
                  ->PadR(8);
    if (leading) {
        row->Child(RenderToggles(g, false));
    }
    // No line height of its own: `rems(1.0)` is exactly the 16px font size, so
    // the line box has no room for a descender and an `Agent` or a `Properties`
    // loses the tail of its letters wherever the wrapper clips. The row's own
    // 30px and its centring are what place the title.
    El* title = Div(a)->Flex1()->MinW(64)->ClipX()->Child(
        TextEl(a, def->title)->Font(14)->Truncate()->Fg(th.foreground));
    row->Child(DockBindTitleDrag(g, activeIx, title));
    if (!g->collapsed && def->titleSuffix) {
        if (El* suffix = def->titleSuffix(cx, def->data)) {
            row->Child(suffix->Shrink0());
        }
    }
    El* tools = Div(a)->FlexRow()->ItemsCenter()->Shrink0()->Gap(4);
    tools->Child(RenderTools(g));
    if (trailing) {
        tools->Child(RenderToggles(g, true));
    }
    row->Child(tools);
    return row;
}

// TabGroupRenderer::render_tab_bar — the tab bar, or the plain title row a
// lone panel gets.
static El* SkinTabBar(Ctx* cx, void*, const DockTabGroup* g) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    DockState* s = g->state.Get(cx);
    int activeIx = DockGroupActiveIx(g);

    // `visible_panels.len() == 1 && panel_style == PanelStyle::default()`.
    if (DockVisibleCount(s, g->node) == 1 &&
        s->panelStyle == DockPanelStyle::Auto) {
        return RenderTitleRow(g);
    }

    El* bar = Div(a)
                  ->FlexRow()
                  ->ItemsCenter()
                  ->W(kFill)
                  ->H(kDockTabBarH)
                  ->Bg(th.tokens.tabBar)
                  ->BorderB(1, th.border);
    if (HasLeadingToggles(g)) {
        bar->Child(RenderToggles(g, false)->PadX(8));
    }
    // TabBar::track_scroll: a row of tabs wider than the bar scrolls sideways
    // rather than being squeezed, and the wheel over it moves it.
    El* strip = DockBindTabStrip(g, Div(a)
                                        ->FlexRow()
                                        ->ItemsCenter()
                                        ->Flex1()
                                        ->H(kFill)
                                        ->MinW(0)
                                        ->ClipX()
                                        ->HideScrollbar());
    int count = DockGroupCount(g);
    for (int i = 0; i < count; i++) {
        const DockPanelDef* def = DockGroupPanel(g, i);
        // A hidden panel has no tab at all.
        if (!def->visible) {
            continue;
        }
        // "Always not show active tab style, if the panel is collapsed".
        bool on = i == activeIx && !g->collapsed;
        // A tab keeps its own width: a row too wide for the bar scrolls
        // rather than squeezing its labels.
        El* tab = Div(a)
                      ->FlexRow()
                      ->ItemsCenter()
                      ->Shrink0()
                      ->Gap(4)
                      ->PadX(10)
                      ->H(kFill)
                      ->Bg(on ? th.tabActiveBg : th.tabBar);
        if (i > 0) {
            tab->BorderL(1, th.border);
        }
        DockBindTab(g, i, tab);
        // `.drag_over::<DragPanel>(|this, ..| this.border_l_2()
        // .border_color(cx.theme().drag_border))`: the tab a drop would land
        // before is marked down its left edge, and the mark is a refinement
        // the element carries rather than a question the bar asks the window
        // while it builds.
        if (DockGroupDroppable(g)) {
            tab->DragOver(kDockPanelDrag, StateStyle()
                                              .BorderL(2, th.dragBorder));
        }
        // Tab::new().label(..): a tab's label is the tab bar's own size,
        // which is text_sm at Medium. `tab_name` is the short form a panel
        // offers for the bar; the title is the fallback.
        Str label = def->tabName.s ? def->tabName : def->title;
        tab->Child(TextEl(a, label)
                       ->Font(14)
                       ->Fg(on ? th.tabActiveFg : th.tabFg)
                       ->LineHeight(1.f));
        if (def->closable && !DockIsLastPanel(s, g->node) &&
            !DockNodeLocked(s, g->node)) {
            tab->Child(DockBindClose(
                g, i,
                Div(a)
                    ->Pad(2)
                    ->Radius(th.radius * 0.5f)
                    ->HoverBg(th.tokens.secondary)
                    ->Child(IconEl(a, IconName::X, 12)->Fg(th.mutedFg))));
        }
        strip->Child(tab);
    }
    // last_empty_space: the run of bar past the last tab, which takes a drop
    // as "put it at the end" and lights up while a panel is over it.
    El* rest = DockBindTabRest(g, Div(a)->Flex1()->H(kFill)->MinW(64));
    if (DockGroupDroppable(g)) {
        rest->DragOver(kDockPanelDrag, StateStyle().Bg(th.tokens.dropTarget));
    }
    strip->Child(rest);
    bar->Child(strip);
    // `.when(!self.collapsed, |this| this.suffix(..))`: the active panel's own
    // bar content, then the group's tools, then the right dock's toggle.
    if (!g->collapsed) {
        const DockPanelDef* def = DockGroupPanel(g, activeIx);
        if (def && def->titleSuffix) {
            if (El* suffix = def->titleSuffix(cx, def->data)) {
                bar->Child(suffix->Shrink0());
            }
        }
        bar->Child(RenderTools(g));
        if (DockGroupHasToggle(g, DockPlacement::Right)) {
            bar->Child(RenderToggles(g, true)->PadX(8));
        }
    }
    return bar;
}

static El* SkinFrame(Ctx* cx, void*) {
    return Div(cx->a)->Bg(cx->theme().tokens.background);
}

static El* SkinTabContent(Ctx* cx, void*, const DockTabGroup*) {
    return Div(cx->a)->ClipY()->Bg(cx->theme().tokens.background);
}

// render_split_handle: base keeps the four-DIP grab, the cursor and the drag;
// all this says is what it looks like under the pointer.
static El* SkinSplitHandle(Ctx* cx, void*, const DockHandleCtx* h) {
    El* e = Div(cx->a)->SizeFull();
    if (h->hovered || h->active) {
        e->Bg(cx->theme().border);
    }
    return e;
}

// render_dock: one Dock's box, with the strip on its inner edge. A shut left
// or right Dock takes no space at all; a shut bottom one keeps its tab bar,
// so there is still something to click to open it again.
static El* SkinDock(Ctx* cx, void*, const DockCtx* d, El* content) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    if (d->placement == DockPlacement::Bottom) {
        float h = d->open ? d->size : kDockCollapsedH;
        El* dock =
            Div(a)->FlexCol()->Shrink0()->W(kFill)->H(h)->BorderT(1, th.border);
        if (d->open) {
            dock->Child(DockBindResizeStrip(d, Div(a)
                                                   ->W(kFill)
                                                   ->H(kDockHandleW)
                                                   ->Shrink0()
                                                   ->HoverBg(th.border)));
        }
        dock->Child(Div(a)->FlexCol()->Flex1()->W(kFill)->Child(content));
        return dock;
    }
    if (!d->open) {
        return nullptr;
    }
    bool left = d->placement == DockPlacement::Left;
    El* dock = Div(a)->FlexRow()->Shrink0()->W(d->size)->H(kFill);
    El* strip = DockBindResizeStrip(
        d, Div(a)->H(kFill)->W(kDockHandleW)->Shrink0()->HoverBg(th.border));
    El* body = Div(a)->FlexCol()->Flex1()->H(kFill)->Child(content);
    if (left) {
        dock->Child(body->BorderR(1, th.border))->Child(strip);
    } else {
        dock->Child(strip)->Child(body->BorderL(1, th.border));
    }
    return dock;
}

static El* SkinDropIndicator(Ctx* cx, void*, Bounds) {
    return Div(cx->a)->Bg(BackgroundOpacity(cx->theme().tokens.primary, 0.2f));
}

// TabPanel::render_drag_panel: w_24, py_1, px_3, a border, the active tab's
// surface, and 0.75 of the opacity. Base puts it under the pointer.
static El* SkinDragPreview(Ctx* cx, void*, const DockPanelDef* def) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    return Div(a)
        ->W(kDockDragPreviewW)
        ->PadY(4)
        ->PadX(12)
        ->ClipX()
        ->Radius(th.radius)
        ->Border(1, th.border)
        ->Bg(th.tokens.tabActiveBg)
        ->Opacity(0.75f)
        // DragPanel names no size, so the drag preview reads at the base.
        ->Child(TextEl(a, def->title)->Fg(th.tabFg)->LineHeight(1.f));
}

static const DockRenderer& ThemedRenderer() {
    static DockRenderer r;
    static bool inited = false;
    if (!inited) {
        inited = true;
        r.frame = SkinFrame;
        r.splitHandle = SkinSplitHandle;
        r.dock = SkinDock;
        r.tabContentFrame = SkinTabContent;
        r.tabBar = SkinTabBar;
        r.dropIndicator = SkinDropIndicator;
        r.dragPreview = SkinDragPreview;
    }
    return r;
}

El* DockArea::IntoEl() {
    return gpui::DockArea::New(cx, id, state, &ThemedRenderer());
}

} // namespace component
} // namespace gpui

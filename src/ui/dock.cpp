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
                    ->Font(13)
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

static El* RenderNode(Ctx* cx, Str id, Entity<DockState> st, int node,
                      int toolbarNode);

// resize_handle: four pixels between two panels, or along the inner edge of a
// Dock. `packed` is the node and the child it sits after, which is what the
// drag carries back to DockState::OnResizeDrag.
static El* ResizeHandle(Ctx* cx, Str hid, Entity<DockState> st, intptr_t packed,
                        Axis axis) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    bool horizontal = AxisIsHorizontal(axis);
    El* e = Div(a)->Id(hid)->Click(HashClickId(hid))->FlexRow();
    if (horizontal) {
        e->W(kDockHandleW)->H(kFill)->Cursor(CursorKind::ColResize);
    } else {
        e->H(kDockHandleW)->W(kFill)->Cursor(CursorKind::RowResize);
    }
    e->HoverBg(th.border);
    e->OnDrag(kDockResizeDrag, (int)packed);
    e->OnDragMove(ListenTo(st, &DockState::OnResizeDrag));
    e->OnMouseUpOut(ListenTo(st, &DockState::OnResizeEnd));
    e->OnMouseUp(ListenTo(st, &DockState::OnResizeEnd));
    return e;
}

// The three toggle buttons Rust hangs off the tab panel it picked for each
// edge (DockArea::toggle_button_panels).
static El* ToggleButton(Ctx* cx, Str bid, Entity<DockState> st, DockPlacement p,
                        IconName icon) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* e = Div(a)
                ->Pad(4)
                ->Radius(th.radius * 0.5f)
                ->HoverBg(th.secondary)
                ->Child(IconEl(a, icon, 14)->Fg(th.mutedFg));
    BindClick(e, bid, ListenTo(st, &DockState::OnToggleSide, (intptr_t)p));
    // tab_panel.rs marks every tool on the bar `.tab_stop(false)`: the panel
    // is what Tab moves between, not the four buttons hung off its edge.
    e->TabStop(false);
    return e;
}

static El* RenderToolbar(Ctx* cx, Str id, Entity<DockState> st, bool trailing) {
    Arena* a = cx->a;
    DockState* s = st.Get(cx);
    El* row = Div(a)->FlexRow()->ItemsCenter()->Gap(2)->PadX(4);
    if (trailing) {
        if (s->right.node >= 0 && s->right.collapsible) {
            row->Child(ToggleButton(cx, StrDup(a, fmt("%s-toggle-right", id)),
                                    st, DockPlacement::Right,
                                    s->right.open ? IconName::PanelRightClose
                                                  : IconName::PanelRightOpen));
        }
        return row;
    }
    if (s->left.node >= 0 && s->left.collapsible) {
        row->Child(ToggleButton(
            cx, StrDup(a, fmt("%s-toggle-left", id)), st, DockPlacement::Left,
            s->left.open ? IconName::PanelLeftClose : IconName::PanelLeftOpen));
    }
    if (s->bottom.node >= 0 && s->bottom.collapsible) {
        row->Child(ToggleButton(
            cx, StrDup(a, fmt("%s-toggle-bottom", id)), st,
            DockPlacement::Bottom,
            s->bottom.open ? IconName::ChevronDown : IconName::ChevronUp));
    }
    return row;
}

// TabPanel::render. The tab bar, the active panel under it, and — while a tab
// is being dragged over this group — the half of the box the drop would take.
static El* RenderTabs(Ctx* cx, Str id, Entity<DockState> st, int node,
                      bool toolbar) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    DockState* s = st.Get(cx);
    DockNode& n = s->nodes[node];

    // TabPanel::collapsed: a Dock that is shut keeps its tab bar and nothing
    // else — no body, and none of the suffix buttons. Clicking a tab in it is
    // what opens the Dock again.
    DockSide* side = DockSideOf(s, DockPlacementOfNode(s, node));
    bool collapsed = side && !side->open;

    El* box = Div(a)->FlexCol()->SizeFull();
    // The group is its own drop target, and its box is what decides which of
    // the five zones a drop landed in — so the box has to be reported back.
    box->BoundsOut(&n.bounds);
    box->OnDrop(kDockPanelDrag,
                ListenTo(st, &DockState::OnDropPanel, (intptr_t)node));

    El* bar = Div(a)
                  ->FlexRow()
                  ->ItemsCenter()
                  ->W(kFill)
                  ->H(kDockTabBarH)
                  ->Bg(th.tabBar)
                  ->BorderB(1, th.border);
    if (toolbar) {
        bar->Child(RenderToolbar(cx, id, st, false));
    }
    // TabBar::track_scroll: a row of tabs wider than the bar scrolls sideways
    // rather than being squeezed, and the wheel over it moves it. The tab
    // just made active is brought into view from where last frame put it,
    // which is scroll_to_item.
    if (n.pendingScrollIx >= 0) {
        if (n.activeTabBoundsIx == n.pendingScrollIx) {
            n.tabScrollX = DockTabScrollTo(n.tabScrollX, n.tabStripBounds,
                                           n.activeTabBounds);
            n.pendingScrollIx = -1;
        } else {
            // Its box is measured by this frame; the scroll is worked out on
            // the next one, which is why one is asked for.
            WindowRequestAnimationFrame(cx->win);
        }
    }
    Str scrollId = StrDup(a, fmt("%s-tabscroll-%d", id, node));
    El* strip = Div(a)
                    ->FlexRow()
                    ->ItemsCenter()
                    ->Grow()
                    ->H(kFill)
                    ->MinW(0)
                    ->ClipX()
                    ->ScrollX(n.tabScrollX)
                    ->HideScrollbar()
                    ->ScrollId(HashClickId(scrollId))
                    ->OnScroll(ListenTo(st, &DockState::OnTabBarScroll,
                                        (intptr_t)node));
    strip->BoundsOut(&n.tabStripBounds);
    for (int i = 0; i < n.nPanel; i++) {
        int panelIx = n.panel[i];
        const DockPanelDef& def = s->panels[panelIx];
        bool on = i == n.activeIx;
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
        BindClick(tab, StrDup(a, fmt("%s-tab-%d-%d", id, node, i)),
                  ListenTo(st, &DockState::OnTabClick, DockPack(node, i)));
        // on_drag(DragPanel::new(..)): the tab is what a press picks up, and
        // the moves that follow say where it would land.
        if (!s->locked) {
            tab->OnDrag(kDockPanelDrag, panelIx);
            tab->OnDragMove(ListenTo(st, &DockState::OnTabDragMove));
            tab->OnMouseUpOut(ListenTo(st, &DockState::OnTabDragEnd));
            tab->OnMouseUp(ListenTo(st, &DockState::OnTabDragEnd));
        }
        // Every tab is a drop target of its own: a panel let go over one
        // takes that place in the row, which is what makes a tab reorder.
        // Rust marks the tab it would land before with a border down its left
        // edge while the drag is over it.
        if (!s->locked) {
            Str dropId = StrDup(a, fmt("%s-tabdrop-%d-%d", id, node, i));
            tab->Id(dropId)
                ->OnDrop(kDockPanelDrag, ListenTo(st, &DockState::OnDropTab,
                                                  DockPack(node, i)));
            const DragPayload* drag = WindowActiveDrag(cx);
            if (drag && StrSame(drag->kind, kDockPanelDrag) &&
                WindowDragOverId(cx) == HashClickId(dropId)) {
                tab->BorderL(2, th.primary);
            }
        }
        tab->Child(TextEl(a, def.title)
                       ->Font(13)
                       ->Fg(on ? th.tabActiveFg : th.tabFg)
                       ->LineHeight(1.f));
        if (def.closable) {
            El* x = Div(a)
                        ->Pad(2)
                        ->Radius(th.radius * 0.5f)
                        ->HoverBg(th.secondary)
                        ->Child(IconEl(a, IconName::X, 12)->Fg(th.mutedFg));
            BindClick(
                x, StrDup(a, fmt("%s-close-%d-%d", id, node, i)),
                ListenTo(st, &DockState::OnCloseClick, DockPack(node, i)));
            tab->Child(x);
        }
        if (on) {
            tab->BoundsOut(&n.activeTabBounds);
            n.activeTabBoundsIx = i;
        }
        strip->Child(tab);
    }
    // last_empty_space: the run of bar past the last tab, which takes a drop
    // as "put it at the end" and lights up while a panel is over it.
    El* rest = Div(a)->Grow()->H(kFill)->MinW(64);
    (void)0;
    if (!s->locked) {
        Str restId = StrDup(a, fmt("%s-tabrest-%d", id, node));
        rest->Id(restId)
            ->OnDrop(kDockPanelDrag,
                     ListenTo(st, &DockState::OnDropTabBar, (intptr_t)node));
        const DragPayload* drag = WindowActiveDrag(cx);
        if (drag && StrSame(drag->kind, kDockPanelDrag) &&
            WindowDragOverId(cx) == HashClickId(restId)) {
            rest->Bg(RgbaOpacity(th.primary, 0.2f));
        }
    }
    strip->Child(rest);
    bar->Child(strip);
    if (!collapsed && n.nPanel > 0 && s->panels[n.panel[n.activeIx]].zoomable) {
        int panelIx = n.panel[n.activeIx];
        El* zoom =
            Div(a)
                ->Pad(4)
                ->Radius(th.radius * 0.5f)
                ->HoverBg(th.secondary)
                ->Child(IconEl(a,
                               s->zoomPanel == panelIx ? IconName::Minimize
                                                       : IconName::Maximize,
                               14)
                            ->Fg(th.mutedFg));
        BindClick(zoom, StrDup(a, fmt("%s-zoom-%d", id, node)),
                  ListenTo(st, &DockState::OnZoomClick, (intptr_t)panelIx));
        zoom->TabStop(false);
        bar->Child(zoom);
    }
    // TabPanel::render_toolbar's menu button: the same two actions the
    // toolbar has, where a narrow tab bar can still reach them.
    if (!collapsed && n.nPanel > 0) {
        int panelIx = n.panel[n.activeIx];
        const DockPanelDef& def = s->panels[panelIx];
        bool zoomed = s->zoomPanel == panelIx;
        Str menuId = StrDup(a, fmt("%s-menu-%d", id, node));
        component::PopupMenu* menu = component::PopupMenu::New(cx, menuId);
        menu->Menu(zoomed ? StrL("Zoom Out") : StrL("Zoom In"));
        if (!def.zoomable) {
            menu->Disabled(true);
        }
        if (def.closable) {
            menu->Separator()->Menu(StrL("Close"));
        }
        if (PopupMenuState* ms = menu->state.Get(cx)) {
            // The menu hands its listener the row that was taken, so the node
            // travels the only other way it can: the group that has the menu
            // open says so as it builds it.
            ms->onConfirm = ListenTo(st, &DockState::OnMenuItem);
            if (ms->open) {
                s->menuNode = node;
            }
        }
        bar->Child(component::DropdownMenu::New(cx, menuId)
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
    }
    if (toolbar) {
        bar->Child(RenderToolbar(cx, id, st, true));
    }
    box->Child(bar);

    if (!collapsed) {
        El* body =
            Div(a)->FlexCol()->Grow()->W(kFill)->ClipY()->Bg(th.background);
        if (n.nPanel > 0) {
            const DockPanelDef& def = s->panels[n.panel[n.activeIx]];
            if (def.render) {
                body->Child(def.render(cx, def.data));
            }
        }
        box->Child(body);
    }

    // The drop placeholder. Rust animates it from where it was to where it is
    // now; here it simply is where it is.
    if (s->dropNode == node) {
        Bounds ph = DockDropPlaceholder(n.bounds, s->dropAt);
        box->Child(Div(a)
                       ->Absolute()
                       ->Left(ph.x - n.bounds.x)
                       ->Top(ph.y - n.bounds.y)
                       ->W(ph.w)
                       ->H(ph.h)
                       ->Bg(RgbaOpacity(th.primary, 0.2f)));
    }
    return box;
}

// StackPanel: the children along the axis, each at the size the last drag
// left it, with a handle between every pair.
static El* RenderSplit(Ctx* cx, Str id, Entity<DockState> st, int node,
                       int toolbarNode) {
    Arena* a = cx->a;
    DockState* s = st.Get(cx);
    DockNode& n = s->nodes[node];
    bool horizontal = AxisIsHorizontal(n.axis);
    El* box = Div(a)->SizeFull();
    if (horizontal) {
        box->FlexRow();
    } else {
        box->FlexCol();
    }
    box->BoundsOut(&n.bounds);
    for (int i = 0; i < n.nChild; i++) {
        El* wrap = Div(a)->FlexCol();
        // The last child takes what is left, so rounding never leaves a gap
        // down the edge of the split.
        if (i == n.nChild - 1) {
            wrap->Grow();
            if (horizontal) {
                wrap->H(kFill);
            } else {
                wrap->W(kFill);
            }
        } else if (horizontal) {
            wrap->W(n.size[i])->H(kFill);
        } else {
            wrap->H(n.size[i])->W(kFill);
        }
        wrap->Child(RenderNode(cx, id, st, n.child[i], toolbarNode));
        box->Child(wrap);
        if (i < n.nChild - 1) {
            box->Child(
                ResizeHandle(cx, StrDup(a, fmt("%s-split-%d-%d", id, node, i)),
                             st, DockPack(node, i), n.axis));
        }
    }
    return box;
}

static El* RenderNode(Ctx* cx, Str id, Entity<DockState> st, int node,
                      int toolbarNode) {
    DockState* s = st.Get(cx);
    if (node < 0 || node >= kMaxDockNodes || !s->nodes[node].used) {
        return Div(cx->a)->SizeFull();
    }
    if (s->nodes[node].split) {
        return RenderSplit(cx, id, st, node, toolbarNode);
    }
    return RenderTabs(cx, id, st, node, node == toolbarNode);
}

// TabPanel::render_drag_panel: what follows the pointer while a tab is being
// dragged. Rust hands GPUI a view to render as the drag's own; here the dock
// draws it itself, over everything, at the point the press was inside the tab
// so the label sits where it was picked up from.
static El* RenderDragPreview(Ctx* cx, DockState* s) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    const DragPayload* drag = WindowActiveDrag(cx);
    if (!drag || !StrSame(drag->kind, kDockPanelDrag)) {
        return nullptr;
    }
    int panelIx = drag->ix;
    if (panelIx < 0 || panelIx >= s->nPanels) {
        return nullptr;
    }
    Point off = WindowDragOffset(cx);
    // w_24, py_1, px_3, a border, the active tab's surface, and 0.75 of the
    // opacity — the whole of Rust's `drag-panel`.
    return Div(a)
        ->Fixed()
        ->Left(cx->win->mouseX - off.x)
        ->Top(cx->win->mouseY - off.y)
        ->W(96)
        ->PadY(4)
        ->PadX(12)
        ->ClipX()
        ->Radius(th.radius)
        ->Border(1, th.border)
        ->Bg(th.tabActiveBg)
        ->Opacity(0.75f)
        ->Child(TextEl(a, s->panels[panelIx].title)
                    ->Font(13)
                    ->Fg(th.tabFg)
                    ->LineHeight(1.f))
        ->Deferred();
}

// Which tab group carries the toggle buttons: the first one in the centre
// item, which is where Rust's `toggle_button_panels` ends up for a plain
// left-to-right layout.
static int FirstTabsNode(const DockState* s, int node) {
    if (node < 0 || node >= kMaxDockNodes || !s->nodes[node].used) {
        return -1;
    }
    if (!s->nodes[node].split) {
        return node;
    }
    for (int i = 0; i < s->nodes[node].nChild; i++) {
        int found = FirstTabsNode(s, s->nodes[node].child[i]);
        if (found >= 0) {
            return found;
        }
    }
    return -1;
}

El* DockArea::IntoEl() {
    const Theme& th = cx->theme();
    DockState* s = state.Get(cx);
    if (!s) {
        return Div(a)->SizeFull();
    }
    El* box = Div(a)->FlexCol()->SizeFull()->Bg(th.background);
    box->BoundsOut(&s->bounds);

    // ToggleZoom: one panel over the whole area, and nothing else rendered.
    if (s->zoomPanel >= 0) {
        int node = DockNodeOfPanel(s, s->zoomPanel);
        if (node >= 0) {
            box->Child(RenderTabs(cx, id, state, node, false));
            return box;
        }
        s->zoomPanel = -1;
    }

    int toolbarNode = FirstTabsNode(s, s->center);
    El* row = Div(a)->FlexRow()->Grow()->W(kFill);
    if (s->left.node >= 0 && s->left.open) {
        El* dock = Div(a)->FlexRow()->W(s->left.size)->H(kFill);
        dock->Child(
            Div(a)
                ->FlexCol()
                ->Grow()
                ->H(kFill)
                ->BorderR(1, th.border)
                ->Child(RenderNode(cx, id, state, s->left.node, toolbarNode)));
        dock->Child(
            ResizeHandle(cx, StrDup(a, fmt("%s-dock-left", id)), state,
                         DockPack(kMaxDockNodes + (int)DockPlacement::Left, 0),
                         Axis::Horizontal));
        row->Child(dock);
    }
    row->Child(Div(a)->FlexCol()->Grow()->H(kFill)->Child(
        RenderNode(cx, id, state, s->center, toolbarNode)));
    if (s->right.node >= 0 && s->right.open) {
        El* dock = Div(a)->FlexRow()->W(s->right.size)->H(kFill);
        dock->Child(
            ResizeHandle(cx, StrDup(a, fmt("%s-dock-right", id)), state,
                         DockPack(kMaxDockNodes + (int)DockPlacement::Right, 0),
                         Axis::Horizontal));
        dock->Child(
            Div(a)
                ->FlexCol()
                ->Grow()
                ->H(kFill)
                ->BorderL(1, th.border)
                ->Child(RenderNode(cx, id, state, s->right.node, toolbarNode)));
        row->Child(dock);
    }
    box->Child(row);
    // The dragged tab's preview goes over everything, which is what a
    // deferred child is for.
    if (El* preview = RenderDragPreview(cx, s)) {
        box->Child(preview);
    }

    if (s->bottom.node >= 0) {
        // A closed bottom dock keeps its tab bar, so there is still something
        // to click to open it again.
        float h = s->bottom.open ? s->bottom.size : kDockCollapsedH;
        El* dock = Div(a)->FlexCol()->W(kFill)->H(h)->BorderT(1, th.border);
        if (s->bottom.open) {
            dock->Child(ResizeHandle(
                cx, StrDup(a, fmt("%s-dock-bottom", id)), state,
                DockPack(kMaxDockNodes + (int)DockPlacement::Bottom, 0),
                Axis::Vertical));
        }
        dock->Child(Div(a)->FlexCol()->Grow()->W(kFill)->Child(
            RenderNode(cx, id, state, s->bottom.node, toolbarNode)));
        box->Child(dock);
    }
    return box;
}

} // namespace component
} // namespace gpui

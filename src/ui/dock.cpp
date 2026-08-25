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

static El* RenderNode(Ctx* cx, Str id, Entity<DockState> st, int node);

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
                ->HoverBg(th.tokens.secondary)
                ->Child(IconEl(a, icon, 14)->Fg(th.mutedFg));
    BindClick(e, bid, ListenTo(st, &DockState::OnToggleSide, (intptr_t)p));
    // tab_panel.rs marks every tool on the bar `.tab_stop(false)`: the panel
    // is what Tab moves between, not the four buttons hung off its edge.
    e->TabStop(false);
    return e;
}

// render_dock_toggle_button: whether this group is the one carrying the
// toggle for that side. Rust keeps the three answers on the DockArea and
// refreshes them as the tree changes (`update_toggle_button_tab_panels`); the
// tree is walked here instead, since a frame is where the question comes up
// and the walk is a few nodes deep.
static bool TogglesHere(DockState* s, int node, DockPlacement p) {
    if (!s->toggleButtonVisible || s->zoomPanel >= 0) {
        return false;
    }
    DockSide* side = DockSideOf(s, p);
    if (!side || side->node < 0 || !side->collapsible) {
        return false;
    }
    if (p == DockPlacement::Bottom) {
        // The bottom toggle lives on the bottom Dock's own first group, which
        // is what makes a collapsed bottom dock's strip usable.
        return node == DockLeftTopTabs(s, s->bottom.node);
    }
    if (p == DockPlacement::Right) {
        return node == DockRightTopTabs(s, s->center);
    }
    return node == DockLeftTopTabs(s, s->center);
}

// The leading pair — left and bottom — or the trailing one, right.
static El* RenderToggles(Ctx* cx, Str id, Entity<DockState> st, int node,
                         bool trailing) {
    Arena* a = cx->a;
    DockState* s = st.Get(cx);
    El* row = Div(a)->FlexRow()->ItemsCenter()->Shrink0()->Gap(4);
    if (trailing) {
        if (TogglesHere(s, node, DockPlacement::Right)) {
            row->Child(ToggleButton(cx, StrDup(a, fmt("%s-toggle-right", id)),
                                    st, DockPlacement::Right,
                                    s->right.open ? IconName::PanelRight
                                                  : IconName::PanelRightOpen));
        }
        return row;
    }
    if (TogglesHere(s, node, DockPlacement::Left)) {
        row->Child(ToggleButton(
            cx, StrDup(a, fmt("%s-toggle-left", id)), st, DockPlacement::Left,
            s->left.open ? IconName::PanelLeft : IconName::PanelLeftOpen));
    }
    if (TogglesHere(s, node, DockPlacement::Bottom)) {
        row->Child(ToggleButton(cx, StrDup(a, fmt("%s-toggle-bottom", id)), st,
                                DockPlacement::Bottom,
                                s->bottom.open ? IconName::PanelBottom
                                               : IconName::PanelBottomOpen));
    }
    return row;
}

static bool HasLeadingToggles(DockState* s, int node) {
    return TogglesHere(s, node, DockPlacement::Left) ||
           TogglesHere(s, node, DockPlacement::Bottom);
}

// TabPanel::render_toolbar: the panel's zoom button — only where its
// PanelControl says the toolbar is one of the places it shows — and the menu
// beside it. A collapsed group draws neither.
static El* RenderTools(Ctx* cx, Str id, Entity<DockState> st, int node,
                       bool collapsed) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    DockState* s = st.Get(cx);
    DockNode& n = s->nodes[node];
    El* row = Div(a)->FlexRow()->ItemsCenter()->Shrink0()->Gap(4);
    int activeIx = DockActiveIx(s, node);
    if (collapsed || activeIx < 0) {
        return row;
    }
    int panelIx = n.panel[activeIx];
    const DockPanelDef& def = s->panels[panelIx];
    bool zoomed = s->zoomPanel == panelIx;
    // `zoomable_toolbar_visible`: a zoomed panel always shows the way back
    // out, and Zoom In is on the bar only for Toolbar and Both.
    if (zoomed || DockPanelControlToolbar(def.zoomable)) {
        El* zoom =
            Div(a)
                ->Pad(4)
                ->Radius(th.radius * 0.5f)
                ->HoverBg(th.tokens.secondary)
                ->Child(IconEl(a,
                               zoomed ? IconName::Minimize : IconName::Maximize,
                               14)
                            ->Fg(th.mutedFg));
        BindClick(zoom, StrDup(a, fmt("%s-zoom-%d", id, node)),
                  ListenTo(st, &DockState::OnZoomClick, (intptr_t)panelIx));
        zoom->TabStop(false);
        row->Child(zoom);
    }
    // The menu button: the same two actions, where a narrow bar can still
    // reach them.
    Str menuId = StrDup(a, fmt("%s-menu-%d", id, node));
    component::PopupMenu* menu = component::PopupMenu::New(cx, menuId);
    menu->Menu(zoomed ? StrL("Zoom Out") : StrL("Zoom In"));
    if (!DockPanelControlMenu(def.zoomable) && !zoomed) {
        menu->Disabled(true);
    }
    // `closable`: the last panel of a dock has no Close, so a dock cannot be
    // emptied by hand.
    if (def.closable && !DockIsLastPanel(s, node) && !DockNodeLocked(s, node)) {
        menu->Separator()->Menu(StrL("Close"));
    }
    if (PopupMenuState* ms = menu->state.Get(cx)) {
        // The menu hands its listener the row that was taken, so the node
        // travels the only other way it can: the group that has the menu open
        // says so as it builds it.
        ms->onConfirm = ListenTo(st, &DockState::OnMenuItem);
        if (ms->open) {
            s->menuNode = node;
        }
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
static El* RenderTitleRow(Ctx* cx, Str id, Entity<DockState> st, int node,
                          bool collapsed) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    DockState* s = st.Get(cx);
    DockNode& n = s->nodes[node];
    int activeIx = DockActiveIx(s, node);
    if (activeIx < 0) {
        return Div(a);
    }
    int panelIx = n.panel[activeIx];
    const DockPanelDef& def = s->panels[panelIx];
    bool leading = HasLeadingToggles(s, node);
    bool trailing = TogglesHere(s, node, DockPlacement::Right);
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
        row->Child(RenderToggles(cx, id, st, node, false));
    }
    // No line height of its own: `rems(1.0)` is exactly the 16px font size, so
    // the line box has no room for a descender and an `Agent` or a `Properties`
    // loses the tail of its letters wherever the wrapper clips. The row's own
    // 30px and its centring are what place the title.
    El* title = Div(a)->Flex1()->MinW(64)->ClipX()->Child(
        TextEl(a, def.title)->Font(14)->Truncate()->Fg(th.foreground));
    if (DockNodeDraggable(s, node)) {
        title->Id(StrDup(a, fmt("%s-title-%d", id, node)))
            ->OnDrag(kDockPanelDrag, panelIx)
            ->OnDragMove(ListenTo(st, &DockState::OnTabDragMove))
            ->OnMouseUpOut(ListenTo(st, &DockState::OnTabDragEnd))
            ->OnMouseUp(ListenTo(st, &DockState::OnTabDragEnd));
    }
    row->Child(title);
    if (!collapsed && def.titleSuffix) {
        if (El* suffix = def.titleSuffix(cx, def.data)) {
            row->Child(suffix->Shrink0());
        }
    }
    El* tools = Div(a)->FlexRow()->ItemsCenter()->Shrink0()->Gap(4);
    tools->Child(RenderTools(cx, id, st, node, collapsed));
    if (trailing) {
        tools->Child(RenderToggles(cx, id, st, node, true));
    }
    row->Child(tools);
    return row;
}

// TabPanel::render. The tab bar — or the plain title row a lone panel gets —
// the active panel under it, and, while a tab is being dragged over this
// group, the half of the box the drop would take.
static El* RenderTabs(Ctx* cx, Str id, Entity<DockState> st, int node) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    DockState* s = st.Get(cx);
    DockNode& n = s->nodes[node];

    // TabPanel::collapsed: a Dock that is shut keeps its tab bar and nothing
    // else — no body, and none of the suffix buttons. Clicking a tab in it is
    // what opens the Dock again.
    DockSide* side = DockSideOf(s, DockPlacementOfNode(s, node));
    bool collapsed = side && !side->open;
    bool droppable = DockNodeDroppable(s, node);
    bool draggable = DockNodeDraggable(s, node);
    int activeIx = DockActiveIx(s, node);

    El* box = Div(a)->FlexCol()->SizeFull();
    // The group is its own drop target, and its box is what decides which of
    // the five zones a drop landed in — so the box has to be reported back.
    box->BoundsOut(&n.bounds);
    box->OnDrop(kDockPanelDrag,
                ListenTo(st, &DockState::OnDropPanel, (intptr_t)node));

    // `visible_panels.len() == 1 && panel_style == PanelStyle::default()`.
    if (DockVisibleCount(s, node) == 1 &&
        s->panelStyle == DockPanelStyle::Auto) {
        box->Child(RenderTitleRow(cx, id, st, node, collapsed));
    } else {
        El* bar = Div(a)
                      ->FlexRow()
                      ->ItemsCenter()
                      ->W(kFill)
                      ->H(kDockTabBarH)
                      ->Bg(th.tokens.tabBar)
                      ->BorderB(1, th.border);
        if (HasLeadingToggles(s, node)) {
            bar->Child(RenderToggles(cx, id, st, node, false)->PadX(8));
        }
        // TabBar::track_scroll: a row of tabs wider than the bar scrolls
        // sideways rather than being squeezed, and the wheel over it moves it.
        // The tab just made active is brought into view from where last frame
        // put it, which is scroll_to_item.
        if (n.pendingScrollIx >= 0) {
            if (n.activeTabBoundsIx == n.pendingScrollIx) {
                n.tabScrollX = DockTabScrollTo(n.tabScrollX, n.tabStripBounds,
                                               n.activeTabBounds);
                n.pendingScrollIx = -1;
            } else {
                // Its box is measured by this frame; the scroll is worked out
                // on the next one, which is why one is asked for.
                WindowRequestAnimationFrame(cx->win);
            }
        }
        Str scrollId = StrDup(a, fmt("%s-tabscroll-%d", id, node));
        El* strip = Div(a)
                        ->FlexRow()
                        ->ItemsCenter()
                        ->Flex1()
                        ->H(kFill)
                        ->MinW(0)
                        ->ClipX()
                        ->ScrollX(n.tabScrollX)
                        ->HideScrollbar()
                        ->ScrollId(HashClickId(scrollId))
                        ->OnScroll(ListenTo(st, &DockState::OnTabBarScroll,
                                            (intptr_t)node));
        strip->BoundsOut(&n.tabStripBounds);
        for (int i = 0; i < n.panel.len; i++) {
            int panelIx = n.panel[i];
            const DockPanelDef& def = s->panels[panelIx];
            // A hidden panel has no tab at all.
            if (!def.visible) {
                continue;
            }
            // "Always not show active tab style, if the panel is collapsed".
            bool on = i == activeIx && !collapsed;
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
            // `when(!droppable, ..)`, where that local is `self.collapsed`: a
            // shut dock's tabs neither pick up nor take a drag, they only
            // click to open it again.
            if (!collapsed) {
                // on_drag(DragPanel::new(..)): the tab is what a press picks
                // up, and the moves that follow say where it would land.
                if (draggable) {
                    tab->OnDrag(kDockPanelDrag, panelIx);
                    tab->OnDragMove(ListenTo(st, &DockState::OnTabDragMove));
                    tab->OnMouseUpOut(ListenTo(st, &DockState::OnTabDragEnd));
                    tab->OnMouseUp(ListenTo(st, &DockState::OnTabDragEnd));
                }
                // Every tab is a drop target of its own: a panel let go over
                // one takes that place in the row, which is what makes a tab
                // reorder. Rust marks the tab it would land before with a
                // border down its left edge while the drag is over it.
                if (droppable) {
                    Str dropId =
                        StrDup(a, fmt("%s-tabdrop-%d-%d", id, node, i));
                    tab->Id(dropId)->OnDrop(
                        kDockPanelDrag,
                        ListenTo(st, &DockState::OnDropTab, DockPack(node, i)));
                    const DragPayload* drag = WindowActiveDrag(cx);
                    if (drag && StrSame(drag->kind, kDockPanelDrag) &&
                        WindowDragOverId(cx) == HashClickId(dropId)) {
                        tab->BorderL(2, th.primary);
                    }
                }
            }
            // Tab::new().label(..): a tab's label is the tab bar's own size,
            // which is text_sm at Medium. `tab_name` is the short form a panel
            // offers for the bar; the title is the fallback.
            Str label = def.tabName.s ? def.tabName : def.title;
            tab->Child(TextEl(a, label)
                           ->Font(14)
                           ->Fg(on ? th.tabActiveFg : th.tabFg)
                           ->LineHeight(1.f));
            if (def.closable && !DockIsLastPanel(s, node) &&
                !DockNodeLocked(s, node)) {
                El* x = Div(a)
                            ->Pad(2)
                            ->Radius(th.radius * 0.5f)
                            ->HoverBg(th.tokens.secondary)
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
        // last_empty_space: the run of bar past the last tab, which takes a
        // drop as "put it at the end" and lights up while a panel is over it.
        El* rest = Div(a)->Flex1()->H(kFill)->MinW(64);
        if (droppable && !collapsed) {
            Str restId = StrDup(a, fmt("%s-tabrest-%d", id, node));
            rest->Id(restId)
                ->OnDrop(kDockPanelDrag, ListenTo(st, &DockState::OnDropTabBar,
                                                  (intptr_t)node));
            const DragPayload* drag = WindowActiveDrag(cx);
            if (drag && StrSame(drag->kind, kDockPanelDrag) &&
                WindowDragOverId(cx) == HashClickId(restId)) {
                rest->Bg(BackgroundOpacity(th.tokens.primary, 0.2f));
            }
        }
        strip->Child(rest);
        bar->Child(strip);
        // `.when(!self.collapsed, |this| this.suffix(..))`: the active panel's
        // own bar content, then the group's tools, then the right dock's
        // toggle.
        if (!collapsed) {
            if (activeIx >= 0) {
                const DockPanelDef& def = s->panels[n.panel[activeIx]];
                if (def.titleSuffix) {
                    if (El* suffix = def.titleSuffix(cx, def.data)) {
                        bar->Child(suffix->Shrink0());
                    }
                }
            }
            bar->Child(RenderTools(cx, id, st, node, collapsed));
            if (TogglesHere(s, node, DockPlacement::Right)) {
                bar->Child(RenderToggles(cx, id, st, node, true)->PadX(8));
            }
        }
        box->Child(bar);
    }

    if (!collapsed) {
        El* body = Div(a)->FlexCol()->Flex1()->W(kFill)->ClipY()->Bg(
            th.tokens.background);
        if (activeIx >= 0) {
            const DockPanelDef& def = s->panels[n.panel[activeIx]];
            if (def.render) {
                body->Child(def.render(cx, def.data));
            }
        }
        box->Child(body);
    }

    // The drop placeholder, sprung. `sync_drop_placeholder` keeps a from, a
    // to and an epoch and restarts a run whenever the zone changes; the four
    // numbers are springs here, and a pointer crossing from one half of a
    // group to the other retargets them faster than they arrive — which is
    // what a spring is for. PLACEHOLDER_SPRING: a 200 ms response, and half a
    // pixel is arrived. The one thing they cannot work out for themselves is
    // where the *first* one starts, so a drag reaching a group seeds them
    // with the dragged tab's preview and the placeholder flies in from under
    // the pointer.
    if (s->dropNode == node) {
        Bounds ph = DockDropPlaceholder(n.bounds, s->dropAt);
        Spring spring = SpringNew(200.f);
        spring.epsilon = 0.5f;
        uint32_t kx = MotionId(id, StrL("drop-x"));
        uint32_t ky = MotionId(id, StrL("drop-y"));
        uint32_t kw = MotionId(id, StrL("drop-w"));
        uint32_t kh = MotionId(id, StrL("drop-h"));
        if (s->dropFromPending) {
            s->dropFromPending = false;
            SpringSeed(cx, kx, s->dropFrom.x);
            SpringSeed(cx, ky, s->dropFrom.y);
            SpringSeed(cx, kw, s->dropFrom.w);
            SpringSeed(cx, kh, s->dropFrom.h);
        }
        float x = SpringValue(cx, kx, ph.x, spring);
        float y = SpringValue(cx, ky, ph.y, spring);
        float w = SpringValue(cx, kw, ph.w, spring);
        float h = SpringValue(cx, kh, ph.h, spring);
        box->Child(Div(a)
                       ->Absolute()
                       ->Left(x - n.bounds.x)
                       ->Top(y - n.bounds.y)
                       ->W(w)
                       ->H(h)
                       ->Bg(BackgroundOpacity(th.tokens.primary, 0.2f)));
    }
    return box;
}

// StackPanel: the children along the axis, each at the size the last drag
// left it, with a handle between every pair.
static El* RenderSplit(Ctx* cx, Str id, Entity<DockState> st, int node) {
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
    // A child with nothing to show takes no slot, and the slot that takes
    // what is left has to be one that is drawn: a hidden slot grows nothing,
    // so leaving the growth on the last child would end the split short of
    // its box and show a band of the frame under the last visible panel.
    int grows = -1;
    for (int i = 0; i < n.child.len; i++) {
        if (DockNodeVisible(s, n.child[i])) {
            grows = i;
        }
    }
    for (int i = 0; i < n.child.len; i++) {
        if (!DockNodeVisible(s, n.child[i])) {
            continue;
        }
        El* wrap = Div(a)->FlexCol();
        if (i == grows) {
            wrap->Flex1();
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
        wrap->Child(RenderNode(cx, id, st, n.child[i]));
        box->Child(wrap);
        // A handle sits between two drawn slots, and there is one only while
        // something is still to come.
        if (i != grows) {
            box->Child(
                ResizeHandle(cx, StrDup(a, fmt("%s-split-%d-%d", id, node, i)),
                             st, DockPack(node, i), n.axis));
        }
    }
    return box;
}

static El* RenderNode(Ctx* cx, Str id, Entity<DockState> st, int node) {
    DockState* s = st.Get(cx);
    if (node < 0 || node >= s->nodes.len || !s->nodes[node].used) {
        return Div(cx->a)->SizeFull();
    }
    if (s->nodes[node].split) {
        return RenderSplit(cx, id, st, node);
    }
    return RenderTabs(cx, id, st, node);
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
    if (panelIx < 0 || panelIx >= s->panels.len) {
        return nullptr;
    }
    Point off = WindowDragOffset(cx);
    // w_24, py_1, px_3, a border, the active tab's surface, and 0.75 of the
    // opacity — the whole of Rust's `drag-panel`.
    return Div(a)
        ->Fixed()
        ->Left(cx->win->mouseX - off.x)
        ->Top(cx->win->mouseY - off.y)
        ->W(kDockDragPreviewW)
        ->PadY(4)
        ->PadX(12)
        ->ClipX()
        ->Radius(th.radius)
        ->Border(1, th.border)
        ->Bg(th.tokens.tabActiveBg)
        ->Opacity(0.75f)
        // DragPanel names no size, so the drag preview reads at the base.
        ->Child(
            TextEl(a, s->panels[panelIx].title)->Fg(th.tabFg)->LineHeight(1.f))
        ->Deferred();
}

El* DockArea::IntoEl() {
    const Theme& th = cx->theme();
    DockState* s = state.Get(cx);
    if (!s) {
        return Div(a)->SizeFull();
    }
    El* box = Div(a)->FlexCol()->SizeFull()->Bg(th.tokens.background);
    box->BoundsOut(&s->bounds);

    // ToggleZoom: one panel over the whole area, and nothing else rendered.
    if (s->zoomPanel >= 0) {
        int node = DockNodeOfPanel(s, s->zoomPanel);
        if (node >= 0) {
            box->Child(RenderTabs(cx, id, state, node));
            return box;
        }
        s->zoomPanel = -1;
    }

    El* row = Div(a)->FlexRow()->Flex1()->W(kFill);
    if (s->left.node >= 0 && s->left.open) {
        El* dock = Div(a)->FlexRow()->W(s->left.size)->H(kFill);
        dock->Child(Div(a)
                        ->FlexCol()
                        ->Flex1()
                        ->H(kFill)
                        ->BorderR(1, th.border)
                        ->Child(RenderNode(cx, id, state, s->left.node)));
        dock->Child(
            ResizeHandle(cx, StrDup(a, fmt("%s-dock-left", id)), state,
                         DockPack(kDockSideBase + (int)DockPlacement::Left, 0),
                         Axis::Horizontal));
        row->Child(dock);
    }
    row->Child(Div(a)->FlexCol()->Flex1()->H(kFill)->Child(
        RenderNode(cx, id, state, s->center)));
    if (s->right.node >= 0 && s->right.open) {
        El* dock = Div(a)->FlexRow()->W(s->right.size)->H(kFill);
        dock->Child(
            ResizeHandle(cx, StrDup(a, fmt("%s-dock-right", id)), state,
                         DockPack(kDockSideBase + (int)DockPlacement::Right, 0),
                         Axis::Horizontal));
        dock->Child(Div(a)
                        ->FlexCol()
                        ->Flex1()
                        ->H(kFill)
                        ->BorderL(1, th.border)
                        ->Child(RenderNode(cx, id, state, s->right.node)));
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
                DockPack(kDockSideBase + (int)DockPlacement::Bottom, 0),
                Axis::Vertical));
        }
        dock->Child(Div(a)->FlexCol()->Flex1()->W(kFill)->Child(
            RenderNode(cx, id, state, s->bottom.node)));
        box->Child(dock);
    }
    return box;
}

} // namespace component
} // namespace gpui

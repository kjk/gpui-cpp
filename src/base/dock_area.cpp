/* DockArea, as an element — crates/base/src/dock/dock_area.rs + tab_group.rs

   The tree, the three Docks around the centre, the splits and their handles,
   each group's body, the drop placeholder and the dragged tab's preview.
   Every pixel comes back through the DockRenderer the caller handed in: base
   draws nothing at all, which is what lets `component::DockArea` and the base
   showcase's page be two skins over one behavior. */

#include "base/dock.h"

#include "base/motion.h"
#include "base/resizable.h"

namespace gpui {

namespace {

// A name among the area's own parts. The area is what carries the caller's
// id, so a tab is `tab-{node}-{ix}` inside it rather than spelling the area
// out again -- which is `("tab", ix)` under the panel's own id in Rust.
Str DockElId(Ctx* cx, const char* what, int a1, int a2) {
    return StrDup(cx->a, fmt("%s-%d-%d", Str(what), a1, a2));
}

// Every bound element names itself the same way, and the id it is found by
// is that name folded with the area's.
void BindId(El* e, Str id, bool focus = false) {
    if (focus) {
        e->PathId(id);
    } else {
        e->PathClick(id);
    }
}

DockState* GroupState(const DockTabGroup* g) {
    if (!g || !g->cx || g->node < 0) {
        return nullptr;
    }
    DockState* s = g->state.Get(g->cx);
    if (!s || g->node >= s->nodes.len || !s->nodes[g->node].used) {
        return nullptr;
    }
    return s;
}

} // namespace

// ---------------------------------------------------------------------------
// What the skin may ask

int DockGroupCount(const DockTabGroup* g) {
    DockState* s = GroupState(g);
    return s ? s->nodes[g->node].panel.len : 0;
}

const DockPanelDef* DockGroupPanel(const DockTabGroup* g, int ix) {
    DockState* s = GroupState(g);
    if (!s) {
        return nullptr;
    }
    DockNode& n = s->nodes[g->node];
    if (ix < 0 || ix >= n.panel.len) {
        return nullptr;
    }
    return &s->panels[n.panel[ix]];
}

int DockGroupActiveIx(const DockTabGroup* g) {
    DockState* s = GroupState(g);
    return s ? DockActiveIx(s, g->node) : -1;
}

DockPlacement DockGroupPlacement(const DockTabGroup* g) {
    DockState* s = GroupState(g);
    return s ? DockPlacementOfNode(s, g->node) : DockPlacement::Center;
}

// render_dock_toggle_button. Rust keeps the three answers on the DockArea and
// refreshes them as the tree changes (`update_toggle_button_tab_panels`); the
// tree is walked here instead, since a frame is where the question comes up
// and the walk is a few nodes deep.
bool DockGroupHasToggle(const DockTabGroup* g, DockPlacement p) {
    DockState* s = GroupState(g);
    if (!s || !s->toggleButtonVisible || s->zoomPanel >= 0) {
        return false;
    }
    DockSide* side = DockSideOf(s, p);
    if (!side || side->node < 0 || !side->collapsible) {
        return false;
    }
    if (p == DockPlacement::Bottom) {
        // The bottom toggle lives on the bottom Dock's own first group, which
        // is what makes a collapsed bottom dock's strip usable.
        return g->node == DockLeftTopTabs(s, s->bottom.node);
    }
    if (p == DockPlacement::Right) {
        return g->node == DockRightTopTabs(s, s->center);
    }
    return g->node == DockLeftTopTabs(s, s->center);
}

// ---------------------------------------------------------------------------
// What the skin hands back

El* DockBindTab(const DockTabGroup* g, int ix, El* tab) {
    DockState* s = GroupState(g);
    if (!s || !tab) {
        return tab;
    }
    Ctx* cx = g->cx;
    DockNode& n = s->nodes[g->node];
    if (ix < 0 || ix >= n.panel.len) {
        return tab;
    }
    int panelIx = n.panel[ix];
    BindId(tab, DockElId(cx, "tab", g->node, ix), true);
    tab->OnClick(
        ListenTo(g->state, &DockState::OnTabClick, DockPack(g->node, ix)));
    // `when(!droppable, ..)`, where that local is `self.collapsed`: a shut
    // dock's tabs neither pick up nor take a drag, they only click to open it
    // again.
    if (!g->collapsed) {
        // on_drag(DragPanel::new(..)): the tab is what a press picks up, and
        // the moves that follow say where it would land.
        if (DockNodeDraggable(s, g->node)) {
            tab->OnDrag(kDockPanelDrag, panelIx);
            tab->OnDragMove(ListenTo(g->state, &DockState::OnTabDragMove));
            tab->OnMouseUpOut(ListenTo(g->state, &DockState::OnTabDragEnd));
            tab->OnMouseUp(ListenTo(g->state, &DockState::OnTabDragEnd));
        }
        // Every tab is a drop target of its own: a panel let go over one
        // takes that place in the row, which is what makes a tab reorder.
        if (DockNodeDroppable(s, g->node)) {
            tab->OnDrop(kDockPanelDrag,
                        ListenTo(g->state, &DockState::OnDropTab,
                                 DockPack(g->node, ix)));
        }
        if (ix == DockActiveIx(s, g->node)) {
            tab->BoundsOut(&n.activeTabBounds);
            n.activeTabBoundsIx = ix;
        }
    }
    return tab;
}

El* DockBindTabRest(const DockTabGroup* g, El* rest) {
    DockState* s = GroupState(g);
    if (!s || !rest || g->collapsed || !DockNodeDroppable(s, g->node)) {
        return rest;
    }
    BindId(rest, DockElId(g->cx, "tabrest", g->node, 0));
    rest->OnDrop(kDockPanelDrag, ListenTo(g->state, &DockState::OnDropTabBar,
                                          (intptr_t)g->node));
    return rest;
}

El* DockBindTabStrip(const DockTabGroup* g, El* strip) {
    DockState* s = GroupState(g);
    if (!s || !strip) {
        return strip;
    }
    DockNode& n = s->nodes[g->node];
    // The tab just made active is brought into view from where last frame put
    // it, which is scroll_to_item.
    if (n.pendingScrollIx >= 0) {
        if (n.activeTabBoundsIx == n.pendingScrollIx) {
            n.tabScrollX = DockTabScrollTo(n.tabScrollX, n.tabStripBounds,
                                           n.activeTabBounds);
            n.pendingScrollIx = -1;
        } else {
            // Its box is measured by this frame; the scroll is worked out on
            // the next one, which is why one is asked for.
            WindowRequestAnimationFrame(g->cx->win);
        }
    }
    strip->Id(DockElId(g->cx, "tabscroll", g->node, 0))
        ->ScrollX(n.tabScrollX)
        ->ScrollFromPath()
        ->OnScroll(
            ListenTo(g->state, &DockState::OnTabBarScroll, (intptr_t)g->node))
        ->BoundsOut(&n.tabStripBounds);
    return strip;
}

bool DockGroupDroppable(const DockTabGroup* g) {
    DockState* st = GroupState(g);
    return st && !g->collapsed && DockNodeDroppable(st, g->node);
}

El* DockBindTitleDrag(const DockTabGroup* g, int ix, El* e) {
    DockState* s = GroupState(g);
    if (!s || !e || !DockNodeDraggable(s, g->node)) {
        return e;
    }
    DockNode& n = s->nodes[g->node];
    if (ix < 0 || ix >= n.panel.len) {
        return e;
    }
    e->Id(DockElId(g->cx, "title", g->node, ix))
        ->OnDrag(kDockPanelDrag, n.panel[ix])
        ->OnDragMove(ListenTo(g->state, &DockState::OnTabDragMove))
        ->OnMouseUpOut(ListenTo(g->state, &DockState::OnTabDragEnd))
        ->OnMouseUp(ListenTo(g->state, &DockState::OnTabDragEnd));
    return e;
}

El* DockBindToggle(const DockTabGroup* g, DockPlacement p, El* e) {
    if (!e) {
        return e;
    }
    BindId(e, DockElId(g->cx, "toggle", g->node, (int)p), true);
    e->OnClick(ListenTo(g->state, &DockState::OnToggleSide, (intptr_t)p));
    // tab_panel.rs marks every tool on the bar `.tab_stop(false)`: the panel
    // is what Tab moves between, not the buttons hung off its edge.
    e->TabStop(false);
    return e;
}

El* DockBindZoom(const DockTabGroup* g, int panelIx, El* e) {
    if (!e) {
        return e;
    }
    BindId(e, DockElId(g->cx, "zoom", g->node, panelIx), true);
    e->OnClick(ListenTo(g->state, &DockState::OnZoomClick, (intptr_t)panelIx));
    e->TabStop(false);
    return e;
}

El* DockBindClose(const DockTabGroup* g, int ix, El* e) {
    if (!e) {
        return e;
    }
    BindId(e, DockElId(g->cx, "close", g->node, ix), true);
    e->OnClick(
        ListenTo(g->state, &DockState::OnCloseClick, DockPack(g->node, ix)));
    // The X sits inside the tab, which is listening for the same click now
    // that a click bubbles: closing a panel must not also select it first.
    e->TabStop(false)->StopClick();
    return e;
}

El* DockBindResizeStrip(const DockCtx* d, El* e) {
    if (!e) {
        return e;
    }
    BindId(e, DockElId(d->cx, "dockresize", (int)d->placement, 0));
    e->Cursor(d->placement == DockPlacement::Bottom ? CursorKind::RowResize
                                                    : CursorKind::ColResize);
    e->OnDrag(kDockResizeDrag,
              (int)DockPack(kDockSideBase + (int)d->placement, 0));
    e->OnDragMove(ListenTo(d->state, &DockState::OnResizeDrag));
    e->OnMouseUpOut(ListenTo(d->state, &DockState::OnResizeEnd));
    e->OnMouseUp(ListenTo(d->state, &DockState::OnResizeEnd));
    return e;
}

void DockGroupOpenMenu(const DockTabGroup* g, bool open) {
    DockState* s = GroupState(g);
    if (s && open) {
        s->menuNode = g->node;
    }
}

// ---------------------------------------------------------------------------
// The area

namespace {

struct AreaCtx {
    Ctx* cx = nullptr;
    Str id = {};
    Entity<DockState> state = {};
    DockState* s = nullptr;
    const DockRenderer* r = nullptr;
};

El* RenderNode(const AreaCtx& ac, int node);

// resize_handle: the grab between two panels of a split. Base sizes it, gives
// it a cursor and drags it; `render_split_handle` is the paint inside.
El* SplitHandle(const AreaCtx& ac, int node, int ix, Axis axis) {
    Ctx* cx = ac.cx;
    Arena* a = cx->a;
    bool horizontal = AxisIsHorizontal(axis);
    El* e = Div(a)->FlexRow()->ItemsCenter()->JustifyCenter()->Shrink0();
    if (horizontal) {
        e->W(kDockHandleW)->H(kFill)->Cursor(CursorKind::ColResize);
    } else {
        e->H(kDockHandleW)->W(kFill)->Cursor(CursorKind::RowResize);
    }
    Str hid = DockElId(cx, "split", node, ix);
    BindId(e, hid);
    // `.group("handle")`: what the line inside asks about is the pointer
    // being in the grab area around it, not the line being the hovered
    // element -- the two differ by the four DIPs of padding either side.
    e->Group();
    // Whether this is the handle being dragged is the handle's own state,
    // kept where Rust keeps it. The group's resize listener goes through it,
    // since the port's element carries one listener per event.
    Entity<ResizeHandleState> hs = ResizeHandleStateFor(cx, hid);
    ResizeHandleState* hst = hs.Get(cx);
    if (hst) {
        hst->nextUp = ListenTo(ac.state, &DockState::OnResizeEnd);
    }
    e->OnDrag(kDockResizeDrag, (int)DockPack(node, ix));
    e->OnDragMove(ListenTo(ac.state, &DockState::OnResizeDrag));
    e->OnMouseDown(ListenTo(hs, &ResizeHandleState::OnDown));
    e->OnMouseUpOut(ListenTo(hs, &ResizeHandleState::OnUp));
    e->OnMouseUp(ListenTo(hs, &ResizeHandleState::OnUp));
    DockHandleCtx h;
    h.axis = axis;
    h.active = hst && hst->active;
    if (ac.r->splitHandle) {
        if (El* paint = ac.r->splitHandle(cx, ac.r->data, &h)) {
            e->Child(paint);
        }
    }
    return e;
}

// TabPanel::render. The bar the skin draws, the active panel under it, and,
// while a tab is being dragged over this group, the half of the box the drop
// would take.
El* RenderTabs(const AreaCtx& ac, int node) {
    Ctx* cx = ac.cx;
    Arena* a = cx->a;
    DockState* s = ac.s;
    DockNode& n = s->nodes[node];

    // TabPanel::collapsed: a Dock that is shut keeps its tab bar and nothing
    // else. Clicking a tab in it is what opens the Dock again.
    DockSide* side = DockSideOf(s, DockPlacementOfNode(s, node));
    DockTabGroup g;
    g.cx = cx;
    g.state = ac.state;
    g.id = ac.id;
    g.node = node;
    g.collapsed = side && !side->open;

    El* box =
        ac.r->tabGroupFrame ? ac.r->tabGroupFrame(cx, ac.r->data, &g) : nullptr;
    if (!box) {
        box = Div(a);
    }
    box->FlexCol()->SizeFull();
    // The group is its own drop target, and its box is what decides which of
    // the five zones a drop landed in — so the box has to be reported back.
    box->BoundsOut(&n.bounds);
    box->OnDrop(kDockPanelDrag,
                ListenTo(ac.state, &DockState::OnDropPanel, (intptr_t)node));

    if (ac.r->tabBar) {
        if (El* bar = ac.r->tabBar(cx, ac.r->data, &g)) {
            box->Child(bar->Shrink0());
        }
    }

    if (!g.collapsed) {
        El* body = ac.r->tabContentFrame
                       ? ac.r->tabContentFrame(cx, ac.r->data, &g)
                       : nullptr;
        if (!body) {
            body = Div(a);
        }
        body->FlexCol()->Flex1()->W(kFill);
        int activeIx = DockActiveIx(s, node);
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
    if (s->dropNode == node && ac.r->dropIndicator) {
        Bounds ph = DockDropPlaceholder(n.bounds, s->dropAt);
        Spring spring = SpringNew(200.f);
        spring.epsilon = 0.5f;
        uint32_t kx = MotionId(ac.id, StrL("drop-x"));
        uint32_t ky = MotionId(ac.id, StrL("drop-y"));
        uint32_t kw = MotionId(ac.id, StrL("drop-w"));
        uint32_t kh = MotionId(ac.id, StrL("drop-h"));
        if (s->dropFromPending) {
            s->dropFromPending = false;
            SpringSeed(cx, kx, s->dropFrom.x);
            SpringSeed(cx, ky, s->dropFrom.y);
            SpringSeed(cx, kw, s->dropFrom.w);
            SpringSeed(cx, kh, s->dropFrom.h);
        }
        Bounds to;
        to.x = SpringValue(cx, kx, ph.x, spring) - n.bounds.x;
        to.y = SpringValue(cx, ky, ph.y, spring) - n.bounds.y;
        to.w = SpringValue(cx, kw, ph.w, spring);
        to.h = SpringValue(cx, kh, ph.h, spring);
        if (El* mark = ac.r->dropIndicator(cx, ac.r->data, to)) {
            box->Child(
                mark->Absolute()->Left(to.x)->Top(to.y)->W(to.w)->H(to.h));
        }
    }
    return box;
}

// StackPanel: the children along the axis, each at the size the last drag
// left it, with a handle between every pair.
El* RenderSplit(const AreaCtx& ac, int node) {
    Ctx* cx = ac.cx;
    Arena* a = cx->a;
    DockState* s = ac.s;
    DockNode& n = s->nodes[node];
    bool horizontal = AxisIsHorizontal(n.axis);
    El* box = ac.r->splitFrame ? ac.r->splitFrame(cx, ac.r->data, node, n.axis)
                               : nullptr;
    if (!box) {
        box = Div(a);
    }
    box->SizeFull();
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
        wrap->Child(RenderNode(ac, n.child[i]));
        box->Child(wrap);
        // A handle sits between two drawn slots, and there is one only while
        // something is still to come.
        if (i != grows) {
            box->Child(SplitHandle(ac, node, i, n.axis));
        }
    }
    return box;
}

El* RenderNode(const AreaCtx& ac, int node) {
    DockState* s = ac.s;
    if (node < 0 || node >= s->nodes.len || !s->nodes[node].used) {
        return Div(ac.cx->a)->SizeFull();
    }
    if (s->nodes[node].split) {
        return RenderSplit(ac, node);
    }
    return RenderTabs(ac, node);
}

// TabPanel::render_drag_panel: what follows the pointer while a tab is being
// dragged. Rust hands GPUI a view to render as the drag's own; here the skin
// draws it and base puts it over everything, at the point the press was
// inside the tab so the label sits where it was picked up from.
El* RenderDragPreview(const AreaCtx& ac) {
    Ctx* cx = ac.cx;
    const DragPayload* drag = WindowActiveDrag(cx);
    if (!drag || !StrSame(drag->kind, kDockPanelDrag) || !ac.r->dragPreview) {
        return nullptr;
    }
    int panelIx = drag->ix;
    if (panelIx < 0 || panelIx >= ac.s->panels.len) {
        return nullptr;
    }
    El* preview = ac.r->dragPreview(cx, ac.r->data, &ac.s->panels[panelIx]);
    if (!preview) {
        return nullptr;
    }
    Point off = WindowDragOffset(cx);
    return preview->Fixed()
        ->Left(cx->win->mouseX - off.x)
        ->Top(cx->win->mouseY - off.y)
        ->Deferred();
}

// One Dock, and the strip on its inner edge. `render_dock` is the whole box:
// a skin that answers null for a shut Dock takes it out of the layout, which
// is what upstream's showcase does.
El* RenderDock(const AreaCtx& ac, DockPlacement p, const DockSide& side) {
    if (side.node < 0 || !ac.r->dock) {
        return nullptr;
    }
    DockCtx d;
    d.cx = ac.cx;
    d.state = ac.state;
    d.id = ac.id;
    d.placement = p;
    d.size = side.size;
    d.open = side.open;
    return ac.r->dock(ac.cx, ac.r->data, &d, RenderNode(ac, side.node));
}

} // namespace

El* DockArea::New(Ctx* cx, Str id, Entity<DockState> state,
                  const DockRenderer* r) {
    Arena* a = cx->a;
    // The area's name, on the stack and on the tree, while everything under
    // it is built: a tab, a handle and a group's menu are all named among the
    // area's parts, and two areas on one page are still two areas.
    IdScope scope(cx, id);
    static const DockRenderer kBare;
    AreaCtx ac;
    ac.cx = cx;
    ac.id = id;
    ac.state = state;
    ac.s = state.Get(cx);
    ac.r = r ? r : &kBare;
    if (!ac.s) {
        return Div(a)->SizeFull();
    }
    DockState* s = ac.s;

    El* box = ac.r->frame ? ac.r->frame(cx, ac.r->data) : nullptr;
    if (!box) {
        box = Div(a);
    }
    box->Id(id)->FlexCol()->SizeFull();
    box->BoundsOut(&s->bounds);

    // ToggleZoom: one panel over the whole area, and nothing else rendered.
    if (s->zoomPanel >= 0) {
        int node = DockNodeOfPanel(s, s->zoomPanel);
        if (node >= 0) {
            box->Child(RenderTabs(ac, node));
            return box;
        }
        s->zoomPanel = -1;
    }

    El* row = Div(a)->FlexRow()->Flex1()->W(kFill);
    if (El* left = RenderDock(ac, DockPlacement::Left, s->left)) {
        row->Child(left);
    }
    El* center =
        ac.r->centerFrame ? ac.r->centerFrame(cx, ac.r->data) : nullptr;
    if (!center) {
        center = Div(a);
    }
    row->Child(
        center->FlexCol()->Flex1()->H(kFill)->Child(RenderNode(ac, s->center)));
    if (El* right = RenderDock(ac, DockPlacement::Right, s->right)) {
        row->Child(right);
    }
    box->Child(row);
    // The dragged tab's preview goes over everything, which is what a
    // deferred child is for.
    if (El* preview = RenderDragPreview(ac)) {
        box->Child(preview);
    }
    if (El* bottom = RenderDock(ac, DockPlacement::Bottom, s->bottom)) {
        box->Child(bottom);
    }
    return box;
}

} // namespace gpui

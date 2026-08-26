#include "base/resizable.h"

namespace gpui {

static float PanelMin(const float* mins, int ix) {
    return mins ? mins[ix] : kResizablePanelMinSize;
}

static float PanelMax(const float* maxs, int ix) {
    // Rust's range ends at Pixels::MAX when a panel names no ceiling.
    return maxs ? maxs[ix] : 1e9f;
}

bool ResizablePanelResize(float* sizes, const float* mins, const float* maxs,
                          int n, int ix, float size, float containerSize) {
    // The handle sits between ix and ix + 1, so the last panel has none.
    if (n <= 1 || ix < 0 || ix >= n - 1) {
        return false;
    }
    float moved = size - sizes[ix];
    if (moved == 0) {
        return false;
    }
    float lo = PanelMin(mins, ix);
    float hi = PanelMax(maxs, ix);
    float newSize = size < lo ? lo : (size > hi ? hi : size);
    int mainIx = ix;
    float old = sizes[ix];

    if (moved > 0) {
        // Growing: the panels after it give up what they can spare, nearest
        // first, each stopping at its own minimum.
        float changed = newSize - sizes[ix];
        sizes[ix] = newSize;
        int i = ix;
        while (changed > 0 && i < n - 1) {
            i++;
            float spare = sizes[i] - PanelMin(mins, i);
            if (spare < 0) {
                spare = 0;
            }
            float take = changed < spare ? changed : spare;
            sizes[i] -= take;
            changed -= take;
        }
    } else {
        // Shrinking. Rust measures what is left to give from the requested
        // size rather than the clamped one, so a request below the minimum
        // stops there and the remainder is not handed on.
        float changed = newSize - size;
        sizes[ix] = newSize;
        int i = ix;
        while (changed > 0 && i > 0) {
            i--;
            float spare = sizes[i] - PanelMin(mins, i);
            if (spare < 0) {
                spare = 0;
            }
            float take = changed < spare ? changed : spare;
            changed -= take;
            sizes[i] -= take;
        }
        sizes[mainIx + 1] += old - size - changed;
    }

    float total = 0;
    for (int i = 0; i < n; i++) {
        total += sizes[i];
    }
    if (total > containerSize) {
        float overflow = total - containerSize;
        float shrunk = sizes[mainIx] - overflow;
        sizes[mainIx] = shrunk < lo ? lo : shrunk;
    }
    return true;
}

void ResizableAdjustToContainer(float* sizes, int n, float containerSize) {
    if (containerSize <= 0) {
        return;
    }
    float total = 0;
    for (int i = 0; i < n; i++) {
        total += sizes[i];
    }
    if (total <= 0) {
        return;
    }
    for (int i = 0; i < n; i++) {
        sizes[i] = containerSize * (sizes[i] / total);
    }
}

// The name a resize drag goes by, which is `DragPanel` in Rust.
static const Str kResizeDrag = StrL("resizable-handle");

float ResizablePanelSize(const ResizableState* s, int ix, float declared) {
    if (!s || ix < 0 || ix >= s->sizes.len || s->sizes[ix] <= 0) {
        return declared;
    }
    return s->sizes[ix];
}

void ResizableState::OnHandleDown(ResizableState* self, Ctx* cx,
                                  const MouseDownEvent* ev, intptr_t ix) {
    if (ev->button != MouseButton::Left) {
        return;
    }
    self->dragging = (int)ix;
    Notify(cx);
}

void ResizableState::OnHandleDrag(ResizableState* self, Ctx* cx,
                                  const DragMoveEvent* ev) {
    int ix = self->dragging;
    if (ix < 0 || ix + 1 >= self->sizes.len) {
        return;
    }
    // A hidden panel is not on the boundary being dragged and must not be
    // given or taken space, so the arithmetic sees the panels that are drawn
    // and nothing else. Rust leaves the hidden slot in its array and lets its
    // number drift; the sizes are compacted here and written back instead.
    const int kMaxPanels = 64;
    float sizes[kMaxPanels] = {};
    float mins[kMaxPanels] = {};
    float maxs[kMaxPanels] = {};
    int back[kMaxPanels] = {};
    int n = 0;
    int at = -1;
    for (int i = 0; i < self->sizes.len && n < kMaxPanels; i++) {
        if (i < self->shown.len && !self->shown[i]) {
            continue;
        }
        sizes[n] = self->sizes[i];
        mins[n] = self->mins[i];
        maxs[n] = self->maxs[i];
        back[n] = i;
        if (i == ix) {
            at = n;
        }
        n++;
    }
    if (at < 0 || at + 1 >= n) {
        return;
    }
    // Where the boundary now is, in the group's own coordinates: the size the
    // panels before it take up plus what the pointer has moved to.
    float before = 0;
    for (int i = 0; i < at; i++) {
        before += sizes[i];
    }
    bool horiz = AxisIsHorizontal(self->axis);
    float pt =
        horiz ? ev->event.x - self->bounds.x : ev->event.y - self->bounds.y;
    float want = pt - before;
    float container = horiz ? self->bounds.w : self->bounds.h;
    if (!ResizablePanelResize(sizes, mins, maxs, n, at, want, container)) {
        return;
    }
    for (int i = 0; i < n; i++) {
        self->sizes[back[i]] = sizes[i];
    }
    Notify(cx);
}

void ResizableState::OnHandleUp(ResizableState* self, Ctx* cx,
                                const MouseUpEvent*) {
    if (self->dragging < 0) {
        return;
    }
    self->dragging = -1;
    // ResizablePanelEvent::Resized, once the boundary has settled.
    if (self->onResized.IsValid()) {
        ClickEvent ev = {};
        ListenerCall(cx->app, cx->win, self->onResized, &ev);
    }
    Notify(cx);
}

Resizable* Resizable::New(Ctx* cx, Str id, Entity<ResizableState> state,
                          Axis axis) {
    Arena* a = cx->a;
    Resizable* r = ArenaNew<Resizable>(a);
    r->a = a;
    r->cx = cx;
    r->id = id;
    // `self.state.unwrap_or(window.use_keyed_state(self.id, .., ResizableState
    // ::default()))`: a group only needs the caller to hold its state when the
    // caller means to drive it -- the programmatic story resizes panels from
    // buttons. Every other group is `h_resizable("id")` and nothing more, and
    // the sizes a drag leaves belong to the element that was dragged.
    r->state = state.IsValid() ? state
                               : ElementStateEntity<ResizableState>(
                                     cx, id, StrL("gpui::ResizableState"));
    if (ResizableState* s = r->state.Get(cx)) {
        s->axis = axis;
    }
    return r;
}

Resizable* Resizable::W(float v) {
    width = v;
    return this;
}
Resizable* Resizable::H(float v) {
    height = v;
    return this;
}
void ResizeHandleState::OnDown(ResizeHandleState* self, Ctx* cx,
                               const MouseDownEvent* ev) {
    // `if bounds.contains(&ev.position)`: the listener is the element's, so
    // being called is already the answer to that.
    self->active = true;
    if (self->nextDown.IsValid()) {
        ListenerCall(cx->app, cx->win, self->nextDown, ev);
    }
    Notify(cx);
}

void ResizeHandleState::OnUp(ResizeHandleState* self, Ctx* cx,
                             const MouseUpEvent* ev) {
    // Any release ends it, whether or not it landed on the handle.
    self->active = false;
    if (self->nextUp.IsValid()) {
        ListenerCall(cx->app, cx->win, self->nextUp, ev);
    }
    Notify(cx);
}

Entity<ResizeHandleState> ResizeHandleStateFor(Ctx* cx, Str name) {
    return ElementStateEntity<ResizeHandleState>(
        cx, name, StrL("gpui::ResizeHandleState"));
}

Resizable* Resizable::HandleColors(Rgba rest, Rgba dragging) {
    handleColor = rest;
    handleDragColor = dragging;
    return this;
}

Resizable* Resizable::Panel(El* content, float size, float min, float max) {
    panels.Append(a, content);
    sizes.Append(a, size);
    mins.Append(a, min);
    maxs.Append(a, max);
    grows.Append(a, false);
    shown.Append(a, true);
    return this;
}

Resizable* Resizable::Grow(El* content, float min) {
    Panel(content, 0, min, 0);
    return Flex();
}

Resizable* Resizable::Flex() {
    if (grows.len > 0) {
        grows[grows.len - 1] = true;
    }
    return this;
}

Resizable* Resizable::Visible(bool v) {
    if (shown.len > 0) {
        shown[shown.len - 1] = v;
    }
    return this;
}

El* Resizable::IntoEl() {
    ResizableState* s = state.Get(cx);
    bool horiz = !s || AxisIsHorizontal(s->axis);
    // The group's name, on the stack while its panels and handles are built.
    // Nesting one group inside another is the ordinary case here, and without
    // this both groups' `resizable-handle-0` would be one element state.
    IdScope scope(cx, id);
    El* root = Div(a)->Id(id)->W(width)->H(height);
    root->FlexRow();
    if (!horiz) {
        root->FlexCol();
    }
    if (!s) {
        for (El* panel : panels) {
            root->Child(panel);
        }
        return root;
    }

    // The declared sizes are the state's until a drag has moved one, and the
    // count is the caller's: a page that changes how many panels it has gets
    // what it declared rather than the old group's numbers.
    if (s->sizes.len != panels.len) {
        s->sizes.Clear();
        s->mins.Clear();
        s->maxs.Clear();
        for (int i = 0; i < panels.len; i++) {
            // A panel that flexes has no size of its own until the container
            // is known: what it declared is its flex basis, and the share it
            // takes is worked out below.
            s->sizes.Append(grows[i] ? 0 : sizes[i]);
            s->mins.Append(mins[i]);
            // A declared 0 is Rust's `Pixels::MAX` — no ceiling — and the
            // arithmetic takes a number, not a flag.
            s->maxs.Append(maxs[i] > 0 ? maxs[i] : 1e9f);
        }
        s->lastContainer = 0;
    } else {
        for (int i = 0; i < panels.len; i++) {
            s->mins[i] = mins[i];
            s->maxs[i] = maxs[i] > 0 ? maxs[i] : 1e9f;
        }
    }
    s->grows.Clear();
    s->shown.Clear();
    for (int i = 0; i < panels.len; i++) {
        s->grows.Append(grows[i]);
        s->shown.Append(shown[i]);
    }
    while (s->laid.len < panels.len) {
        s->laid.Append(Bounds{});
    }

    // The first frame is the layout's answer rather than this code's: a
    // panel whose size the state does not know yet is declared the way Rust
    // declares it — `size_full`, `flex_grow: 1`, the `size_range` as the min
    // and the max, and the declared size as the flex basis — and taffy
    // resolves the line. What it measured is written back here on the next
    // frame and is the panel's size from then on, which is what
    // `update_panel_size` does from Rust's own prepaint. Two things fall out
    // of it that arithmetic here would have had to invent: a sized panel that
    // flexes shrinks with its neighbours instead of holding its number, and a
    // growing panel's `width: 100%` is what makes it the one that gives way.
    float container = horiz ? s->bounds.w : s->bounds.h;
    bool resolved = true;
    for (int i = 0; i < panels.len; i++) {
        if (s->sizes[i] <= 0 && shown[i] && i < s->laid.len) {
            float was = horiz ? s->laid[i].w : s->laid[i].h;
            if (was > 0) {
                s->sizes[i] = was;
            }
        }
        resolved = resolved && (s->sizes[i] > 0 || !shown[i]);
    }
    // adjust_to_container_size: every panel keeps the share it had when the
    // container changes size. A group that is still waiting for the layout to
    // answer has nothing to keep the share of.
    if (resolved && container > 0 && s->lastContainer > 0 &&
        container != s->lastContainer) {
        ResizableAdjustToContainer(s->sizes.els, s->sizes.len, container);
    }
    if (resolved && container > 0) {
        s->lastContainer = container;
    }
    root->BoundsOut(&s->bounds);

    Listener down = ListenTo(state, &ResizableState::OnHandleDown, 0);
    Listener drag = ListenTo(state, &ResizableState::OnHandleDrag);
    Listener up = ListenTo(state, &ResizableState::OnHandleUp);
    for (int i = 0; i < panels.len; i++) {
        // `visible(false)` draws nothing at all — Rust's panel renders a bare
        // div, which takes no room and carries no handle.
        if (!shown[i]) {
            continue;
        }
        // No clip: the handle straddles the boundary, four DIPs either side,
        // which is where Rust puts it and what a clip would cut off.
        // `div().id(("resizable-panel", panel_ix))`: the panel names itself,
        // which is what the handle drawn inside it folds under.
        El* box = Div(a)
                      ->Id(StrDup(a, fmt("resizable-panel-%d", i)))
                      ->FlexCol()
                      ->Shrink0();
        // What the handle is measured against: the panel's own size along
        // the axis. Across it the handle fills the panel — the group's
        // measured box would do as well, but not on the frame that measures
        // it, and a handle with no height on the first frame cannot be
        // grabbed until something else happens to redraw the page.
        float boxW = horiz ? s->sizes[i] : kFill;
        float boxH = horiz ? kFill : s->sizes[i];
        if (s->sizes[i] <= 0) {
            // Rust's own declaration, for the frames before the layout has
            // answered: `flex().flex_grow_1().size_full()`, then `flex_none()`
            // where the caller cancelled the growth, then the size range, then
            // the declared size as the basis.
            box->SizeFull();
            if (grows[i]) {
                box->Grow(1)->Shrink(1);
            } else {
                box->FlexNone();
            }
            if (horiz) {
                box->MinW(mins[i])->MaxW(maxs[i] > 0 ? maxs[i] : 1e9f);
            } else {
                box->MinH(mins[i])->MaxH(maxs[i] > 0 ? maxs[i] : 1e9f);
            }
            if (sizes[i] > 0) {
                box->Basis(sizes[i]);
            }
            if (i < s->laid.len) {
                box->BoundsOut(&s->laid[i]);
            }
        } else if (horiz) {
            box->W(s->sizes[i])->H(kFill);
        } else {
            box->H(s->sizes[i])->W(kFill);
        }
        if (panels[i]) {
            box->Child(panels[i]);
        }
        // The handle sits over the boundary rather than taking room from it:
        // a hairline with four DIPs of grab either side, absolutely placed on
        // the panel's trailing edge. The last panel has no boundary after it,
        // and neither has the last one that is drawn — Rust draws the handle
        // from the panel *after* the boundary, so hiding a panel takes the
        // handle before it away.
        bool hasNext = false;
        for (int j = i + 1; j < panels.len; j++) {
            hasNext = hasNext || shown[j];
        }
        if (hasNext) {
            // Whether this handle is the one being dragged is the handle's
            // own state, kept where Rust keeps it: `with_element_state` under
            // the handle's name. The group's `dragging` is what the resize
            // arithmetic needs, which is a different question.
            Str hid = StrDup(a, fmt("resizable-handle-%d", i));
            Entity<ResizeHandleState> hs = ResizeHandleStateFor(cx, hid);
            ResizeHandleState* h = hs.Get(cx);
            bool active = h && h->active;
            if (h) {
                h->nextDown = ListenerArg(down, i);
                h->nextUp = up;
            }
            El* line = Div(a)->Bg(active ? handleDragColor : handleColor);
            El* handle =
                Div(a)
                    ->Absolute()
                    // `resize_handle(("resizable-handle", ix), axis)`, drawn
                    // from inside the panel it follows.
                    ->PathClick(hid)
                    ->OnMouseDown(ListenTo(hs, &ResizeHandleState::OnDown))
                    ->OnDrag(kResizeDrag, i)
                    ->OnDragMove(drag)
                    ->OnMouseUp(ListenTo(hs, &ResizeHandleState::OnUp))
                    ->OnMouseUpOut(ListenTo(hs, &ResizeHandleState::OnUp));
            // Placed by its leading edge rather than its trailing one: the
            // panel's own size is what the boundary is, and an offset from
            // the near edge is the one an absolute box takes everywhere here.
            float span = horiz ? boxW : boxH;
            float at = span - kResizeHandlePadding;
            if (horiz) {
                handle->Cursor(CursorKind::ColResize)
                    ->Top(0)
                    ->Left(at)
                    ->W(kResizeHandleSize + kResizeHandlePadding * 2)
                    ->H(boxH)
                    ->JustifyCenter();
                line->W(kResizeHandleSize)->H(kFill);
            } else {
                handle->Cursor(CursorKind::RowResize)
                    ->Left(0)
                    ->Top(at)
                    ->H(kResizeHandleSize + kResizeHandlePadding * 2)
                    ->W(boxW)
                    ->ItemsCenter();
                line->H(kResizeHandleSize)->W(kFill);
            }
            handle->Child(line);
            box->Child(handle);
        }
        root->Child(box);
    }
    return root;
}

El* ResizablePanel::New(Ctx* cx) {
    Arena* a = cx->a;
    return Div(a);
}
} // namespace gpui

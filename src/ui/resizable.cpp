#include "ui/resizable.h"

namespace gpui {

namespace component {

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
    // Where the boundary now is, in the group's own coordinates: the size the
    // panels before it take up plus what the pointer has moved to.
    float before = 0;
    for (int i = 0; i < ix; i++) {
        before += self->sizes[i];
    }
    bool horiz = AxisIsHorizontal(self->axis);
    float at =
        horiz ? ev->event.x - self->bounds.x : ev->event.y - self->bounds.y;
    float want = at - before;
    float container = horiz ? self->bounds.w : self->bounds.h;
    if (!ResizablePanelResize(self->sizes.els, self->mins.els, self->maxs.els,
                              self->sizes.len, ix, want, container)) {
        return;
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
    r->state = state;
    if (ResizableState* s = state.Get(cx)) {
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

Resizable* Resizable::Panel(El* content, float size, float min, float max) {
    panels.Append(a, content);
    sizes.Append(a, size);
    mins.Append(a, min);
    maxs.Append(a, max);
    grows.Append(a, false);
    return this;
}

Resizable* Resizable::Grow(El* content, float min) {
    Panel(content, 0, min, 0);
    grows[grows.len - 1] = true;
    return this;
}

El* Resizable::IntoEl() {
    const Theme& th = cx->theme();
    ResizableState* s = state.Get(cx);
    bool horiz = !s || AxisIsHorizontal(s->axis);
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
            s->sizes.Append(sizes[i]);
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
    s->growIx = -1;
    int growIx = -1;
    for (bool grow : grows) {
        growIx++;
        if (grow) {
            s->growIx = growIx;
            break;
        }
    }

    // adjust_to_container_size: every panel keeps the share it had. Only a
    // group whose panels all have a size of their own is normalised — one
    // with a growing panel gets the leftover from the layout instead, which
    // is what `flex_grow` is for.
    float container = horiz ? s->bounds.w : s->bounds.h;
    if (container > 0 && s->lastContainer > 0 &&
        container != s->lastContainer) {
        ResizableAdjustToContainer(s->sizes.els, s->sizes.len, container);
    }
    // A growing panel has no size of its own until the container is known;
    // once it is, it takes what the others leave and becomes an ordinary
    // panel, which is what makes a drag either side of it arithmetic rather
    // than a special case. Rust's state carries a size for every panel too.
    if (s->growIx >= 0 && container > 0 && s->sizes[s->growIx] <= 0) {
        float rest = 0;
        for (int i = 0; i < s->sizes.len; i++) {
            if (i != s->growIx) {
                rest += s->sizes[i];
            }
        }
        float want = container - rest;
        s->sizes[s->growIx] =
            want > s->mins[s->growIx] ? want : s->mins[s->growIx];
    }
    if (container > 0) {
        s->lastContainer = container;
    }
    root->BoundsOut(&s->bounds);

    Listener down = ListenTo(state, &ResizableState::OnHandleDown, 0);
    Listener drag = ListenTo(state, &ResizableState::OnHandleDrag);
    Listener up = ListenTo(state, &ResizableState::OnHandleUp);
    for (int i = 0; i < panels.len; i++) {
        // No clip: the handle straddles the boundary, four DIPs either side,
        // which is where Rust puts it and what a clip would cut off.
        El* box = Div(a)->FlexCol()->Shrink0();
        // What the handle is measured against: the panel's own size along the
        // axis, and the group's box across it.
        float boxW = horiz ? s->sizes[i] : (s->bounds.w > 0 ? s->bounds.w : 0);
        float boxH = horiz ? (s->bounds.h > 0 ? s->bounds.h : 0) : s->sizes[i];
        if (grows[i] && s->sizes[i] <= 0) {
            // The one frame before the container is known.
            box->Flex1();
            if (horiz) {
                box->H(kFill)->MinW(mins[i]);
            } else {
                box->W(kFill)->MinH(mins[i]);
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
        // the panel's trailing edge. The last panel has no boundary after it.
        if (i + 1 < panels.len) {
            bool active = s->dragging == i;
            El* line = Div(a)->Bg(active ? th.dragBorder : th.border);
            El* handle =
                Div(a)
                    ->Absolute()
                    ->Click(HashClickId(StrDup(a, fmt("%s-handle-%d", id, i))))
                    ->OnMouseDown(ListenerArg(down, i))
                    ->OnDrag(kResizeDrag, i)
                    ->OnDragMove(drag)
                    ->OnMouseUp(up)
                    ->OnMouseUpOut(up);
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

} // namespace component
} // namespace gpui

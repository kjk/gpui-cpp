/* Unstyled resizable — crates/base/src/resizable */

#include "gpui/gpui.h"
#include "base/geometry.h"

namespace gpui {

// PANEL_MIN_SIZE. A panel never shrinks below this unless its own range says
// otherwise.
const float kResizablePanelMinSize = 100.f;

// resize_panel_at_handle: set panel `ix` to `size` and settle the rest.
//
// `ix` names the handle between panel ix and ix + 1, so the last panel has
// none and answers false. Growing takes the space from the panels after it,
// one at a time, each down to its own minimum; shrinking gives it back to the
// panel immediately after and pulls from the ones before if that is not
// enough. A total that still overruns the container comes off the panel that
// was dragged, which is Rust's last correction.
//
// `sizes` is read and written in place. `mins` and `maxs` are the per-panel
// range — pass null for either to use kResizablePanelMinSize and no ceiling.
// Answers false when there was nothing to do, which is Rust's early return.
bool ResizablePanelResize(float* sizes, const float* mins, const float* maxs,
                          int n, int ix, float size, float containerSize);

// adjust_to_container_size: every panel keeps the share it had when the
// container changes size.
void ResizableAdjustToContainer(float* sizes, int n, float containerSize);

// HANDLE_SIZE and HANDLE_PADDING: a hairline with four DIPs of grab either
// side of it, sitting over the boundary rather than taking room from it.
const float kResizeHandleSize = 1.f;
const float kResizeHandlePadding = 4.f;

// ResizeHandleState. Rust's resize handle is an Element of its own and keeps
// this in `window.with_element_state`: one flag, set from the press inside
// its bounds and cleared by any release. It is the handle's own and not the
// group's, which is why a handle does not have to be told which of the
// group's boundaries it is in order to know whether it is the one being
// dragged.
struct ResizeHandleState {
    bool active = false;
    // GPUI's `window.on_mouse_event` registers a listener; the port's element
    // carries one per event, and the handle's own answer is not the only one
    // the press has to reach. So the group's goes through here, which is what
    // Rust's closure does anyway once it has set the flag.
    Listener nextDown;
    Listener nextUp;

    static void OnDown(ResizeHandleState* self, Ctx* cx,
                       const MouseDownEvent* ev);
    static void OnUp(ResizeHandleState* self, Ctx* cx, const MouseUpEvent* ev);
};

// `with_element_state` for one handle, named among the parts of whatever it
// is being built inside.
Entity<ResizeHandleState> ResizeHandleStateFor(Ctx* cx, Str name);

// ResizableState. Rust keeps the sizes, the per-panel range and the axis on
// the state and re-derives a panel's size from it every frame; so does this.
// The panels are declared by the caller each frame, which is what fills the
// three arrays in — a page that changes how many panels it has gets the sizes
// it declared, and one that does not keeps what the drags left.
struct ResizableState {
    Axis axis = Axis::Horizontal;
    // One entry per panel, in the order they are declared.
    Vec<float> sizes;
    Vec<float> mins;
    Vec<float> maxs;
    // Which panels take a share of what is left over — Rust's panels carry
    // `flex_grow: 1` unless a caller cancels it with `flex_none()`. One entry
    // per panel, like the three above.
    Vec<bool> grows;
    // resizable_panel().visible(false). The slot keeps its place and its size
    // while nothing is drawn for it, so showing the panel again brings back
    // the width a drag left it at.
    Vec<bool> shown;
    // Where the layout put each panel on the last frame. A panel with no size
    // of its own is declared as a flex item and measured, and what comes back
    // is its size from then on — `update_panel_size`, one frame later, since
    // a box reports where it landed after it has been laid out.
    Vec<Bounds> laid;
    // The box the panels lie in, written at paint. `adjust_to_container_size`
    // keeps every panel's share of it when it changes.
    Bounds bounds = {};
    float lastContainer = 0;
    // The handle being dragged, or -1. `ix` is the boundary between panel ix
    // and ix + 1.
    int dragging = -1;
    // ResizablePanelEvent::Resized, once the drag ends.
    Listener onResized = {};

    ~ResizableState() {
        sizes.Reset();
        mins.Reset();
        maxs.Reset();
        grows.Reset();
        shown.Reset();
        laid.Reset();
    }

    static void OnHandleDown(ResizableState* self, Ctx* cx,
                             const MouseDownEvent* ev, intptr_t ix);
    static void OnHandleDrag(ResizableState* self, Ctx* cx,
                             const DragMoveEvent* ev);
    static void OnHandleUp(ResizableState* self, Ctx* cx,
                           const MouseUpEvent* ev);
};

// The size panel `ix` is drawn at: what the state holds, or what the caller
// declared until a drag has moved it.
float ResizablePanelSize(const ResizableState* s, int ix, float declared);

// ResizablePanelGroup — `h_resizable(id)` / `v_resizable(id)` with
// `resizable_panel()` children. The group owns the panels' sizes, the handle
// between each pair and the drag that moves it. Rust keeps the whole of this
// in `crates/base` and has no themed counterpart at all, because the only
// thing a theme has to say about it is what colour the hairline is — which is
// `HandleColors` here, since nothing in this layer may read a theme.
struct Resizable {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<ResizableState> state = {};
    float width = kFill;
    float height = kFill;
    // The hairline over each boundary, at rest and while it is being dragged.
    Rgba handleColor = {};
    Rgba handleDragColor = {};
    // One per panel, in declaration order.
    ArenaVec<El*> panels;
    ArenaVec<float> sizes;
    ArenaVec<float> mins;
    ArenaVec<float> maxs;
    ArenaVec<bool> grows;
    ArenaVec<bool> shown;

    // `h_resizable(id)` / `v_resizable(id)`. The state is optional, as
    // `.state(..)` is upstream: a group left to itself keys its own off the
    // id, and only a caller that means to resize the panels itself -- from a
    // button, rather than from the handle -- has to hold one.
    static Resizable* New(Ctx* cx, Str id, Entity<ResizableState> state = {},
                          Axis axis = Axis::Horizontal);
    Resizable* W(float v);
    Resizable* H(float v);
    Resizable* HandleColors(Rgba rest, Rgba dragging);
    // A panel of a fixed starting size, with the range a drag keeps it in.
    // `max` of 0 is Rust's `Pixels::MAX` — no ceiling.
    Resizable* Panel(El* content, float size,
                     float min = kResizablePanelMinSize, float max = 0);
    // The panel that takes what the others leave: `panel_box(..)` handed to
    // the group as a plain child, which becomes a panel with no size of its
    // own.
    Resizable* Grow(El* content, float min = kResizablePanelMinSize);
    // The panel last declared keeps its size *and* takes a share of the
    // slack — a `resizable_panel()` that never called `flex_none()`, whose
    // internal `flex_grow: 1` stands.
    Resizable* Flex();
    // resizable_panel().visible(v), on the panel last declared. A hidden
    // panel draws nothing, has no handle on the boundary before it, and is
    // left out of the arithmetic — but keeps its slot and its size.
    Resizable* Visible(bool v);
    El* IntoEl();
};

struct ResizablePanel {
    static El* New(Ctx* cx);
};
} // namespace gpui

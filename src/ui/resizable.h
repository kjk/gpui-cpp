/* Themed resizable panels — crates/base/src/resizable + crates/ui

   Rust's `ResizableState` owns the panels' sizes and the axis they lie along;
   the panels themselves are elements, and a handle between two of them drags
   the boundary. The arithmetic — `resize_panel_at_handle` and
   `adjust_to_container_size` — is `base/resizable.h` and was already here with
   only the dock calling it. This is the other half: the state a page holds
   between frames, and the element that draws the panels and their handles. */

#include "base/resizable.h"
#include "ui/sizing.h"

namespace gpui {

namespace component {

// HANDLE_SIZE and HANDLE_PADDING: a hairline with four DIPs of grab either
// side of it, sitting over the boundary rather than taking room from it.
const float kResizeHandleSize = 1.f;
const float kResizeHandlePadding = 4.f;

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

struct Resizable {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<ResizableState> state = {};
    float width = kFill;
    float height = kFill;
    // One per panel, in declaration order.
    ArenaVec<El*> panels;
    ArenaVec<float> sizes;
    ArenaVec<float> mins;
    ArenaVec<float> maxs;
    ArenaVec<bool> grows;
    ArenaVec<bool> shown;

    static Resizable* New(Ctx* cx, Str id, Entity<ResizableState> state,
                          Axis axis = Axis::Horizontal);
    Resizable* W(float v);
    Resizable* H(float v);
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

} // namespace component
} // namespace gpui

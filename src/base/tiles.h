/* Unstyled tiles — crates/ui/src/dock/tiles.rs

   A tile is a panel that floats over the tiles area: it can be moved by its
   drag bar and resized by its edges, and both are magnetic — an edge close to
   a neighbour's edge, or to the top or left of the area, snaps flush to it,
   and an edge close to nothing rounds to the grid instead. The panels are the
   caller's; what is here is where each one sits, which one is being moved or
   resized, and the history that undoes it. */

#include "gpui/gpui.h"

namespace gpui {

// MINIMUM_SIZE: a tile never resizes smaller than this.
const float kTileMinW = 100.f;
const float kTileMinH = 100.f;
// DRAG_BAR_HEIGHT, the strip along the top of a tile that moves it.
const float kTileDragBarH = 30.f;
// HANDLE_SIZE, how wide the grab strip along each edge is.
const float kTileHandleSize = 5.f;
// Theme::tile_grid_size: both the snap threshold and the grid an unsnapped
// edge rounds to.
const float kTileGridSize = 8.f;
// apply_boundary_constraints keeps this much of a tile on screen when it is
// dragged off the left.
const float kTileKeepVisible = 64.f;

// What a press on a tile picks up: the bar that moves it, or the edge that
// resizes it. `ix` is the tile for both.
extern const Str kTileMoveDrag;
extern const Str kTileResizeDrag;

// ResizeSide. The corner handle moves the right and bottom edges together.
enum class TileSide : uint8_t {
    None,
    Left,
    Right,
    Top,
    Bottom,
    BottomRight
};

const int kMaxTiles = 16;
// How many changes the undo history holds.
const int kMaxTileChanges = 64;

// TileItem: a panel of the caller's, where it sits, and how high it stacks.
struct TileItem {
    // The caller's panel, by index — Rust's Arc<dyn PanelView>.
    int panel = 0;
    Bounds bounds = {};
    int zIndex = 0;
};

// TileChange: one entry of the history. A move or resize carries the bounds
// on both sides; bringing a tile to the front carries the order instead.
struct TileChange {
    int tile = 0;
    bool hasBounds = false;
    Bounds oldBounds = {};
    Bounds newBounds = {};
    bool hasOrder = false;
    int oldOrder = 0;
    int newOrder = 0;
};

struct TilesState {
    TileItem items[kMaxTiles] = {};
    int n = 0;

    // The tile being moved, and where the drag started — the pointer in the
    // area's own coordinates, and the tile's bounds when the press landed.
    int dragging = -1;
    Point dragInitialMouse = {};
    Bounds dragInitialBounds = {};

    // The tile being resized, which of its edges, and the same pair for it.
    int resizing = -1;
    TileSide side = TileSide::None;
    Point resizeInitialMouse = {};
    Bounds resizeInitialBounds = {};

    // Where the tiles area was last painted, so a pointer position in the
    // window can be read in the area's own coordinates.
    Bounds bounds = {};
    // How far the area has been scrolled. A tile can be dragged past the edge
    // of the view, so the area scrolls to whatever the tiles cover — the
    // coordinates the tiles are laid out in are the content's, not the
    // view's.
    float scrollX = 0;
    float scrollY = 0;
    // set_scrollbar_mode: whether the bars are always there or appear on
    // hover. Rust leaves it None and falls back to the theme's.
    ScrollbarMode scrollbarMode = ScrollbarMode::Always;

    // The history, and the flag that keeps an undo from recording itself.
    TileChange changes[kMaxTileChanges] = {};
    int nChange = 0;
    int cursor = 0;
    bool ignoring = false;

    // The press that picks a tile up, by its bar or by one of its edges, and
    // the moves and the release that follow. `ix` on a resize is the tile and
    // the side packed together, which is what the drag payload carries.
    static void OnMoveDown(TilesState* self, Ctx* cx, const MouseDownEvent* ev,
                           intptr_t ix);
    static void OnResizeDown(TilesState* self, Ctx* cx,
                             const MouseDownEvent* ev, intptr_t packed);
    static void OnMoveDrag(TilesState* self, Ctx* cx, const DragMoveEvent* ev);
    static void OnResizeDrag(TilesState* self, Ctx* cx,
                             const DragMoveEvent* ev);
    static void OnDragEnd(TilesState* self, Ctx* cx, const MouseUpEvent* ev);
    static void OnScroll(TilesState* self, Ctx* cx, const ScrollEvent* ev);
};

// The tile and the side, as one number: what a resize handle's drag carries.
inline int TileResizePack(int ix, TileSide side) {
    return ix * 8 + (int)side;
}
inline int TileResizeTile(int packed) {
    return packed / 8;
}
inline TileSide TileResizeSide(int packed) {
    return (TileSide)(packed % 8);
}

// How much room the tiles take between them, which is what the area scrolls
// over: from the leftmost and topmost edge any of them reaches — never past
// the origin — to the furthest right and bottom. Rust folds it out of the
// panels' bounds and hands it to the Scrollbar as its scroll_size.
Size TilesContentSize(const TilesState* s);

// The tiles in the order they paint: by z-index, and by insertion where two
// share one. Writes `n` indices into `out`.
void TilesPaintOrder(const TilesState* s, int* out);

// add_item / remove.
int TilesAdd(TilesState* s, int panel, Bounds bounds);
void TilesRemove(TilesState* s, int ix);
// The index of the tile showing `panel`, or -1.
int TilesIndexOfPanel(const TilesState* s, int panel);

// snap_edge: the nearest candidate strictly inside `threshold`, or no answer.
bool TileSnapEdge(float edge, const float* candidates, int n, float threshold,
                  float* out);
// round_to_nearest_ten_with: the nearest multiple of the grid.
float TileRoundToGrid(float v, float grid);

// compute_resized_bounds. Which edges move is read off which values are
// given, the way Rust reads it off which Options are Some:
//   newX     — the left edge moves, the right edge is pinned
//   newW     — the right edge moves, the left edge is pinned
//   newY     — the top edge moves, the bottom edge is pinned
//   newH     — the bottom edge moves, the top edge is pinned
// A null is Rust's None. `others` are the neighbours whose edges the moving
// edge can snap to.
Bounds TileComputeResizedBounds(Bounds prev, const float* newX,
                                const float* newY, const float* newW,
                                const float* newH, const Bounds* others,
                                int nOthers, float grid);

// calculate_magnetic_snap: where the tile being moved would land on each
// axis, given where the pointer has put it. False means that axis is free.
void TilesMagneticSnap(const TilesState* s, Bounds dragging, int itemIx,
                       float threshold, bool* hasX, float* snapX, bool* hasY,
                       float* snapY);
// apply_boundary_constraints: never above the top, and never so far left that
// less than 64px of the tile is left.
Point TilesConstrainOrigin(const TilesState* s, Point origin);

// The press that starts a move or a resize. `x`, `y` are in the window.
void TilesBeginMove(TilesState* s, int ix, float x, float y);
void TilesBeginResize(TilesState* s, int ix, TileSide side, float x, float y);
// update_position: the pointer moved while a tile is being dragged.
void TilesUpdatePosition(TilesState* s, float x, float y);
// The same for a resize: the edges the side names follow the pointer.
void TilesUpdateResize(TilesState* s, float x, float y);
// on_mouse_up: the final position rounds to the grid, the change goes into
// the history, and nothing is being dragged any more.
void TilesMouseUp(TilesState* s);

// bring_to_front: the tile moves to the end of the list, which is what puts
// it over the others. Answers its new index, or -1 when there was nothing to
// do.
int TilesBringToFront(TilesState* s, int ix);

// The history. Undo puts back the bounds and the order a change moved, redo
// puts them forward again, and neither records itself.
bool TilesCanUndo(const TilesState* s);
bool TilesCanRedo(const TilesState* s);
void TilesUndo(TilesState* s);
void TilesRedo(TilesState* s);

} // namespace gpui

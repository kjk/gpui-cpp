#include "base/tiles.h"

namespace gpui {

const Str kTileMoveDrag = StrL("tile-move");
const Str kTileResizeDrag = StrL("tile-resize");

void TilesPaintOrder(const TilesState* s, int* out) {
    for (int i = 0; i < s->n; i++) {
        out[i] = i;
    }
    // sorted_panels: by z-index, and by the order they were added where two
    // share one. An insertion sort keeps that tie unbroken.
    for (int i = 1; i < s->n; i++) {
        int v = out[i];
        int j = i - 1;
        while (j >= 0 && s->items[out[j]].zIndex > s->items[v].zIndex) {
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = v;
    }
}

int TilesAdd(TilesState* s, int panel, Bounds bounds) {
    if (s->n >= kMaxTiles) {
        return -1;
    }
    TileItem& it = s->items[s->n];
    it = TileItem{};
    it.panel = panel;
    it.bounds = bounds;
    return s->n++;
}

void TilesRemove(TilesState* s, int ix) {
    if (ix < 0 || ix >= s->n) {
        return;
    }
    for (int i = ix; i + 1 < s->n; i++) {
        s->items[i] = s->items[i + 1];
    }
    s->n--;
    s->dragging = -1;
    s->resizing = -1;
}

int TilesIndexOfPanel(const TilesState* s, int panel) {
    for (int i = 0; i < s->n; i++) {
        if (s->items[i].panel == panel) {
            return i;
        }
    }
    return -1;
}

bool TileSnapEdge(float edge, const float* candidates, int n, float threshold,
                  float* out) {
    bool found = false;
    float best = threshold;
    for (int i = 0; i < n; i++) {
        float dist = edge - candidates[i];
        if (dist < 0) {
            dist = -dist;
        }
        if (dist < best) {
            best = dist;
            *out = candidates[i];
            found = true;
        }
    }
    return found;
}

float TileRoundToGrid(float v, float grid) {
    if (grid <= 0) {
        return v;
    }
    float t = v / grid;
    // Rust's f32::round is half away from zero.
    float r = t >= 0 ? (float)(int)(t + 0.5f) : -(float)(int)(-t + 0.5f);
    return r * grid;
}

Bounds TileComputeResizedBounds(Bounds prev, const float* newX,
                                const float* newY, const float* newW,
                                const float* newH, const Bounds* others,
                                int nOthers, float grid) {
    // The edges of the neighbours, which is what a moving edge snaps to.
    float xEdges[kMaxTiles * 2 + 1];
    float yEdges[kMaxTiles * 2 + 1];
    int nx = 0;
    int ny = 0;
    for (int i = 0; i < nOthers && i < kMaxTiles; i++) {
        xEdges[nx++] = others[i].x;
        xEdges[nx++] = others[i].Right();
        yEdges[ny++] = others[i].y;
        yEdges[ny++] = others[i].Bottom();
    }

    float prevRight = prev.x + prev.w;
    float prevBottom = prev.y + prev.h;
    float finalX = prev.x;
    float finalW = prev.w;
    float finalY = prev.y;
    float finalH = prev.h;

    if (newX) {
        // The left edge moves and the right one is pinned; the left of the
        // area is a target too.
        float rawLeft = *newX > 0 ? *newX : 0;
        xEdges[nx] = 0;
        float snapped = 0;
        if (!TileSnapEdge(rawLeft, xEdges, nx + 1, grid, &snapped)) {
            snapped = TileRoundToGrid(rawLeft, grid);
        }
        float w = prevRight - snapped;
        finalX = snapped;
        finalW = w > kTileMinW ? w : kTileMinW;
    } else if (newW) {
        // The right edge moves and the left one is pinned.
        float rawRight = prev.x + *newW;
        float snapped = 0;
        if (!TileSnapEdge(rawRight, xEdges, nx, grid, &snapped)) {
            snapped = TileRoundToGrid(rawRight, grid);
        }
        float w = snapped - prev.x;
        finalW = w > kTileMinW ? w : kTileMinW;
    }

    if (newY) {
        float rawTop = *newY > 0 ? *newY : 0;
        yEdges[ny] = 0;
        float snapped = 0;
        if (!TileSnapEdge(rawTop, yEdges, ny + 1, grid, &snapped)) {
            snapped = TileRoundToGrid(rawTop, grid);
        }
        float h = prevBottom - snapped;
        finalY = snapped;
        finalH = h > kTileMinH ? h : kTileMinH;
    } else if (newH) {
        float rawBottom = prev.y + *newH;
        float snapped = 0;
        if (!TileSnapEdge(rawBottom, yEdges, ny, grid, &snapped)) {
            snapped = TileRoundToGrid(rawBottom, grid);
        }
        float h = snapped - prev.y;
        finalH = h > kTileMinH ? h : kTileMinH;
    }

    return {finalX, finalY, finalW, finalH};
}

void TilesMagneticSnap(const TilesState* s, Bounds dragging, int itemIx,
                       float threshold, bool* hasX, float* snapX, bool* hasY,
                       float* snapY) {
    *hasX = false;
    *hasY = false;
    // Only the neighbours within a threshold of the tile are looked at.
    Bounds search = {dragging.x - threshold, dragging.y - threshold,
                     dragging.w + threshold * 2, dragging.h + threshold * 2};
    float minX = threshold;
    float minY = threshold;

    float dragLeft = dragging.x;
    float dragRight = dragging.Right();
    float dragTop = dragging.y;
    float dragBottom = dragging.Bottom();

    // The top and left of the area come first: a tile near either snaps flush
    // to it whatever its neighbours say.
    float topDist = dragTop < 0 ? -dragTop : dragTop;
    if (topDist < threshold) {
        *hasY = true;
        *snapY = 0;
        minY = topDist;
    }
    float leftDist = dragLeft < 0 ? -dragLeft : dragLeft;
    if (leftDist < threshold) {
        *hasX = true;
        *snapX = 0;
        minX = leftDist;
    }
    if (*hasX && *hasY) {
        return;
    }

    for (int i = 0; i < s->n; i++) {
        if (i == itemIx) {
            continue;
        }
        Bounds o = s->items[i].bounds;
        if (o.Right() < search.x || o.x > search.Right() ||
            o.Bottom() < search.y || o.y > search.Bottom()) {
            continue;
        }
        if (!*hasX) {
            // Either edge of the tile against either edge of the neighbour.
            float dists[4] = {dragLeft - o.x, dragLeft - o.Right(),
                              dragRight - o.x, dragRight - o.Right()};
            float posns[4] = {o.x, o.Right(), o.x - dragging.w,
                              o.Right() - dragging.w};
            for (int k = 0; k < 4; k++) {
                float d = dists[k] < 0 ? -dists[k] : dists[k];
                if (d < minX) {
                    minX = d;
                    *hasX = true;
                    *snapX = posns[k];
                }
            }
        }
        if (!*hasY) {
            float dists[4] = {dragTop - o.y, dragTop - o.Bottom(),
                              dragBottom - o.y, dragBottom - o.Bottom()};
            float posns[4] = {o.y, o.Bottom(), o.y - dragging.h,
                              o.Bottom() - dragging.h};
            for (int k = 0; k < 4; k++) {
                float d = dists[k] < 0 ? -dists[k] : dists[k];
                if (d < minY) {
                    minY = d;
                    *hasY = true;
                    *snapY = posns[k];
                }
            }
        }
        if (*hasX && *hasY) {
            break;
        }
    }
}

Point TilesConstrainOrigin(const TilesState* s, Point origin) {
    if (origin.y < 0) {
        origin.y = 0;
    }
    // A tile can hang off the left, but not so far that there is nothing left
    // to grab.
    float minLeft = -s->dragInitialBounds.w + kTileKeepVisible;
    if (origin.x < minLeft) {
        origin.x = minLeft;
    }
    return origin;
}

// The history: everything past the cursor is dropped, the way a new edit
// drops the redo branch.
static void PushChange(TilesState* s, const TileChange& c) {
    if (s->ignoring) {
        return;
    }
    if (s->cursor >= kMaxTileChanges) {
        // The oldest change goes to make room for this one.
        for (int i = 1; i < kMaxTileChanges; i++) {
            s->changes[i - 1] = s->changes[i];
        }
        s->cursor = kMaxTileChanges - 1;
    }
    s->changes[s->cursor] = c;
    s->cursor++;
    s->nChange = s->cursor;
}

void TilesBeginMove(TilesState* s, int ix, float x, float y) {
    if (ix < 0 || ix >= s->n) {
        return;
    }
    s->dragging = ix;
    s->dragInitialMouse = {x - s->bounds.x, y - s->bounds.y};
    s->dragInitialBounds = s->items[ix].bounds;
}

void TilesBeginResize(TilesState* s, int ix, TileSide side, float x, float y) {
    if (ix < 0 || ix >= s->n || side == TileSide::None) {
        return;
    }
    s->resizing = ix;
    s->side = side;
    s->resizeInitialMouse = {x - s->bounds.x, y - s->bounds.y};
    s->resizeInitialBounds = s->items[ix].bounds;
}

void TilesUpdatePosition(TilesState* s, float x, float y) {
    int ix = s->dragging;
    if (ix < 0 || ix >= s->n) {
        return;
    }
    Bounds previous = s->items[ix].bounds;
    Point adjusted = {x - s->bounds.x, y - s->bounds.y};
    Point origin = {
        s->dragInitialBounds.x + adjusted.x - s->dragInitialMouse.x,
        s->dragInitialBounds.y + adjusted.y - s->dragInitialMouse.y};

    // The snap comes before the boundary, and neither rounds to the grid —
    // the drag itself is smooth, and only the release lands on it.
    Bounds dragging = {origin.x, origin.y, s->dragInitialBounds.w,
                       s->dragInitialBounds.h};
    bool hasX = false;
    bool hasY = false;
    float snapX = 0;
    float snapY = 0;
    TilesMagneticSnap(s, dragging, ix, kTileGridSize, &hasX, &snapX, &hasY,
                      &snapY);
    if (hasX) {
        origin.x = snapX;
    }
    if (hasY) {
        origin.y = snapY;
    }
    origin = TilesConstrainOrigin(s, origin);

    if (origin.x == previous.x && origin.y == previous.y) {
        return;
    }
    s->items[ix].bounds.x = origin.x;
    s->items[ix].bounds.y = origin.y;
    TileChange c;
    c.tile = ix;
    c.hasBounds = true;
    c.oldBounds = previous;
    c.newBounds = s->items[ix].bounds;
    PushChange(s, c);
}

void TilesUpdateResize(TilesState* s, float x, float y) {
    int ix = s->resizing;
    if (ix < 0 || ix >= s->n) {
        return;
    }
    // The neighbours, which are what the moving edge snaps to.
    Bounds others[kMaxTiles];
    int nOthers = 0;
    for (int i = 0; i < s->n; i++) {
        if (i != ix) {
            others[nOthers++] = s->items[i].bounds;
        }
    }

    Point at = {x - s->bounds.x, y - s->bounds.y};
    Bounds init = s->resizeInitialBounds;
    float dx = at.x - s->resizeInitialMouse.x;
    float dy = at.y - s->resizeInitialMouse.y;
    float newX = init.x + dx;
    float newY = init.y + dy;
    float newW = init.w + dx;
    float newH = init.h + dy;
    // Which of the four the side moves, which is what tells
    // compute_resized_bounds what is pinned.
    const float* px = nullptr;
    const float* py = nullptr;
    const float* pw = nullptr;
    const float* ph = nullptr;
    switch (s->side) {
        case TileSide::Left:
            px = &newX;
            break;
        case TileSide::Right:
            pw = &newW;
            break;
        case TileSide::Top:
            py = &newY;
            break;
        case TileSide::Bottom:
            ph = &newH;
            break;
        case TileSide::BottomRight:
            pw = &newW;
            ph = &newH;
            break;
        case TileSide::None:
            return;
    }

    Bounds previous = s->items[ix].bounds;
    Bounds next = TileComputeResizedBounds(previous, px, py, pw, ph, others,
                                           nOthers, kTileGridSize);
    if (next.x == previous.x && next.y == previous.y && next.w == previous.w &&
        next.h == previous.h) {
        return;
    }
    s->items[ix].bounds = next;
    TileChange c;
    c.tile = ix;
    c.hasBounds = true;
    c.oldBounds = previous;
    c.newBounds = next;
    PushChange(s, c);
}

void TilesMouseUp(TilesState* s) {
    if (s->dragging < 0 && s->resizing < 0) {
        return;
    }
    if (s->dragging >= 0 && s->dragging < s->n) {
        int ix = s->dragging;
        Bounds initial = s->dragInitialBounds;
        Bounds current = s->items[ix].bounds;
        // The release is what lands the tile on the grid; the drag itself is
        // free of it.
        Point aligned = {TileRoundToGrid(current.x, kTileGridSize),
                         TileRoundToGrid(current.y, kTileGridSize)};
        if (initial.x != aligned.x || initial.y != aligned.y ||
            initial.w != current.w || initial.h != current.h) {
            s->items[ix].bounds.x = aligned.x;
            s->items[ix].bounds.y = aligned.y;
            TileChange c;
            c.tile = ix;
            c.hasBounds = true;
            c.oldBounds = initial;
            c.newBounds = s->items[ix].bounds;
            PushChange(s, c);
        }
    }
    if (s->resizing >= 0 && s->resizing < s->n) {
        Bounds initial = s->resizeInitialBounds;
        Bounds current = s->items[s->resizing].bounds;
        if (initial.w != current.w || initial.h != current.h) {
            TileChange c;
            c.tile = s->resizing;
            c.hasBounds = true;
            c.oldBounds = initial;
            c.newBounds = current;
            PushChange(s, c);
        }
    }
    s->dragging = -1;
    s->resizing = -1;
    s->side = TileSide::None;
}

int TilesBringToFront(TilesState* s, int ix) {
    if (ix < 0 || ix >= s->n) {
        return -1;
    }
    TileItem item = s->items[ix];
    for (int i = ix; i + 1 < s->n; i++) {
        s->items[i] = s->items[i + 1];
    }
    s->items[s->n - 1] = item;
    int newIx = s->n - 1;
    TileChange c;
    c.tile = newIx;
    c.hasOrder = true;
    c.oldOrder = ix;
    c.newOrder = newIx;
    PushChange(s, c);
    return newIx;
}

bool TilesCanUndo(const TilesState* s) {
    return s->cursor > 0;
}
bool TilesCanRedo(const TilesState* s) {
    return s->cursor < s->nChange;
}

// Move the tile at `from` to `to`, which is what putting an order change back
// comes down to.
static void MoveItem(TilesState* s, int from, int to) {
    if (from < 0 || from >= s->n || to < 0 || to >= s->n || from == to) {
        return;
    }
    TileItem item = s->items[from];
    if (from < to) {
        for (int i = from; i < to; i++) {
            s->items[i] = s->items[i + 1];
        }
    } else {
        for (int i = from; i > to; i--) {
            s->items[i] = s->items[i - 1];
        }
    }
    s->items[to] = item;
}

void TilesUndo(TilesState* s) {
    if (!TilesCanUndo(s)) {
        return;
    }
    s->ignoring = true;
    const TileChange& c = s->changes[--s->cursor];
    if (c.hasBounds && c.tile >= 0 && c.tile < s->n) {
        s->items[c.tile].bounds = c.oldBounds;
    }
    if (c.hasOrder) {
        MoveItem(s, c.newOrder, c.oldOrder);
    }
    s->ignoring = false;
}

void TilesRedo(TilesState* s) {
    if (!TilesCanRedo(s)) {
        return;
    }
    s->ignoring = true;
    const TileChange& c = s->changes[s->cursor++];
    if (c.hasBounds && c.tile >= 0 && c.tile < s->n) {
        s->items[c.tile].bounds = c.newBounds;
    }
    if (c.hasOrder) {
        MoveItem(s, c.oldOrder, c.newOrder);
    }
    s->ignoring = false;
}

void TilesState::OnMoveDown(TilesState* self, Ctx* cx, const MouseDownEvent* ev,
                            intptr_t ix) {
    TilesBeginMove(self, (int)ix, ev->x, ev->y);
    Notify(cx);
}

void TilesState::OnResizeDown(TilesState* self, Ctx* cx,
                              const MouseDownEvent* ev, intptr_t packed) {
    TilesBeginResize(self, TileResizeTile((int)packed),
                     TileResizeSide((int)packed), ev->x, ev->y);
    Notify(cx);
}

void TilesState::OnMoveDrag(TilesState* self, Ctx* cx,
                            const DragMoveEvent* ev) {
    TilesUpdatePosition(self, ev->event.x, ev->event.y);
    Notify(cx);
}

void TilesState::OnResizeDrag(TilesState* self, Ctx* cx,
                              const DragMoveEvent* ev) {
    TilesUpdateResize(self, ev->event.x, ev->event.y);
    Notify(cx);
}

void TilesState::OnDragEnd(TilesState* self, Ctx* cx, const MouseUpEvent* ev) {
    (void)ev;
    // The tile that was moved comes to the front, which is what a click on a
    // window does everywhere.
    int moved = self->dragging;
    TilesMouseUp(self);
    if (moved >= 0) {
        TilesBringToFront(self, moved);
    }
    Notify(cx);
}

} // namespace gpui

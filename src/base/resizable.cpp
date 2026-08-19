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

El* Resizable::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->Id(id);
}
El* ResizablePanel::New(Ctx* cx) {
    Arena* a = cx->a;
    return Div(a);
}
} // namespace gpui

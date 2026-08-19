#include "base/toast.h"

namespace gpui {

bool ToastPush(ToastStackState* s, int id, int timeoutMs) {
    if (s->n >= kToastStackCap) {
        return false;
    }
    ToastEntry e;
    e.id = id;
    e.status = ToastStatus::Starting;
    e.hasTimeout = timeoutMs > 0;
    e.timeoutRemainingMs = timeoutMs;
    e.elapsedMs = 0;
    s->entries[s->n] = e;
    s->n++;
    return true;
}

static void ToastEraseAt(ToastStackState* s, int i) {
    for (int j = i; j < s->n - 1; j++) {
        s->entries[j] = s->entries[j + 1];
    }
    s->n--;
}

bool ToastRemove(ToastStackState* s, int id) {
    for (int i = 0; i < s->n; i++) {
        if (s->entries[i].id == id) {
            ToastEraseAt(s, i);
            return true;
        }
    }
    return false;
}

bool ToastAdvance(ToastStackState* s, int deltaMs, bool paused) {
    bool changed = false;
    for (int i = 0; i < s->n; i++) {
        ToastEntry* e = &s->entries[i];
        switch (e->status) {
            case ToastStatus::Starting:
                e->elapsedMs += deltaMs;
                if (e->elapsedMs >= s->transitionMs) {
                    e->status = ToastStatus::Present;
                    e->elapsedMs = 0;
                    changed = true;
                }
                break;
            case ToastStatus::Present:
                // Only this arm is guarded by `paused`, so a pointer resting
                // on the stack stops the countdown and nothing else.
                if (paused || !e->hasTimeout) {
                    break;
                }
                e->timeoutRemainingMs -= deltaMs;
                if (e->timeoutRemainingMs <= 0) {
                    e->timeoutRemainingMs = 0;
                    e->status = ToastStatus::Ending;
                    e->elapsedMs = 0;
                    changed = true;
                }
                break;
            case ToastStatus::Ending:
                e->elapsedMs += deltaMs;
                break;
        }
    }
    int i = 0;
    while (i < s->n) {
        if (s->entries[i].status == ToastStatus::Ending &&
            s->entries[i].elapsedMs >= s->exitMs) {
            ToastEraseAt(s, i);
            changed = true;
        } else {
            i++;
        }
    }
    return changed;
}

float ToastStackGeometry(const float* heights, int n, float peek, float gap,
                         bool anchoredBottom, float* collapsedOffsets,
                         float* expandedOffsets, float* expandedHeight) {
    if (n <= 0) {
        if (expandedHeight) {
            *expandedHeight = 0;
        }
        return 0;
    }
    float expanded = 0;
    for (int i = 0; i < n; i++) {
        expanded += heights[i];
    }
    expanded += gap * (float)(n - 1);

    // Closed, the stack is as tall as the front toast plus a sliver of each
    // one behind it — or as tall as a taller one behind, whichever wins.
    float collapsed = heights[n - 1] + peek * (float)(n - 1);
    for (int i = 0; i < n; i++) {
        int rank = n - 1 - i;
        float h = heights[i] + peek * (float)rank;
        if (h > collapsed) {
            collapsed = h;
        }
    }

    for (int i = 0; i < n; i++) {
        int rank = n - 1 - i;
        float newer = 0;
        for (int j = i + 1; j < n; j++) {
            newer += heights[j];
        }
        float e = anchoredBottom
                      ? expanded - newer - gap * (float)rank - heights[i]
                      : newer + gap * (float)rank;
        float c = anchoredBottom ? collapsed - heights[i] - peek * (float)rank
                                 : peek * (float)rank;
        if (expandedOffsets) {
            expandedOffsets[i] = e;
        }
        if (collapsedOffsets) {
            collapsedOffsets[i] = c;
        }
    }
    if (expandedHeight) {
        *expandedHeight = expanded;
    }
    return collapsed;
}

El* Toast::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->Id(id);
}
} // namespace gpui

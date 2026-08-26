#include "base/global_state.h"

namespace gpui {

BaseGlobalState* BaseGlobalStateOf(App* app) {
    return AppGlobalEnsure<BaseGlobalState>(app);
}

void BaseGlobalStateInit(App* app) {
    (void)BaseGlobalStateOf(app);
}

void BaseSuppressTextSelection(App* app) {
    if (BaseGlobalState* state = BaseGlobalStateOf(app)) {
        state->suppressTextSelection = true;
    }
}

void BaseResetTextSelectionSuppression(App* app) {
    if (BaseGlobalState* state = BaseGlobalStateOf(app)) {
        state->suppressTextSelection = false;
    }
}

bool BaseIsTextSelectionSuppressed(const App* app) {
    BaseGlobalState* state = AppGlobalGet<BaseGlobalState>(app);
    return state && state->suppressTextSelection;
}

void BaseDeferredPopoverSet(App* app, EntityId popover, bool open) {
    BaseGlobalState* state = BaseGlobalStateOf(app);
    if (!state || !popover.IsValid()) {
        return;
    }
    int found = -1;
    for (int i = 0; i < state->deferredPopovers.len; i++) {
        if (state->deferredPopovers[i] == popover) {
            found = i;
            break;
        }
    }
    if (open && found < 0) {
        state->deferredPopovers.Append(popover);
    } else if (!open && found >= 0) {
        for (int i = found; i < state->deferredPopovers.len - 1; i++) {
            state->deferredPopovers[i] = state->deferredPopovers[i + 1];
        }
        state->deferredPopovers.len--;
    }
}

bool BaseIsInDeferredContext(App* app) {
    BaseGlobalState* state = AppGlobalGet<BaseGlobalState>(app);
    if (!state) {
        return false;
    }
    for (int i = state->deferredPopovers.len - 1; i >= 0; i--) {
        if (EntityGet(app, state->deferredPopovers[i])) {
            continue;
        }
        for (int j = i; j < state->deferredPopovers.len - 1; j++) {
            state->deferredPopovers[j] = state->deferredPopovers[j + 1];
        }
        state->deferredPopovers.len--;
    }
    return state->deferredPopovers.len > 0;
}

} // namespace gpui

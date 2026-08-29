#include "ui/global_state.h"
#include "base/global_state.h"

namespace gpui {
namespace component {

UiGlobalState* UiGlobalStateOf(App* app) {
    return AppGlobalEnsure<UiGlobalState>(app);
}

void UiGlobalStateInit(App* app) {
    // global_state.rs deliberately initializes the re-exported Base global
    // here, before gpui_base::init runs later in ui::init. Keep that legacy
    // seam and let BaseInit's second call be the idempotent one.
    BaseGlobalStateInit(app);
    (void)UiGlobalStateOf(app);
}

void UiSelectionFrameBegin(App* app) {
    if (UiGlobalState* state = UiGlobalStateOf(app)) {
        state->selectionDocumentOrder = 1;
    }
}

uint64_t UiSelectionNextDocumentOrder(App* app) {
    UiGlobalState* state = UiGlobalStateOf(app);
    if (!state) {
        return 0;
    }
    return state->selectionDocumentOrder++;
}

void UiTextViewStatePush(App* app, EntityId view) {
    if (UiGlobalState* state = UiGlobalStateOf(app)) {
        VecAppend(state->textViewStateStack, view);
    }
}

void UiTextViewStatePop(App* app) {
    if (UiGlobalState* state = AppGlobalGet<UiGlobalState>(app)) {
        if (state->textViewStateStack.len > 0) {
            state->textViewStateStack.len--;
        }
    }
}

EntityId UiTextViewStateCurrent(const App* app) {
    UiGlobalState* state = AppGlobalGet<UiGlobalState>(app);
    return state && state->textViewStateStack.len > 0
               ? state->textViewStateStack[state->textViewStateStack.len - 1]
               : EntityId{};
}

} // namespace component
} // namespace gpui

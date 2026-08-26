#include "ui/global_state.h"

namespace gpui {
namespace component {

UiGlobalState* UiGlobalStateOf(App* app) {
    return AppGlobalEnsure<UiGlobalState>(app);
}

void UiGlobalStateInit(App* app) {
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
        state->textViewStateStack.Append(view);
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

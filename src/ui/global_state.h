#ifndef GPUI_UI_GLOBAL_STATE_H_
#define GPUI_UI_GLOBAL_STATE_H_
/* UI-only application global — crates/ui/src/global_state.rs. */

#include "gpui/gpui.h"

namespace gpui {
namespace component {

struct UiGlobalState {
    Vec<EntityId> textViewStateStack;
    uint64_t selectionDocumentOrder = 1;

    ~UiGlobalState() { textViewStateStack.Reset(); }
};

UiGlobalState* UiGlobalStateOf(App* app);
void UiGlobalStateInit(App* app);
void UiSelectionFrameBegin(App* app);
uint64_t UiSelectionNextDocumentOrder(App* app);
void UiTextViewStatePush(App* app, EntityId state);
void UiTextViewStatePop(App* app);
EntityId UiTextViewStateCurrent(const App* app);

} // namespace component
} // namespace gpui
#endif // GPUI_UI_GLOBAL_STATE_H_

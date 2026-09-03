#ifndef GPUI_UI_GLOBAL_STATE_H_
#define GPUI_UI_GLOBAL_STATE_H_
/* UI application global — crates/ui/src/global_state.rs.
 *
 * There is no UI-only global left. The TextView state stack and the
 * selection's document order moved to `gpui_base::GlobalState` with the rich
 * text that reads them, so this file is `pub use gpui_base::GlobalState` plus
 * the legacy initialization point. The Ui* spellings stay as forwarders,
 * because the themed layer's call sites are written in them. */

#include "gpui/gpui.h"
#include "base/global_state.h"

namespace gpui {
namespace component {

using UiGlobalState = gpui::BaseGlobalState;

inline UiGlobalState* UiGlobalStateOf(App* app) {
    return BaseGlobalStateOf(app);
}
void UiGlobalStateInit(App* app);
inline void UiSelectionFrameBegin(App* app) {
    BaseSelectionFrameBegin(app);
}
inline uint64_t UiSelectionNextDocumentOrder(App* app) {
    return BaseSelectionNextDocumentOrder(app);
}
inline void UiTextViewStatePush(App* app, EntityId state) {
    BaseTextViewStatePush(app, state);
}
inline void UiTextViewStatePop(App* app) {
    BaseTextViewStatePop(app);
}
inline EntityId UiTextViewStateCurrent(const App* app) {
    return BaseTextViewStateCurrent(app);
}

} // namespace component
} // namespace gpui
#endif // GPUI_UI_GLOBAL_STATE_H_

#include "ui/global_state.h"

namespace gpui {
namespace component {

void UiGlobalStateInit(App* app) {
    // global_state.rs deliberately initializes the re-exported Base global
    // here, before gpui_base::init runs later in ui::init. Keep that legacy
    // seam and let BaseInit's second call be the idempotent one.
    BaseGlobalStateInit(app);
}

} // namespace component
} // namespace gpui

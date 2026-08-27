#ifndef GPUI_BASE_GLOBAL_STATE_H_
#define GPUI_BASE_GLOBAL_STATE_H_
/* Application-wide Base behavior state — crates/base/src/global_state.rs */

#include "gpui/gpui.h"

namespace gpui {

struct BaseGlobalState {
    Vec<EntityId> deferredPopovers;
    // Rust keeps Vec<OwnedMenu> here so the system menu and the in-window
    // AppMenuBar read one model. MenuDef is the port's owned-menu vocabulary;
    // these pointers all belong to appMenuArena.
    Arena* appMenuArena = nullptr;
    Vec<MenuDef> appMenus;
    bool suppressTextSelection = false;

    ~BaseGlobalState() {
        deferredPopovers.Reset();
        appMenus.Reset();
        if (appMenuArena) {
            ArenaDelete(appMenuArena);
        }
    }
};

// Exact public source names. Base is kept on the implementation name so the
// layer remains unambiguous beside component::UiGlobalState; these aliases
// are what `pub use global_state::{DeferredPopover, GlobalState}` projects.
using GlobalState = BaseGlobalState;
using DeferredPopover = EntityId;

BaseGlobalState* BaseGlobalStateOf(App* app);
void BaseGlobalStateInit(App* app);
void BaseSuppressTextSelection(App* app);
void BaseResetTextSelectionSuppression(App* app);
bool BaseIsTextSelectionSuppressed(const App* app);

// GlobalState::app_menus / set_app_menus. The setter retains a deep copy and
// installs that same copy into the platform menu seam.
const MenuDef* BaseAppMenus(const App* app, int* count);
void BaseSetAppMenus(App* app, const MenuDef* menus, int count);

// A live popover is represented by its generational entity handle. Stale
// handles are swept on read, which is the Rc<Weak<()>> lifetime Rust uses
// without requiring reference counting in this tree.
void BaseDeferredPopoverSet(App* app, EntityId popover, bool open);
DeferredPopover BaseRegisterDeferredPopover(App* app, EntityId popover);
bool BaseIsInDeferredContext(App* app);

} // namespace gpui
#endif // GPUI_BASE_GLOBAL_STATE_H_

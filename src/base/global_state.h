/* Application-wide Base behavior state — crates/base/src/global_state.rs */

#include "gpui/gpui.h"

namespace gpui {

struct BaseGlobalState {
    Vec<EntityId> deferredPopovers;
    bool suppressTextSelection = false;

    ~BaseGlobalState() { deferredPopovers.Reset(); }
};

BaseGlobalState* BaseGlobalStateOf(App* app);
void BaseGlobalStateInit(App* app);
void BaseSuppressTextSelection(App* app);
void BaseResetTextSelectionSuppression(App* app);
bool BaseIsTextSelectionSuppressed(const App* app);

// A live popover is represented by its generational entity handle. Stale
// handles are swept on read, which is the Rc<Weak<()>> lifetime Rust uses
// without requiring reference counting in this tree.
void BaseDeferredPopoverSet(App* app, EntityId popover, bool open);
bool BaseIsInDeferredContext(App* app);

} // namespace gpui

#ifndef GPUI_SHELL_ROOT_H_
#define GPUI_SHELL_ROOT_H_
/* The window-level host for a shell application — crates/shell/src/root.rs. */

#include "shell/view.h"
#include "ui/sheet.h"

namespace gpui {

struct DialogOptions {
    bool escapeDismissable = true;
    bool backdropDismissable = true;

    DialogOptions& EscapeDismissable(bool value) {
        escapeDismissable = value;
        return *this;
    }
    DialogOptions& BackdropDismissable(bool value) {
        backdropDismissable = value;
        return *this;
    }
};

enum class ToastLevel : uint8_t {
    Info,
    Success,
    Warning,
    Error,
};

const char* ToastLevelName(ToastLevel level);
bool ToastLevelFromName(Str name, ToastLevel* out);

struct ToastRequest {
    Str title;
    Str description;
    ToastLevel level = ToastLevel::Info;
    int timeoutMs = 5000;
    bool hasId = false;
    Str id;
};

// ShellRoot is always the first view of a shell window. It owns the script
// content; window-owned dialog, sheet and toast entities are rendered through
// the same layer store used by the native component Root.
struct ShellRoot {
    App* app = nullptr;
    EntityId content = {};
    uint64_t nextToastOrdinal = 0;

    ~ShellRoot();

    static Entity<ShellRoot> New(App* app, EntityId content);
    static El* Render(ShellRoot* self, Ctx* cx);
};

ShellRoot* ShellRootOf(Window* window, App* app);

int ShellRootOpenDialog(Ctx* cx, Entity<ScriptView> content,
                        DialogOptions options = {});
bool ShellRootCloseDialog(Ctx* cx);
int ShellRootCloseAllDialogs(Ctx* cx);
bool ShellRootHasDialog(Ctx* cx);

bool ShellRootOpenSheet(Ctx* cx, Entity<ScriptView> content,
                        component::SheetPlacement placement =
                            component::SheetPlacement::Right);
bool ShellRootCloseSheet(Ctx* cx);
bool ShellRootHasSheet(Ctx* cx);

bool ShellRootPushToast(Ctx* cx, const ToastRequest& toast);
bool ShellRootRemoveToast(Ctx* cx, Str id);
void ShellRootClearToasts(Ctx* cx);
int ShellRootToastCount(Ctx* cx);

} // namespace gpui
#endif // GPUI_SHELL_ROOT_H_

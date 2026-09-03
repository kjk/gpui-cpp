#ifndef GPUI_SHELL_VIEW_H_
#define GPUI_SHELL_VIEW_H_

#include "shell/materialize.h"
#include "base/number_input.h"
#include "base/popover.h"

namespace gpui {

struct ResizablePanelEvent;

struct ShellBoolBinding {
    shell::CallbackId callback = 0;
    bool value = false;
};

struct ShellStringBinding {
    shell::CallbackId callback = 0;
    Str value;
};

struct ShellSelectBinding {
    shell::CallbackId onOpenChange = 0;
    shell::CallbackId onConfirm = 0;
    shell::CallbackId onDismiss = 0;
    bool open = false;
    bool disabled = false;
    FocusHandle triggerFocus = {};
    FocusHandle contentFocus = {};
};

struct ShellNumberBinding {
    InputState* state = nullptr;
    NumberStep step = {};
    bool hasStep = false;
    bool hasMin = false;
    double min = 0;
    bool hasMax = false;
    double max = 0;
    bool disabled = false;
    shell::CallbackId onStep = 0;
};

// The retained native half of a script view. JavaScript runs only when dirty
// and publishes a RenderSnapshot; every ordinary repaint replays that snapshot
// through ShellMaterialize without entering the VM.
struct ScriptView {
    ShellRuntime* runtime = nullptr;
    ViewType* type = nullptr;
    ViewObject* object = nullptr;
    RenderSnapshot* snapshot = nullptr;
    Policy* policy = nullptr;
    ShellError error = {};
    EntityId self = {};
    // The palette revision the current snapshot resolved its colors against.
    // A theme change reaches every description, and nothing else would notify
    // a view about it.
    uint32_t themeRevision = 0;
    bool dirty = true;

    ~ScriptView();

    static Entity<ScriptView> New(App* app, ShellRuntime* runtime,
                                  ViewType* type, Policy* policy = nullptr);
    static El* Render(ScriptView* self, Ctx* cx);
    static void Refresh(ScriptView* self, Ctx* cx);
    static bool Reload(ScriptView* self, Ctx* cx, Str directory, Str entry,
                       ShellError* error = nullptr);

    static void OnClick(ScriptView* self, Ctx* cx, const ClickEvent* event,
                        intptr_t callback);
    static void OnChange(ScriptView* self, Ctx* cx, const ClickEvent* event,
                         intptr_t value);
    static void OnHover(ScriptView* self, Ctx* cx, const HoverEvent* event,
                        intptr_t callback);
    static void OnMouseMove(ScriptView* self, Ctx* cx,
                            const MouseMoveEvent* event,
                            intptr_t callback);
    static void OnOpenChange(ScriptView* self, Ctx* cx,
                             const PopoverOpenChangeEvent* event,
                             intptr_t callback);
    static void OnResize(ScriptView* self, Ctx* cx,
                         const ResizablePanelEvent* event,
                         intptr_t callback);
    static void OnBoundBool(ScriptView* self, Ctx* cx,
                            const void* event, intptr_t binding);
    static void OnBoundString(ScriptView* self, Ctx* cx,
                              const ClickEvent* event, intptr_t binding);
    static void OnItemSecondaryPress(ScriptView* self, Ctx* cx,
                                     const MouseDownEvent* event,
                                     intptr_t binding);
    static void OnSelectAction(ScriptView* self, Ctx* cx,
                               const ActionEvent* event,
                               intptr_t binding);
    static void OnSelectOpen(ScriptView* self, Ctx* cx,
                             const ClickEvent* event,
                             intptr_t binding);
    static void OnNumberStep(ScriptView* self, Ctx* cx,
                             const NumberInputEvent* event,
                             intptr_t callback);
    static void OnNumberKey(ScriptView* self, Ctx* cx,
                            const KeyEvent* event, intptr_t binding);
    static void OnInputEvent(ScriptView* self, Ctx* cx,
                             const InputEvent* event, intptr_t handle);
    static void OnSliderEvent(ScriptView* self, Ctx* cx,
                              const SliderEvent* event, intptr_t handle);
    static void OnOtpEvent(ScriptView* self, Ctx* cx, const OtpEvent* event,
                           intptr_t handle);
};

} // namespace gpui
#endif // GPUI_SHELL_VIEW_H_

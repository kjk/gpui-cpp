#ifndef GPUI_SHELL_VIEW_H_
#define GPUI_SHELL_VIEW_H_

#include "shell/materialize.h"
#include "base/popover.h"

namespace gpui {

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
    bool dirty = true;

    ~ScriptView();

    static Entity<ScriptView> New(App* app, ShellRuntime* runtime,
                                  ViewType* type, Policy* policy = nullptr);
    static El* Render(ScriptView* self, Ctx* cx);
    static void Refresh(ScriptView* self, Ctx* cx);

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
    static void OnInputEvent(ScriptView* self, Ctx* cx,
                             const InputEvent* event, intptr_t handle);
    static void OnSliderEvent(ScriptView* self, Ctx* cx,
                              const SliderEvent* event, intptr_t handle);
    static void OnOtpEvent(ScriptView* self, Ctx* cx, const OtpEvent* event,
                           intptr_t handle);
};

} // namespace gpui
#endif // GPUI_SHELL_VIEW_H_

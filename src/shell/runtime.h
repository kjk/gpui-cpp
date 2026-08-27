#ifndef GPUI_SHELL_RUNTIME_H_
#define GPUI_SHELL_RUNTIME_H_

#include "shell/metrics.h"
#include "shell/policy.h"
#include "shell/retained.h"
#include "shell/snapshot.h"

namespace gpui {

struct ViewType;
struct ViewObject;
struct ShellRuntimeImpl;
struct ShellRuntimeControl;
struct ShellRuntimeAccess;
struct ShellTaskDriver;

class ShellRuntime {
  public:
    static ShellRuntime* New(App* app = nullptr, ShellError* error = nullptr);
    ShellRuntime* Retain();
    void Release();

    ViewType* LoadSource(Str name, Str source, ShellError* error = nullptr);
    ViewType* LoadApp(Str directory, Str entry = StrL("main.js"),
                      ShellError* error = nullptr);
    ViewObject* Instantiate(ViewType* type, Window* window, App* app,
                            Policy* policy = nullptr,
                            ShellError* error = nullptr,
                            EntityId view = {});
    RenderSnapshot* BuildSnapshot(ViewObject* object, Window* window, App* app,
                                  EntityId view = {}, Policy* policy = nullptr,
                                  ShellError* error = nullptr);
    Str RenderToSpec(Arena* into, ViewObject* object, Window* window, App* app,
                     EntityId view = {}, Policy* policy = nullptr,
                     ShellError* error = nullptr);

    bool Eval(Str source, Str name = StrL("<eval>"),
              ShellError* error = nullptr);
    bool DrainJobs(int limit = 1024, ShellError* error = nullptr);
    RuntimeMetrics ReadMetrics() const;
    void RecordMaterialize(uint64_t nanos);
    int LiveCallbacks() const;
    int LiveEntities() const;
    int LiveTasks() const;
    shell::RetainedEntry* Retained(shell::EntityHandle handle) const;

    // ScriptView registers its dirty bit so cx.notify() can invalidate the
    // JavaScript description as well as the native window. Ordinary repaint
    // causes then replay the published snapshot without entering QuickJS.
    void RegisterScriptView(EntityId view, bool* dirty);
    void UnregisterScriptView(EntityId view, bool* dirty);
    void InvalidateScriptView(EntityId view);
    void ReleaseOwnedEntities(EntityId view);

    void DispatchClick(shell::CallbackId callback, const ClickEvent& event,
                       Window* window, App* app);
    void DispatchMouseMove(shell::CallbackId callback,
                           const MouseMoveEvent& event, Window* window,
                           App* app);
    void DispatchChange(shell::CallbackId callback, bool value, Window* window,
                        App* app);
    void DispatchIndex(shell::CallbackId callback, uint32_t value, Window* window,
                       App* app);
    void DispatchSignal(shell::CallbackId callback, Window* window, App* app);
    void DispatchInputEvent(shell::EntityHandle handle,
                            const InputEvent& event, Window* window, App* app);
    void DispatchSliderEvent(shell::EntityHandle handle,
                             const SliderEvent& event, Window* window,
                             App* app);
    void DispatchOtpEvent(shell::EntityHandle handle, const OtpEvent& event,
                          Window* window, App* app);
    void RenderVirtualItems(shell::CallbackId render,
                            shell::CallbackId getKey, int first, int end,
                            Ctx* cx, El** out);

  private:
    friend struct ShellRuntimeAccess;
    friend struct ShellTaskDriver;
    friend void ShellRuntimeRetireSnapshot(void*, uint64_t);
    void ResumeTask(uint32_t id, Ctx* cx);
    ShellRuntime();
    ~ShellRuntime();

    uint32_t refs = 1;
    ShellRuntimeImpl* impl = nullptr;
    ShellRuntimeControl* control = nullptr;
};

ViewType* ViewTypeRetain(ViewType* type);
void ViewTypeRelease(ViewType* type);
ViewObject* ViewObjectRetain(ViewObject* object);
void ViewObjectRelease(ViewObject* object);
void ShellSetDevelopmentMode(bool enabled);
bool ShellDevelopmentMode();

struct ShellExitRequest {
    int code = 0;
    EntityId view = {};
};
using ShellExitHandler = void (*)(const ShellExitRequest&, Ctx*);
void ShellOnExitRequest(ShellExitHandler handler);

} // namespace gpui
#endif // GPUI_SHELL_RUNTIME_H_

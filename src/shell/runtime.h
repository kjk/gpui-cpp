#ifndef GPUI_SHELL_RUNTIME_H_
#define GPUI_SHELL_RUNTIME_H_

#include "shell/metrics.h"
#include "shell/snapshot.h"

namespace gpui {

struct ViewType;
struct ViewObject;
struct ShellRuntimeImpl;
struct ShellRuntimeControl;
struct ShellRuntimeAccess;

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
                            ShellError* error = nullptr);
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
    int LiveCallbacks() const;

    void DispatchClick(shell::CallbackId callback, const ClickEvent& event,
                       Window* window, App* app);
    void DispatchChange(shell::CallbackId callback, bool value, Window* window,
                        App* app);
    void DispatchIndex(shell::CallbackId callback, uint32_t value, Window* window,
                       App* app);
    void DispatchSignal(shell::CallbackId callback, Window* window, App* app);

  private:
    friend struct ShellRuntimeAccess;
    friend void ShellRuntimeRetireSnapshot(void*, uint64_t);
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

} // namespace gpui
#endif // GPUI_SHELL_RUNTIME_H_

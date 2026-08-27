#include "shell/runtime.h"

#include "quickjs/quickjs.h"
#include "shell/materialize.h"
#include "shell/retained.h"
#include "shell/scope.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace gpui {

constexpr uint64_t kMaxModuleBytes = 8ull * 1024ull * 1024ull;
constexpr int kMaxJobBatch = 1024;

struct ShellRuntimeControl {
    uint32_t refs = 1;
    ShellRuntime* runtime = nullptr;
};

struct AppModule {
    Str root;
    uint32_t generation = 0;
};

struct ScriptViewRegistration {
    EntityId view = {};
    bool* dirty = nullptr;
};

struct CallbackEntry {
    shell::CallbackId id = 0;
    uint64_t generation = 0;
    JSValue function = JS_UNDEFINED;
    EntityId view = {};
    Policy* policy = nullptr;
    AppModule* application = nullptr;
    uint64_t registeredIn = 0;
    bool committed = false;
};

struct CallbackArena {
    Vec<CallbackEntry*> entries;
    uint64_t nextGeneration = 0;
    // Zero is the element runtime's "no click" sentinel. Callback ids are
    // also used as explicit click ids for controls that supply a bool value.
    shell::CallbackId nextCallback = 1;
    uint64_t buildingGeneration = 0;
    int buildingStart = 0;
    bool building = false;

    uint64_t Begin(JSContext* ctx) {
        Abort(ctx);
        building = true;
        buildingStart = entries.len;
        buildingGeneration = nextGeneration++;
        return buildingGeneration;
    }

    shell::CallbackId Push(JSContext* ctx, JSValueConst function,
                           EntityId view, Policy* policy,
                           uint64_t registeredIn,
                           AppModule* application) {
        if (!building || nextCallback == UINT64_MAX) return UINT64_MAX;
        CallbackEntry* entry = new CallbackEntry();
        entry->id = nextCallback++;
        entry->generation = buildingGeneration;
        entry->function = JS_DupValue(ctx, function);
        entry->view = view;
        entry->policy = PolicyRetain(policy);
        entry->application = application;
        entry->registeredIn = registeredIn;
        entries.Append(entry);
        return entry->id;
    }

    shell::CallbackId PushPersistent(JSContext* ctx, JSValueConst function,
                                     EntityId view, Policy* policy,
                                     AppModule* application) {
        if (nextCallback == UINT64_MAX) return UINT64_MAX;
        CallbackEntry* entry = new CallbackEntry();
        entry->id = nextCallback++;
        entry->generation = UINT64_MAX;
        entry->function = JS_DupValue(ctx, function);
        entry->view = view;
        entry->policy = PolicyRetain(policy);
        entry->application = application;
        entry->registeredIn = shell::ScopeCurrentGeneration();
        entry->committed = true;
        entries.Append(entry);
        return entry->id;
    }

    void Commit() {
        if (!building) return;
        for (int i = buildingStart; i < entries.len; i++) {
            entries[i]->committed = true;
        }
        building = false;
    }

    void Abort(JSContext* ctx) {
        if (!building) return;
        while (entries.len > buildingStart) {
            CallbackEntry* entry = entries[entries.len - 1];
            if (ctx) JS_FreeValue(ctx, entry->function);
            PolicyRelease(entry->policy);
            delete entry;
            entries.len--;
        }
        building = false;
    }

    CallbackEntry* Get(shell::CallbackId id) const {
        for (int i = 0; i < entries.len; i++) {
            CallbackEntry* entry = entries[i];
            if (entry->committed && entry->id == id) return entry;
        }
        return nullptr;
    }

    void Retire(JSContext* ctx, uint64_t generation) {
        int out = 0;
        for (int i = 0; i < entries.len; i++) {
            CallbackEntry* entry = entries[i];
            if (entry->committed && entry->generation == generation) {
                JS_FreeValue(ctx, entry->function);
                PolicyRelease(entry->policy);
                delete entry;
            } else {
                entries[out++] = entry;
            }
        }
        entries.len = out;
        if (buildingStart > out) buildingStart = out;
    }

    void RetireId(JSContext* ctx, shell::CallbackId id) {
        for (int i = 0; i < entries.len; i++) {
            CallbackEntry* entry = entries[i];
            if (entry->id != id) continue;
            JS_FreeValue(ctx, entry->function);
            PolicyRelease(entry->policy);
            delete entry;
            for (int j = i + 1; j < entries.len; j++) {
                entries[j - 1] = entries[j];
            }
            entries.len--;
            if (buildingStart > i) buildingStart--;
            return;
        }
    }

    void Clear(JSContext* ctx) {
        for (int i = 0; i < entries.len; i++) {
            CallbackEntry* entry = entries[i];
            JS_FreeValue(ctx, entry->function);
            PolicyRelease(entry->policy);
            delete entry;
        }
        entries.Reset();
        building = false;
        buildingStart = 0;
    }

    int Live() const {
        int count = 0;
        for (int i = 0; i < entries.len; i++) {
            if (entries[i]->committed) count++;
        }
        return count;
    }
};

struct ShellRuntimeImpl {
    ShellRuntime* owner = nullptr;
    JSRuntime* jsRuntime = nullptr;
    JSContext* context = nullptr;
    shell::SpecArena* scratch = nullptr;
    CallbackArena callbacks;
    shell::RetainedStore retained;
    shell::Metrics metrics;
    Vec<AppModule*> modules;
    Vec<ScriptViewRegistration> views;
    uint32_t nextModuleGeneration = 1;
    uint64_t detachedExecution = 0;
    uint64_t interruptIdentity = UINT64_MAX;
    double interruptStarted = 0;
    bool interruptWasScoped = false;
};

struct ViewType {
    uint32_t refs = 1;
    ShellRuntime* runtime = nullptr;
    JSValue value = JS_UNDEFINED;
    AppModule* application = nullptr;
};

struct ViewObject {
    uint32_t refs = 1;
    ShellRuntime* runtime = nullptr;
    JSValue value = JS_UNDEFINED;
    AppModule* application = nullptr;
};

struct ShellRuntimeAccess {
    static ShellRuntimeImpl* Impl(ShellRuntime* runtime) {
        return runtime ? runtime->impl : nullptr;
    }
    static ShellRuntimeControl* Control(ShellRuntime* runtime) {
        return runtime ? runtime->control : nullptr;
    }
};

static void ControlRetain(void* state) {
    if (state) ((ShellRuntimeControl*)state)->refs++;
}

static void ControlRelease(void* state) {
    ShellRuntimeControl* control = (ShellRuntimeControl*)state;
    if (control && --control->refs == 0) delete control;
}

void ShellRuntimeRetireSnapshot(void* state, uint64_t generation) {
    ShellRuntimeControl* control = (ShellRuntimeControl*)state;
    if (!control || !control->runtime || !control->runtime->impl) return;
    ShellRuntimeImpl* impl = control->runtime->impl;
    impl->callbacks.Retire(impl->context, generation);
}

static SnapshotRuntimeLease SnapshotLease(ShellRuntime* runtime) {
    SnapshotRuntimeLease lease = {};
    lease.state = ShellRuntimeAccess::Control(runtime);
    lease.retain = ControlRetain;
    lease.release = ControlRelease;
    lease.retireCallbacks = ShellRuntimeRetireSnapshot;
    return lease;
}

static void SetError(ShellError* error, Str message) {
    ShellErrorSet(error, message);
}

static void BeginExecution(ShellRuntimeImpl* impl) {
    impl->detachedExecution++;
    if (impl->detachedExecution == 0) impl->detachedExecution++;
}

static int Interrupt(JSRuntime*, void* opaque) {
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)opaque;
    uint64_t generation = shell::ScopeCurrentGeneration();
    bool scoped = generation != 0;
    uint64_t identity = scoped ? generation : impl->detachedExecution;
    if (impl->interruptIdentity != identity ||
        impl->interruptWasScoped != scoped) {
        impl->interruptIdentity = identity;
        impl->interruptWasScoped = scoped;
        impl->interruptStarted = TimeNow();
    }
    double budget = 5.0;
    if (scoped) {
        ScopePhase phase = shell::ScopeCurrentPhase();
        budget = phase == ScopePhase::Render || phase == ScopePhase::Layout
                     ? 0.050
                     : 0.500;
    }
    return TimeNow() - impl->interruptStarted > budget;
}

static Str ExceptionText(Arena* arena, JSContext* ctx) {
    JSValue exception = JS_GetException(ctx);
    StrBuilder out;
    out.a = arena;
    size_t messageLen = 0;
    const char* message = JS_ToCStringLen(ctx, &messageLen, exception);
    Str messageText;
    if (message) {
        messageText = StrDup(arena, Str(message, (int)messageLen));
        out.Append(messageText);
        JS_FreeCString(ctx, message);
    } else {
        out.Append(StrL("JavaScript exception"));
    }
    if (JS_IsError(exception)) {
        JSValue stack = JS_GetPropertyStr(ctx, exception, "stack");
        if (!JS_IsException(stack) && !JS_IsUndefined(stack)) {
            size_t stackLen = 0;
            const char* text = JS_ToCStringLen(ctx, &stackLen, stack);
            if (text && stackLen > 0) {
                Str stackText(text, (int)stackLen);
                if (!messageText || !StrStartsWith(stackText, messageText)) {
                    out.AppendChar('\n');
                    out.Append(stackText);
                } else if ((int)stackLen > (int)messageLen) {
                    out.Append(Str(text + messageLen,
                                   (int)stackLen - (int)messageLen));
                }
                JS_FreeCString(ctx, text);
            }
        }
        JS_FreeValue(ctx, stack);
    }
    JS_FreeValue(ctx, exception);
    return out.TakeStr();
}

static bool CaptureException(ShellRuntimeImpl* impl, ShellError* error) {
    Arena* arena = ArenaNew();
    Str text = ExceptionText(arena, impl->context);
    SetError(error, text);
    ArenaDelete(arena);
    return false;
}

static bool Await(ShellRuntimeImpl* impl, JSValueConst value,
                  ShellError* error) {
    if (!JS_IsPromise(value)) return true;
    int count = 0;
    while (JS_PromiseState(impl->context, value) == JS_PROMISE_PENDING &&
           JS_IsJobPending(impl->jsRuntime) && count++ < kMaxJobBatch) {
        JSContext* context = nullptr;
        int result = JS_ExecutePendingJob(impl->jsRuntime, &context);
        if (result < 0) return CaptureException(impl, error);
        if (result == 0) break;
    }
    JSPromiseStateEnum state = JS_PromiseState(impl->context, value);
    if (state == JS_PROMISE_REJECTED) {
        JSValue reason = JS_PromiseResult(impl->context, value);
        JS_Throw(impl->context, reason);
        return CaptureException(impl, error);
    }
    if (state == JS_PROMISE_PENDING) {
        SetError(error, StrL("module evaluation left a pending promise with no host work able to settle it"));
        return false;
    }
    return true;
}

static bool ReadFileBounded(Str path, Str* source, ShellError* error) {
    if (source) *source = {};
    char name[kMaxPath] = {};
    if (path.len <= 0 || path.len >= kMaxPath) {
        SetError(error, StrL("module path is empty or too long"));
        return false;
    }
    memcpy(name, path.s, (size_t)path.len);
    FILE* file = fopen(name, "rb");
    if (!file) {
        SetError(error, fmt("reading module `%s` failed", path));
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        SetError(error, fmt("reading module `%s` failed", path));
        return false;
    }
    long size = ftell(file);
    if (size < 0 || (uint64_t)size > kMaxModuleBytes ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        SetError(error, fmt("module `%s` is over the 8 MiB limit", path));
        return false;
    }
    char* bytes = (char*)Alloc(nullptr, (int)size + 1);
    if (!bytes) {
        fclose(file);
        SetError(error, fmt("allocating %ld bytes for module `%s` failed",
                            size + 1, path));
        return false;
    }
    size_t got = fread(bytes, 1, (size_t)size, file);
    fclose(file);
    if (got != (size_t)size) {
        Free(nullptr, bytes);
        SetError(error, fmt("reading module `%s` failed", path));
        return false;
    }
    bytes[size] = 0;
    if (source) *source = Str(bytes, (int)size);
    return true;
}

static bool IsBuiltin(const char* name) {
    return strcmp(name, "gpui") == 0 || strcmp(name, "gpui-base") == 0 ||
           strcmp(name, "gpui-shell") == 0 || strcmp(name, "gpui-fps") == 0;
}

static const char* const kGpuiExports[] = {
    "View", "div", "svg", "image", "PathBuilder", "Background"};
static const char* const kBaseExports[] = {
    "h_flex", "v_flex", "Button", "Link", "Checkbox", "Switch",
    "Tabs", "Tab", "Progress", "ProgressTrack", "ProgressIndicator",
    "Radio", "Toggle", "RadioGroup", "ToggleGroup", "Table",
    "TableHeader", "TableBody", "TableRow", "TableHead", "TableCell",
    "TableCaption", "h_resizable", "v_resizable", "resizable_panel",
    "Collapsible", "Popover", "HoverCard", "Popup", "Select", "Combobox",
    "DatePicker", "Scrollbar", "v_virtual_list", "h_virtual_list",
    "VirtualListScrollHandle", "Input", "InputState", "NumberInput",
    "Textarea", "TextareaState", "SliderState", "Slider", "SliderTrack",
    "SliderIndicator", "SliderThumb", "OtpState", "OtpInput", "set_theme"};
static const char* const kFpsExports[] = {"fps_monitor"};

static void ModuleExports(const char* name, const char* const** values,
                          int* count) {
    *values = nullptr;
    *count = 0;
    if (strcmp(name, "gpui") == 0) {
        *values = kGpuiExports;
        *count = (int)(sizeof(kGpuiExports) / sizeof(kGpuiExports[0]));
    } else if (strcmp(name, "gpui-base") == 0) {
        *values = kBaseExports;
        *count = (int)(sizeof(kBaseExports) / sizeof(kBaseExports[0]));
    } else if (strcmp(name, "gpui-fps") == 0) {
        *values = kFpsExports;
        *count = (int)(sizeof(kFpsExports) / sizeof(kFpsExports[0]));
    }
}

static int InitBuiltinModule(JSContext* ctx, JSModuleDef* module) {
    JSAtom atom = JS_GetModuleName(ctx, module);
    const char* name = JS_AtomToCString(ctx, atom);
    const char* const* exports = nullptr;
    int count = 0;
    ModuleExports(name ? name : "", &exports, &count);
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue api = JS_GetPropertyStr(ctx, global, "__gpui");
    int result = 0;
    for (int i = 0; i < count; i++) {
        JSValue value = JS_GetPropertyStr(ctx, api, exports[i]);
        if (JS_IsException(value) ||
            JS_SetModuleExport(ctx, module, exports[i], value) < 0) {
            if (!JS_IsException(value)) JS_FreeValue(ctx, value);
            result = -1;
            break;
        }
    }
    JS_FreeValue(ctx, api);
    JS_FreeValue(ctx, global);
    if (name) JS_FreeCString(ctx, name);
    JS_FreeAtom(ctx, atom);
    return result;
}

static AppModule* ApplicationForBase(ShellRuntimeImpl* impl,
                                     const char* base) {
    const char* tag = strrchr(base, '?');
    if (!tag || strncmp(tag, "?v=", 3) != 0) return nullptr;
    uint32_t generation = 0;
    for (const char* at = tag + 3; *at; at++) {
        if (*at < '0' || *at > '9') return nullptr;
        generation = generation * 10u + (uint32_t)(*at - '0');
    }
    for (int i = impl->modules.len - 1; i >= 0; i--) {
        if (impl->modules[i]->generation == generation) return impl->modules[i];
    }
    return nullptr;
}

static void Untag(const char* name, char* out, int cap) {
    const char* tag = strrchr(name, '?');
    int len = tag && strncmp(tag, "?v=", 3) == 0 ? (int)(tag - name)
                                                  : (int)strlen(name);
    if (len >= cap) len = cap - 1;
    memcpy(out, name, (size_t)len);
    out[len] = 0;
}

static void DirectoryName(char* path) {
    int len = (int)strlen(path);
    while (len > 0 && path[len - 1] != '/' && path[len - 1] != '\\') {
        path[--len] = 0;
    }
    while (len > 1 && (path[len - 1] == '/' || path[len - 1] == '\\')) {
        path[--len] = 0;
    }
}

static bool WithinRoot(Str root, Str path) {
    if (path.len < root.len) return false;
#if GPUI_OS_WINDOWS
    if (StrCmpNI(root.s, path.s, root.len) != 0) return false;
#else
    if (memcmp(root.s, path.s, (size_t)root.len) != 0) return false;
#endif
    return path.len == root.len || path.s[root.len] == '/';
}

static char* ModuleNormalize(JSContext* ctx, const char* base,
                             const char* name, void* opaque) {
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)opaque;
    if (IsBuiltin(name)) {
        size_t len = strlen(name);
        char* out = (char*)js_malloc(ctx, len + 1);
        if (out) memcpy(out, name, len + 1);
        return out;
    }
    AppModule* application = ApplicationForBase(impl, base);
    if (!application) {
        JS_ThrowReferenceError(
            ctx, "cannot identify the application importing `%s` from `%s`",
            name, base);
        return nullptr;
    }
    char start[kMaxPath] = {};
    if (name[0] == '.') {
        Untag(base, start, kMaxPath);
        DirectoryName(start);
    } else {
        StrCopyZ(start, kMaxPath, application->root.s);
    }
    char joined[kMaxPath] = {};
    int written = snprintf(joined, sizeof(joined), "%s/%s", start, name);
    if (written <= 0 || written >= kMaxPath) {
        JS_ThrowReferenceError(ctx, "module path `%s` is too long", name);
        return nullptr;
    }
    char candidate[kMaxPath] = {};
    bool found = PlatCanonicalPath(joined, candidate, kMaxPath) &&
                 PlatFileExists(candidate);
    if (!found) {
        int n = snprintf(joined, sizeof(joined), "%s/%s.js", start, name);
        found = n > 0 && n < kMaxPath &&
                PlatCanonicalPath(joined, candidate, kMaxPath) &&
                PlatFileExists(candidate);
    }
    if (!found) {
        JS_ThrowReferenceError(ctx, "cannot resolve module `%s` from `%s`",
                               name, base);
        return nullptr;
    }
    Str canonical(candidate);
    if (!WithinRoot(application->root, canonical)) {
        JS_ThrowReferenceError(
            ctx, "module `%s` resolves outside the application directory `%s`",
            name, application->root.s);
        return nullptr;
    }
    char tagged[kMaxPath + 32] = {};
    int n = snprintf(tagged, sizeof(tagged), "%s?v=%u", candidate,
                     application->generation);
    if (n <= 0 || n >= (int)sizeof(tagged)) return nullptr;
    char* out = (char*)js_malloc(ctx, (size_t)n + 1);
    if (out) memcpy(out, tagged, (size_t)n + 1);
    return out;
}

static JSModuleDef* ModuleLoad(JSContext* ctx, const char* name, void*) {
    if (IsBuiltin(name)) {
        JSModuleDef* module = JS_NewCModule(ctx, name, InitBuiltinModule);
        if (!module) return nullptr;
        const char* const* exports = nullptr;
        int count = 0;
        ModuleExports(name, &exports, &count);
        for (int i = 0; i < count; i++) {
            if (JS_AddModuleExport(ctx, module, exports[i]) < 0) return nullptr;
        }
        return module;
    }
    char path[kMaxPath] = {};
    Untag(name, path, kMaxPath);
    Str source = {};
    ShellError error = {};
    if (!ReadFileBounded(Str(path), &source, &error)) {
        JS_ThrowReferenceError(ctx, "%.*s", error.message.len,
                               error.message.s ? error.message.s : "module load failed");
        ShellErrorClear(&error);
        return nullptr;
    }
    JSValue value = JS_Eval(ctx, source.s, (size_t)source.len, name,
                            JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    Free(nullptr, source.s);
    if (JS_IsException(value)) return nullptr;
    JSModuleDef* module = (JSModuleDef*)JS_VALUE_GET_PTR(value);
    JS_FreeValue(ctx, value);
    return module;
}

static shell::ComponentKind ComponentKindOf(Str name) {
    struct NamedKind {
        const char* name;
        shell::ComponentKind kind;
    };
    static const NamedKind kinds[] = {
        {"div", shell::ComponentKind::Div},
        {"h_flex", shell::ComponentKind::HFlex},
        {"v_flex", shell::ComponentKind::VFlex},
        {"text", shell::ComponentKind::Text},
        {"Button", shell::ComponentKind::Button},
        {"Link", shell::ComponentKind::Link},
        {"Checkbox", shell::ComponentKind::Checkbox},
        {"Switch", shell::ComponentKind::Switch},
        {"Scrollbar", shell::ComponentKind::Scrollbar},
        {"Input", shell::ComponentKind::Input},
        {"Textarea", shell::ComponentKind::Textarea},
        {"NumberInput", shell::ComponentKind::NumberInput},
        {"OtpInput", shell::ComponentKind::OtpInput},
        {"svg", shell::ComponentKind::Svg},
        {"image", shell::ComponentKind::Image},
        {"Tabs", shell::ComponentKind::Tabs},
        {"Tab", shell::ComponentKind::Tab},
        {"Progress", shell::ComponentKind::Progress},
        {"ProgressTrack", shell::ComponentKind::ProgressTrack},
        {"ProgressIndicator", shell::ComponentKind::ProgressIndicator},
        {"FpsMonitor", shell::ComponentKind::FpsMonitor},
        {"Slider", shell::ComponentKind::Slider},
        {"SliderTrack", shell::ComponentKind::SliderTrack},
        {"SliderIndicator", shell::ComponentKind::SliderIndicator},
        {"SliderThumb", shell::ComponentKind::SliderThumb},
        {"Radio", shell::ComponentKind::Radio},
        {"Toggle", shell::ComponentKind::Toggle},
        {"RadioGroup", shell::ComponentKind::RadioGroup},
        {"ToggleGroup", shell::ComponentKind::ToggleGroup},
        {"Table", shell::ComponentKind::Table},
        {"TableHeader", shell::ComponentKind::TableHeader},
        {"TableBody", shell::ComponentKind::TableBody},
        {"TableRow", shell::ComponentKind::TableRow},
        {"TableHead", shell::ComponentKind::TableHead},
        {"TableCell", shell::ComponentKind::TableCell},
        {"TableCaption", shell::ComponentKind::TableCaption},
        {"h_resizable", shell::ComponentKind::HResizable},
        {"v_resizable", shell::ComponentKind::VResizable},
        {"ResizablePanel", shell::ComponentKind::ResizablePanel},
        {"Collapsible", shell::ComponentKind::Collapsible},
        {"Popover", shell::ComponentKind::Popover},
        {"HoverCard", shell::ComponentKind::HoverCard},
        {"Popup", shell::ComponentKind::Popup},
        {"Select", shell::ComponentKind::Select},
        {"Combobox", shell::ComponentKind::Combobox},
        {"DatePicker", shell::ComponentKind::DatePicker},
    };
    for (const NamedKind& named : kinds) {
        if (StrEq(name, named.name)) return named.kind;
    }
    return shell::ComponentKind::Div;
}

static bool JsString(JSContext* ctx, JSValueConst value, Arena* arena,
                     Str* out) {
    size_t len = 0;
    const char* text = JS_ToCStringLen(ctx, &len, value);
    if (!text) return false;
    *out = StrDup(arena, Str(text, (int)len));
    JS_FreeCString(ctx, text);
    return true;
}

static JSValue NativeComponent(JSContext* ctx, JSValueConst, int argc,
                               JSValueConst* argv) {
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    if (!impl || argc < 1) return JS_ThrowTypeError(ctx, "component kind is missing");
    Arena* arena = ArenaNew();
    Str kind;
    if (!JsString(ctx, argv[0], arena, &kind)) {
        ArenaDelete(arena);
        return JS_EXCEPTION;
    }
    shell::Component component = {};
    component.kind = ComponentKindOf(kind);
    if (argc > 1 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1]) &&
        !JsString(ctx, argv[1], arena, &component.text)) {
        ArenaDelete(arena);
        return JS_EXCEPTION;
    }
    if (argc > 2 && !JS_IsUndefined(argv[2]) && !JS_IsNull(argv[2])) {
        int64_t handle = 0;
        if (JS_ToInt64(ctx, &handle, argv[2]) < 0 || handle < 0) {
            ArenaDelete(arena);
            return JS_ThrowTypeError(ctx, "component handle must be non-negative");
        }
        component.handle = (uint64_t)handle;
    }
    if (argc > 3 && !JS_IsUndefined(argv[3]) && !JS_IsNull(argv[3])) {
        if (JS_ToUint32(ctx, &component.index, argv[3]) < 0) {
            ArenaDelete(arena);
            return JS_EXCEPTION;
        }
    }
    shell::SpecId id = impl->scratch->Push(component);
    ArenaDelete(arena);
    return JS_NewUint32(ctx, id);
}

static bool JsSpecId(JSContext* ctx, JSValueConst value, shell::SpecId* out) {
    return JS_ToUint32(ctx, out, value) == 0;
}

static JSValue SpecFailure(JSContext* ctx, const shell::SpecError& failure) {
    Arena* arena = ArenaNew();
    Str message = shell::SpecErrorMessage(arena, failure);
    JSValue result = JS_ThrowTypeError(ctx, "%.*s", message.len, message.s);
    ArenaDelete(arena);
    return result;
}

static JSValue NativeAttach(JSContext* ctx, JSValueConst, int argc,
                            JSValueConst* argv) {
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    shell::SpecId parent = 0, child = 0;
    if (!impl || argc < 2 || !JsSpecId(ctx, argv[0], &parent) ||
        !JsSpecId(ctx, argv[1], &child)) {
        return JS_ThrowTypeError(ctx, "child(element) expects an element");
    }
    shell::SpecError failure = {};
    if (!impl->scratch->Attach(parent, child, &failure)) {
        return SpecFailure(ctx, failure);
    }
    return JS_UNDEFINED;
}

static JSValue NativeState(JSContext* ctx, JSValueConst, int argc,
                           JSValueConst* argv) {
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    shell::SpecId owner = 0;
    if (!impl || argc < 2 || !JsSpecId(ctx, argv[0], &owner)) {
        return JS_ThrowTypeError(ctx, "state style needs an element");
    }
    Arena* arena = ArenaNew();
    Str name;
    if (!JsString(ctx, argv[1], arena, &name)) {
        ArenaDelete(arena);
        return JS_EXCEPTION;
    }
    shell::Component component = {};
    shell::SpecId state = impl->scratch->Push(component);
    shell::SpecError failure = {};
    if (!impl->scratch->Claim(state, &failure)) {
        ArenaDelete(arena);
        return SpecFailure(ctx, failure);
    }
    shell::SpecOp op = {};
    op.kind = shell::SpecOpKind::StateStyle;
    op.name = name;
    op.node = state;
    if (!impl->scratch->PushOp(owner, op, &failure)) {
        ArenaDelete(arena);
        return SpecFailure(ctx, failure);
    }
    ArenaDelete(arena);
    return JS_NewUint32(ctx, state);
}

static JSValue NativeSlot(JSContext* ctx, JSValueConst, int argc,
                          JSValueConst* argv) {
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    shell::SpecId owner = 0, element = 0;
    if (!impl || argc < 3 || !JsSpecId(ctx, argv[0], &owner) ||
        !JsSpecId(ctx, argv[2], &element)) {
        return JS_ThrowTypeError(ctx, "slot(element) expects an element");
    }
    Arena* arena = ArenaNew();
    Str name;
    if (!JsString(ctx, argv[1], arena, &name)) {
        ArenaDelete(arena);
        return JS_EXCEPTION;
    }
    shell::SpecError failure = {};
    if (!impl->scratch->Claim(element, &failure)) {
        ArenaDelete(arena);
        return SpecFailure(ctx, failure);
    }
    shell::SpecOp op = {};
    op.kind = shell::SpecOpKind::Slot;
    op.name = name;
    op.node = element;
    if (!impl->scratch->PushOp(owner, op, &failure)) {
        ArenaDelete(arena);
        return SpecFailure(ctx, failure);
    }
    ArenaDelete(arena);
    return JS_UNDEFINED;
}

static bool IsCallbackMethod(Str name) {
    static const char names[] =
        "on_click\0on_mouse_move\0on_hover\0on_item_click\0on_change\0"
        "on_open_change\0on_confirm\0on_dismiss\0on_step\0on_resize\0";
    for (const char* at = names; *at; at += strlen(at) + 1) {
        if (StrEq(name, at)) return true;
    }
    return false;
}

static bool IsParamStyle(Str name) {
    static const char names[] =
        "w\0h\0size\0min_w\0min_h\0min_size\0max_w\0max_h\0max_size\0"
        "p\0px\0py\0pt\0pb\0pl\0pr\0m\0mx\0my\0mt\0mb\0ml\0mr\0"
        "inset\0top\0bottom\0left\0right\0gap\0gap_x\0gap_y\0"
        "flex_grow\0flex_shrink\0flex_basis\0bg\0text_color\0text_bg\0"
        "text_size\0font_family\0font_weight\0line_height\0opacity\0"
        "border\0border_t\0border_b\0border_l\0border_r\0border_x\0"
        "border_y\0border_color\0rounded\0rounded_t\0rounded_b\0"
        "rounded_l\0rounded_r\0rounded_tl\0rounded_tr\0rounded_bl\0"
        "rounded_br\0";
    for (const char* at = names; *at; at += strlen(at) + 1) {
        if (StrEq(name, at)) return true;
    }
    return false;
}

static bool IsBehavior(Str name) {
    static const char names[] =
        "disabled\0selected\0checked\0accessibility_label\0tooltip\0role\0"
        "aria_selected\0aria_active_descendant\0track_focus\0track_scroll\0"
        "content_focus_handle\0tab_index\0tab_stop\0href\0id\0"
        "overflow_scroll\0overflow_x_scroll\0overflow_y_scroll\0"
        "overflow_scrollbar\0overflow_x_scrollbar\0overflow_y_scrollbar\0"
        "mode\0scroll_size\0viewport_from_layout\0controls_right\0"
        "panel_visible\0panel_size\0size_range\0set_position\0pressed\0"
        "start\0value\0indeterminate\0axis\0row_count\0column_count\0"
        "open\0default_open\0overlay_closable\0anchor\0mouse_button\0"
        "open_delay\0close_delay\0transition\0spring\0"
        "with_item_to_measure_index\0";
    for (const char* at = names; *at; at += strlen(at) + 1) {
        if (StrEq(name, at)) return true;
    }
    return false;
}

static bool BridgeValue(JSContext* ctx, JSValueConst value, Arena* arena,
                        shell::Bridged* out) {
    if (JS_IsUndefined(value) || JS_IsNull(value)) {
        *out = shell::Bridged::Nil();
        return true;
    }
    if (JS_IsBool(value)) {
        *out = shell::Bridged::Bool(JS_ToBool(ctx, value) != 0);
        return true;
    }
    if (JS_IsNumber(value)) {
        double number = 0;
        if (JS_ToFloat64(ctx, &number, value) < 0) return false;
        *out = shell::Bridged::Number(number);
        return true;
    }
    if (JS_IsString(value)) {
        Str text;
        if (!JsString(ctx, value, arena, &text)) return false;
        *out = shell::Bridged::String(text);
        return true;
    }
    JS_ThrowTypeError(ctx, "script values crossing into an element method must be nil, boolean, number or string");
    return false;
}

static JSValue NativeApply(JSContext* ctx, JSValueConst, int argc,
                           JSValueConst* argv) {
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    shell::SpecId id = 0;
    if (!impl || argc < 3 || !JsSpecId(ctx, argv[0], &id)) {
        return JS_ThrowTypeError(ctx, "element method has no live receiver");
    }
    Arena* arena = ArenaNew();
    Str name;
    if (!JsString(ctx, argv[1], arena, &name)) {
        ArenaDelete(arena);
        return JS_EXCEPTION;
    }
    int64_t argCount64 = 0;
    if (JS_GetLength(ctx, argv[2], &argCount64) < 0 || argCount64 < 0 ||
        argCount64 > 1024) {
        ArenaDelete(arena);
        return JS_ThrowRangeError(ctx, "element method has too many arguments");
    }
    int argCount = (int)argCount64;
    shell::SpecOp op = {};
    op.name = name;
    if (IsCallbackMethod(name)) {
        if (shell::ScopeCurrentPhase() == ScopePhase::Layout) {
            ArenaDelete(arena);
            return JS_ThrowTypeError(ctx, "callbacks cannot be registered from a virtual list item renderer");
        }
        JSValue handler = argCount > 0
                              ? JS_GetPropertyUint32(ctx, argv[2], 0)
                              : JS_UNDEFINED;
        if (!JS_IsFunction(ctx, handler)) {
            JS_FreeValue(ctx, handler);
            ArenaDelete(arena);
            return JS_ThrowTypeError(ctx, "%.*s(handler) expects a function",
                                     name.len, name.s);
        }
        shell::CallbackId callback = impl->callbacks.Push(
            ctx, handler, shell::ScopeCurrentView(),
            shell::ScopeCurrentPolicy(), shell::ScopeCurrentGeneration(),
            (AppModule*)shell::ScopeCurrentApplication());
        JS_FreeValue(ctx, handler);
        if (callback == UINT64_MAX) {
            ArenaDelete(arena);
            return JS_ThrowInternalError(ctx, "a callback was registered outside a snapshot build");
        }
        op.kind = shell::SpecOpKind::Callback;
        op.callback = callback;
    } else {
        op.kind = IsParamStyle(name) ? shell::SpecOpKind::ParamStyle
                                     : (!IsBehavior(name) && argCount == 0
                                            ? shell::SpecOpKind::NullaryStyle
                                            : shell::SpecOpKind::Method);
        op.argCount = argCount;
        if (argCount > 0) {
            op.args = (shell::Bridged*)Alloc(
                arena, (int)(sizeof(shell::Bridged) * (size_t)argCount));
            for (int i = 0; i < argCount; i++) {
                JSValue value = JS_GetPropertyUint32(ctx, argv[2], (uint32_t)i);
                bool bridged = !JS_IsException(value) &&
                               BridgeValue(ctx, value, arena, &op.args[i]);
                JS_FreeValue(ctx, value);
                if (!bridged) {
                    ArenaDelete(arena);
                    return JS_EXCEPTION;
                }
            }
        }
    }
    shell::SpecError failure = {};
    if (!impl->scratch->PushOp(id, op, &failure)) {
        ArenaDelete(arena);
        return SpecFailure(ctx, failure);
    }
    ArenaDelete(arena);
    return JS_UNDEFINED;
}

static bool JsHandle(JSContext* ctx, JSValueConst value,
                     shell::EntityHandle* out) {
    uint64_t handle = 0;
    if (JS_ToIndex(ctx, &handle, value) < 0) return false;
    *out = handle;
    return true;
}

static shell::RetainedEntry* LiveRetained(JSContext* ctx,
                                           shell::EntityHandle handle,
                                           shell::RetainedKind kind,
                                           const char* what) {
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    shell::RetainedEntry* entry = impl ? impl->retained.Find(handle) : nullptr;
    if (!entry || entry->kind != kind) {
        JS_ThrowTypeError(ctx, "this %s state has been released", what);
        return nullptr;
    }
    return entry;
}

static bool RefuseRetainedCreation(JSContext* ctx, const char* what) {
    if (shell::ScopeHasCurrent() &&
        (shell::ScopeCurrentPhase() == ScopePhase::Render ||
         shell::ScopeCurrentPhase() == ScopePhase::Layout)) {
        JS_ThrowTypeError(ctx,
                          "%s cannot run during render; create retained state "
                          "in init() or an event handler",
                          what);
        return true;
    }
    return false;
}

static bool RefuseRetainedMutation(JSContext* ctx, const char* what) {
    if (shell::ScopeHasCurrent() &&
        (shell::ScopeCurrentPhase() == ScopePhase::Render ||
         shell::ScopeCurrentPhase() == ScopePhase::Layout)) {
        JS_ThrowTypeError(ctx,
                          "%s cannot run during render or layout; mutate "
                          "retained state from an event handler or task",
                          what);
        return true;
    }
    return false;
}

static bool OptionalJsString(JSContext* ctx, JSValueConst value, Arena* arena,
                             Str* out) {
    *out = {};
    return JS_IsUndefined(value) || JS_IsNull(value) ||
           JsString(ctx, value, arena, out);
}

static JSValue NativeInputStateNew(JSContext* ctx, JSValueConst, int argc,
                                   JSValueConst* argv) {
    if (RefuseRetainedCreation(ctx, "InputState.new(...)")) return JS_EXCEPTION;
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    if (!impl || impl->retained.Len() >= shell::kMaxLiveEntities) {
        return JS_ThrowRangeError(ctx, "the application reached gpui-shell's retained entity limit; release unused handles");
    }
    shell::ScopeHostContext host = shell::ScopeCurrentHost();
    if (!host.IsSet()) return JS_ThrowTypeError(ctx, "InputState.new(...) needs a live host call");
    Arena* arena = ArenaNew();
    Str placeholder, value;
    bool ok = OptionalJsString(ctx, argc > 0 ? argv[0] : JS_UNDEFINED,
                               arena, &placeholder) &&
              OptionalJsString(ctx, argc > 1 ? argv[1] : JS_UNDEFINED,
                               arena, &value);
    shell::EntityHandle handle =
        ok ? impl->retained.CreateInput(
                 false, placeholder, value, 0, host.GetApp(),
                 shell::ScopeCurrentView(), shell::ScopeCurrentApplication())
           : 0;
    ArenaDelete(arena);
    if (!ok) return JS_EXCEPTION;
    if (!handle) return JS_ThrowInternalError(ctx, "creating input state failed");
    return JS_NewInt64(ctx, (int64_t)handle);
}

static JSValue NativeTextareaStateNew(JSContext* ctx, JSValueConst, int argc,
                                      JSValueConst* argv) {
    if (RefuseRetainedCreation(ctx, "TextareaState.new(...)")) return JS_EXCEPTION;
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    if (!impl || impl->retained.Len() >= shell::kMaxLiveEntities) {
        return JS_ThrowRangeError(ctx, "the application reached gpui-shell's retained entity limit; release unused handles");
    }
    shell::ScopeHostContext host = shell::ScopeCurrentHost();
    if (!host.IsSet()) return JS_ThrowTypeError(ctx, "TextareaState.new(...) needs a live host call");
    Arena* arena = ArenaNew();
    Str placeholder, value;
    int32_t rows = 0;
    bool ok = OptionalJsString(ctx, argc > 0 ? argv[0] : JS_UNDEFINED,
                               arena, &placeholder) &&
              OptionalJsString(ctx, argc > 1 ? argv[1] : JS_UNDEFINED,
                               arena, &value);
    if (ok && argc > 2 && !JS_IsNull(argv[2]) && !JS_IsUndefined(argv[2])) {
        ok = JS_ToInt32(ctx, &rows, argv[2]) == 0 && rows > 0;
        if (!ok) JS_ThrowTypeError(ctx, "TextareaState.new rows must be a positive whole number");
    }
    shell::EntityHandle handle =
        ok ? impl->retained.CreateInput(
                 true, placeholder, value, rows, host.GetApp(),
                 shell::ScopeCurrentView(), shell::ScopeCurrentApplication())
           : 0;
    ArenaDelete(arena);
    if (!ok) return JS_EXCEPTION;
    if (!handle) return JS_ThrowInternalError(ctx, "creating textarea state failed");
    return JS_NewInt64(ctx, (int64_t)handle);
}

static JSValue NativeInputValue(JSContext* ctx, JSValueConst, int argc,
                                JSValueConst* argv) {
    shell::EntityHandle handle = 0;
    if (argc < 1 || !JsHandle(ctx, argv[0], &handle)) return JS_EXCEPTION;
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    shell::RetainedEntry* entry = impl ? impl->retained.Find(handle) : nullptr;
    if (!entry || (entry->kind != shell::RetainedKind::Input &&
                   entry->kind != shell::RetainedKind::Textarea)) {
        return JS_ThrowTypeError(ctx, "this text state has been released");
    }
    shell::ScopeHostContext host = shell::ScopeCurrentHost();
    if (!host.IsSet()) return JS_ThrowTypeError(ctx, "value() needs a live host call");
    Str value = InputValue(entry->input);
    return JS_NewStringLen(ctx, value.s ? value.s : "", (size_t)value.len);
}

static JSValue NativeInputSetValue(JSContext* ctx, JSValueConst, int argc,
                                   JSValueConst* argv) {
    if (RefuseRetainedMutation(ctx, "set_value()")) return JS_EXCEPTION;
    shell::EntityHandle handle = 0;
    if (argc < 2 || !JsHandle(ctx, argv[0], &handle)) return JS_EXCEPTION;
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    shell::RetainedEntry* entry = impl ? impl->retained.Find(handle) : nullptr;
    if (!entry || (entry->kind != shell::RetainedKind::Input &&
                   entry->kind != shell::RetainedKind::Textarea)) {
        return JS_ThrowTypeError(ctx, "this text state has been released");
    }
    shell::ScopeHostContext host = shell::ScopeCurrentHost();
    if (!host.IsSet()) return JS_ThrowTypeError(ctx, "set_value() needs a live host call");
    Arena* arena = ArenaNew();
    Str value;
    bool ok = JsString(ctx, argv[1], arena, &value);
    if (ok) InputSetValue(entry->input, value);
    ArenaDelete(arena);
    if (!ok) return JS_EXCEPTION;
    AppInvalidate(host.GetWindow());
    return JS_UNDEFINED;
}

static JSValue NativeInputNumberOption(JSContext* ctx, JSValueConst,
                                       int argc, JSValueConst* argv,
                                       int magic) {
    if (RefuseRetainedMutation(ctx, "numeric input setter")) return JS_EXCEPTION;
    shell::EntityHandle handle = 0;
    if (argc < 2 || !JsHandle(ctx, argv[0], &handle)) return JS_EXCEPTION;
    shell::RetainedEntry* entry = LiveRetained(
        ctx, handle, shell::RetainedKind::Input, "input");
    if (!entry) return JS_EXCEPTION;
    shell::ScopeHostContext host = shell::ScopeCurrentHost();
    if (!host.IsSet()) return JS_ThrowTypeError(ctx, "numeric input setter needs a live host call");
    bool set = !JS_IsNull(argv[1]) && !JS_IsUndefined(argv[1]);
    double value = 0;
    if (set && (JS_ToFloat64(ctx, &value, argv[1]) < 0 || !isfinite(value))) {
        return JS_ThrowTypeError(ctx, "numeric input option must be finite or null");
    }
    if (magic == 0) {
        entry->number.hasStep = set;
        entry->number.step = value;
    } else if (magic == 1) {
        entry->number.hasMin = set;
        entry->number.min = value;
    } else {
        entry->number.hasMax = set;
        entry->number.max = value;
    }
    AppInvalidate(host.GetWindow());
    return JS_UNDEFINED;
}

static JSValue NativeInputFlag(JSContext* ctx, JSValueConst, int argc,
                               JSValueConst* argv, int magic) {
    if (RefuseRetainedMutation(ctx, "input setter")) return JS_EXCEPTION;
    shell::EntityHandle handle = 0;
    if (argc < 2 || !JsHandle(ctx, argv[0], &handle)) return JS_EXCEPTION;
    shell::RetainedEntry* entry = LiveRetained(
        ctx, handle, shell::RetainedKind::Input, "input");
    if (!entry) return JS_EXCEPTION;
    shell::ScopeHostContext host = shell::ScopeCurrentHost();
    if (!host.IsSet()) return JS_ThrowTypeError(ctx, "input setter needs a live host call");
    bool value = JS_ToBool(ctx, argv[1]) != 0;
    if (magic == 0) entry->input->masked = value;
    else entry->input->loading = value;
    AppInvalidate(host.GetWindow());
    return JS_UNDEFINED;
}

static JSValue NativeTextareaRows(JSContext* ctx, JSValueConst, int argc,
                                  JSValueConst* argv, int magic) {
    if (RefuseRetainedMutation(ctx, "textarea setter")) return JS_EXCEPTION;
    shell::EntityHandle handle = 0;
    if (argc < 2 || !JsHandle(ctx, argv[0], &handle)) return JS_EXCEPTION;
    shell::RetainedEntry* entry = LiveRetained(
        ctx, handle, shell::RetainedKind::Textarea, "textarea");
    if (!entry) return JS_EXCEPTION;
    shell::ScopeHostContext host = shell::ScopeCurrentHost();
    if (!host.IsSet()) return JS_ThrowTypeError(ctx, "textarea setter needs a live host call");
    if (magic == 2) {
        entry->input->softWrap = JS_ToBool(ctx, argv[1]) != 0;
    } else {
        int32_t first = 0, second = 0;
        if (JS_ToInt32(ctx, &first, argv[1]) < 0 || first <= 0) {
            return JS_ThrowTypeError(ctx, "textarea rows must be positive");
        }
        if (magic == 0) {
            entry->input->mode.kind = LayoutModeKind::PlainText;
            LayoutModeSetRows(&entry->input->mode, first);
        } else {
            if (argc < 3 || JS_ToInt32(ctx, &second, argv[2]) < 0 ||
                second < first) {
                return JS_ThrowTypeError(ctx, "auto-grow max rows must not be below min rows");
            }
            entry->input->mode.kind = LayoutModeKind::AutoGrow;
            entry->input->mode.minRows = first;
            entry->input->mode.maxRows = second;
            LayoutModeSetRows(&entry->input->mode, first);
        }
    }
    AppInvalidate(host.GetWindow());
    return JS_UNDEFINED;
}

static bool ReadSliderValue(JSContext* ctx, JSValueConst value,
                            SliderValue* out) {
    int64_t count = 0;
    if (JS_GetLength(ctx, value, &count) < 0 || (count != 1 && count != 2)) {
        JS_ThrowTypeError(ctx, "slider value must contain one number or a [start, end] pair");
        return false;
    }
    double values[2] = {};
    for (int i = 0; i < (int)count; i++) {
        JSValue item = JS_GetPropertyUint32(ctx, value, (uint32_t)i);
        bool ok = !JS_IsException(item) &&
                  JS_ToFloat64(ctx, &values[i], item) == 0 &&
                  isfinite(values[i]);
        JS_FreeValue(ctx, item);
        if (!ok) {
            JS_ThrowTypeError(ctx, "slider values must be finite numbers");
            return false;
        }
    }
    *out = count == 1 ? SliderSingle((float)values[0])
                      : SliderRange((float)values[0], (float)values[1]);
    return true;
}

static JSValue SliderValueJs(JSContext* ctx, SliderValue value) {
    JSValue out = JS_NewArray(ctx);
    if (value.range) {
        JS_SetPropertyUint32(ctx, out, 0, JS_NewFloat64(ctx, value.lo));
        JS_SetPropertyUint32(ctx, out, 1, JS_NewFloat64(ctx, value.hi));
    } else {
        JS_SetPropertyUint32(ctx, out, 0, JS_NewFloat64(ctx, value.hi));
    }
    return out;
}

static JSValue NativeSliderStateNew(JSContext* ctx, JSValueConst, int argc,
                                    JSValueConst* argv) {
    if (RefuseRetainedCreation(ctx, "SliderState.new(...)")) return JS_EXCEPTION;
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    if (!impl || impl->retained.Len() >= shell::kMaxLiveEntities) {
        return JS_ThrowRangeError(ctx, "the application reached gpui-shell's retained entity limit; release unused handles");
    }
    double min = 0, max = 0, step = 0;
    Arena* arena = ArenaNew();
    Str scale;
    SliderValue value = {};
    bool ok = argc >= 5 && JS_ToFloat64(ctx, &min, argv[0]) == 0 &&
              JS_ToFloat64(ctx, &max, argv[1]) == 0 &&
              JS_ToFloat64(ctx, &step, argv[2]) == 0 &&
              JsString(ctx, argv[3], arena, &scale) &&
              ReadSliderValue(ctx, argv[4], &value);
    SliderScale nativeScale = StrEq(scale, "logarithmic")
                                  ? SliderScale::Logarithmic
                                  : SliderScale::Linear;
    ok = ok && isfinite(min) && isfinite(max) && isfinite(step) && max > min &&
         step > 0 && (nativeScale == SliderScale::Linear || min > 0) &&
         (StrEq(scale, "linear") || StrEq(scale, "logarithmic"));
    if (!ok && !JS_HasException(ctx)) {
        JS_ThrowTypeError(ctx, "SliderState.new needs a finite min below max, a positive step and a valid scale");
    }
    shell::ScopeHostContext host = shell::ScopeCurrentHost();
    shell::EntityHandle handle =
        ok && host.IsSet()
            ? impl->retained.CreateSlider(
                  (float)min, (float)max, (float)step, nativeScale, value,
                  host.GetApp(), shell::ScopeCurrentView(),
                  shell::ScopeCurrentApplication())
            : 0;
    ArenaDelete(arena);
    if (!ok) return JS_EXCEPTION;
    if (!host.IsSet()) return JS_ThrowTypeError(ctx, "SliderState.new(...) needs a live host call");
    if (!handle) return JS_ThrowInternalError(ctx, "creating slider state failed");
    return JS_NewInt64(ctx, (int64_t)handle);
}

static JSValue NativeSliderValue(JSContext* ctx, JSValueConst, int argc,
                                 JSValueConst* argv) {
    shell::EntityHandle handle = 0;
    if (argc < 1 || !JsHandle(ctx, argv[0], &handle)) return JS_EXCEPTION;
    shell::RetainedEntry* entry = LiveRetained(
        ctx, handle, shell::RetainedKind::Slider, "slider");
    if (!entry) return JS_EXCEPTION;
    shell::ScopeHostContext host = shell::ScopeCurrentHost();
    if (!host.IsSet()) return JS_ThrowTypeError(ctx, "value() needs a live host call");
    return SliderValueJs(ctx, entry->slider->value);
}

static JSValue NativeSliderSetValue(JSContext* ctx, JSValueConst, int argc,
                                    JSValueConst* argv) {
    if (RefuseRetainedMutation(ctx, "SliderState.set_value()")) return JS_EXCEPTION;
    shell::EntityHandle handle = 0;
    SliderValue value = {};
    if (argc < 2 || !JsHandle(ctx, argv[0], &handle) ||
        !ReadSliderValue(ctx, argv[1], &value)) return JS_EXCEPTION;
    shell::RetainedEntry* entry = LiveRetained(
        ctx, handle, shell::RetainedKind::Slider, "slider");
    if (!entry) return JS_EXCEPTION;
    shell::ScopeHostContext host = shell::ScopeCurrentHost();
    if (!host.IsSet()) return JS_ThrowTypeError(ctx, "set_value() needs a live host call");
    SliderSetValue(entry->slider, SliderValueClamp(
                                      value, entry->slider->min,
                                      entry->slider->max));
    AppInvalidate(host.GetWindow());
    return JS_UNDEFINED;
}

static JSValue NativeSliderBounds(JSContext* ctx, JSValueConst, int argc,
                                  JSValueConst* argv) {
    shell::EntityHandle handle = 0;
    if (argc < 1 || !JsHandle(ctx, argv[0], &handle)) return JS_EXCEPTION;
    shell::RetainedEntry* entry = LiveRetained(
        ctx, handle, shell::RetainedKind::Slider, "slider");
    if (!entry) return JS_EXCEPTION;
    shell::ScopeHostContext host = shell::ScopeCurrentHost();
    if (!host.IsSet()) return JS_ThrowTypeError(ctx, "min_value() needs a live host call");
    JSValue out = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, out, 0, JS_NewFloat64(ctx, entry->slider->min));
    JS_SetPropertyUint32(ctx, out, 1, JS_NewFloat64(ctx, entry->slider->max));
    JS_SetPropertyUint32(ctx, out, 2, JS_NewFloat64(ctx, entry->slider->step));
    return out;
}

static JSValue NativeOtpStateNew(JSContext* ctx, JSValueConst, int argc,
                                 JSValueConst* argv) {
    if (RefuseRetainedCreation(ctx, "OtpState.new(...)")) return JS_EXCEPTION;
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    if (!impl || impl->retained.Len() >= shell::kMaxLiveEntities) {
        return JS_ThrowRangeError(ctx, "the application reached gpui-shell's retained entity limit; release unused handles");
    }
    int32_t length = 0;
    if (argc < 1 || JS_ToInt32(ctx, &length, argv[0]) < 0 || length < 1 ||
        length > 64) {
        return JS_ThrowTypeError(ctx, "OtpState.new(length) expects a whole number between 1 and 64");
    }
    Arena* arena = ArenaNew();
    Str value;
    bool ok = OptionalJsString(ctx, argc > 1 ? argv[1] : JS_UNDEFINED,
                               arena, &value);
    bool masked = argc > 2 && JS_ToBool(ctx, argv[2]) != 0;
    shell::ScopeHostContext host = shell::ScopeCurrentHost();
    shell::EntityHandle handle =
        ok && host.IsSet()
            ? impl->retained.CreateOtp(
                  length, value, masked, host.GetApp(),
                  shell::ScopeCurrentView(), shell::ScopeCurrentApplication())
            : 0;
    ArenaDelete(arena);
    if (!ok) return JS_EXCEPTION;
    if (!host.IsSet()) return JS_ThrowTypeError(ctx, "OtpState.new(...) needs a live host call");
    if (!handle) return JS_ThrowInternalError(ctx, "creating OTP state failed");
    return JS_NewInt64(ctx, (int64_t)handle);
}

static JSValue NativeOtpValue(JSContext* ctx, JSValueConst, int argc,
                              JSValueConst* argv) {
    shell::EntityHandle handle = 0;
    if (argc < 1 || !JsHandle(ctx, argv[0], &handle)) return JS_EXCEPTION;
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    shell::RetainedEntry* entry = impl ? impl->retained.Find(handle) : nullptr;
    if (!entry || entry->kind != shell::RetainedKind::Otp) {
        return JS_ThrowTypeError(ctx, "this OTP state has been released");
    }
    shell::ScopeHostContext host = shell::ScopeCurrentHost();
    if (!host.IsSet()) return JS_ThrowTypeError(ctx, "value() needs a live host call");
    OtpState* state = entry->otp.Get(entry->app);
    if (!state) return JS_ThrowTypeError(ctx, "this OTP state has been released");
    return JS_NewStringLen(ctx, state->value, (size_t)state->len);
}

static JSValue NativeOtpSetValue(JSContext* ctx, JSValueConst, int argc,
                                 JSValueConst* argv) {
    if (RefuseRetainedMutation(ctx, "OtpState.set_value()")) return JS_EXCEPTION;
    shell::EntityHandle handle = 0;
    if (argc < 2 || !JsHandle(ctx, argv[0], &handle)) return JS_EXCEPTION;
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    shell::RetainedEntry* entry = impl ? impl->retained.Find(handle) : nullptr;
    if (!entry || entry->kind != shell::RetainedKind::Otp) {
        return JS_ThrowTypeError(ctx, "this OTP state has been released");
    }
    shell::ScopeHostContext host = shell::ScopeCurrentHost();
    if (!host.IsSet()) return JS_ThrowTypeError(ctx, "set_value() needs a live host call");
    OtpState* state = entry->otp.Get(entry->app);
    Arena* arena = ArenaNew();
    Str value;
    bool ok = state && JsString(ctx, argv[1], arena, &value);
    if (ok) {
        int n = value.len;
        if (n > (int)sizeof(state->value) - 1) n = (int)sizeof(state->value) - 1;
        if (n > 0) memcpy(state->value, value.s, (size_t)n);
        state->len = n;
        state->value[n] = 0;
    }
    ArenaDelete(arena);
    if (!ok) return JS_EXCEPTION;
    AppInvalidate(host.GetWindow());
    if (entry->owner.IsValid()) impl->owner->InvalidateScriptView(entry->owner);
    return JS_UNDEFINED;
}

static JSValue NativeOtpProperty(JSContext* ctx, JSValueConst, int argc,
                                 JSValueConst* argv, int magic) {
    shell::EntityHandle handle = 0;
    if (argc < 1 || !JsHandle(ctx, argv[0], &handle)) return JS_EXCEPTION;
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    shell::RetainedEntry* entry = impl ? impl->retained.Find(handle) : nullptr;
    OtpState* state = entry && entry->kind == shell::RetainedKind::Otp
                          ? entry->otp.Get(entry->app)
                          : nullptr;
    if (!state) return JS_ThrowTypeError(ctx, "this OTP state has been released");
    shell::ScopeHostContext host = shell::ScopeCurrentHost();
    if (!host.IsSet()) return JS_ThrowTypeError(ctx, "OTP state access needs a live host call");
    if (magic == 0) return JS_NewInt32(ctx, state->length);
    if (magic == 1) return JS_NewBool(ctx, state->masked);
    if (magic == 2) {
        if (RefuseRetainedMutation(ctx, "OtpState.set_masked()")) return JS_EXCEPTION;
        if (argc < 2) return JS_ThrowTypeError(ctx, "set_masked expects a boolean");
        state->masked = JS_ToBool(ctx, argv[1]) != 0;
        AppInvalidate(host.GetWindow());
        if (entry->owner.IsValid()) impl->owner->InvalidateScriptView(entry->owner);
        return JS_UNDEFINED;
    }
    if (RefuseRetainedMutation(ctx, "OtpState.focus()")) return JS_EXCEPTION;
    OtpFocus(state, host.GetApp(), host.GetWindow());
    FocusHandleFocus(host.GetWindow(), state->focus);
    AppInvalidate(host.GetWindow());
    return JS_UNDEFINED;
}

static bool RetainedEventOf(shell::RetainedKind kind, Str name,
                            shell::RetainedEvent* event, bool* replace) {
    *replace = false;
    if (kind == shell::RetainedKind::Input ||
        kind == shell::RetainedKind::Textarea) {
        if (StrEq(name, "change")) *event = shell::RetainedEvent::InputChange;
        else if (StrEq(name, "submit")) *event = shell::RetainedEvent::InputSubmit;
        else if (StrEq(name, "focus")) *event = shell::RetainedEvent::InputFocus;
        else if (StrEq(name, "blur")) *event = shell::RetainedEvent::InputBlur;
        else return false;
        return true;
    }
    if (kind == shell::RetainedKind::Slider) {
        if (StrEq(name, "change")) *event = shell::RetainedEvent::SliderChange;
        else if (StrEq(name, "release")) *event = shell::RetainedEvent::SliderRelease;
        else return false;
        return true;
    }
    if (kind == shell::RetainedKind::Otp) {
        *replace = true;
        if (StrEq(name, "change")) *event = shell::RetainedEvent::OtpChange;
        else if (StrEq(name, "complete")) *event = shell::RetainedEvent::OtpComplete;
        else if (StrEq(name, "focus")) *event = shell::RetainedEvent::OtpFocus;
        else if (StrEq(name, "blur")) *event = shell::RetainedEvent::OtpBlur;
        else return false;
        return true;
    }
    return false;
}

static JSValue NativeRetainedOn(JSContext* ctx, JSValueConst, int argc,
                                JSValueConst* argv) {
    if (RefuseRetainedMutation(ctx, "state.on()")) return JS_EXCEPTION;
    shell::EntityHandle handle = 0;
    if (argc < 3 || !JsHandle(ctx, argv[0], &handle) ||
        !JS_IsFunction(ctx, argv[2])) {
        return JS_ThrowTypeError(ctx, "state.on(event, handler) expects a function");
    }
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    shell::RetainedEntry* entry = impl ? impl->retained.Find(handle) : nullptr;
    if (!entry) return JS_ThrowTypeError(ctx, "this retained state has been released");
    shell::ScopeHostContext host = shell::ScopeCurrentHost();
    if (!host.IsSet()) {
        return JS_ThrowTypeError(ctx, "on(...) needs a live host call; subscribe from init() or an event handler");
    }
    Arena* arena = ArenaNew();
    Str name;
    bool converted = JsString(ctx, argv[1], arena, &name);
    shell::RetainedEvent event = {};
    bool replace = false;
    bool known = converted && RetainedEventOf(entry->kind, name, &event, &replace);
    ArenaDelete(arena);
    if (!converted) return JS_EXCEPTION;
    if (!known) return JS_ThrowTypeError(ctx, "unknown retained-state event name");
    shell::CallbackId callback = impl->callbacks.PushPersistent(
        ctx, argv[2], entry->owner, shell::ScopeCurrentPolicy(),
        (AppModule*)entry->application);
    if (callback == UINT64_MAX) return JS_ThrowInternalError(ctx, "callback id space is exhausted");
    shell::CallbackId replaced = 0;
    if (!impl->retained.AddCallback(handle, event, callback, replace,
                                    &replaced)) {
        impl->callbacks.RetireId(ctx, callback);
        return JS_ThrowTypeError(ctx, "this retained state has been released");
    }
    if (replaced) impl->callbacks.RetireId(ctx, replaced);
    return JS_TRUE;
}

static JSValue NativeRetainedRelease(JSContext* ctx, JSValueConst, int argc,
                                     JSValueConst* argv) {
    shell::EntityHandle handle = 0;
    if (argc < 1 || !JsHandle(ctx, argv[0], &handle)) return JS_EXCEPTION;
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    Vec<shell::CallbackId> callbacks;
    bool released = impl && impl->retained.Release(handle, &callbacks);
    for (int i = 0; impl && i < callbacks.len; i++) {
        impl->callbacks.RetireId(ctx, callbacks[i]);
    }
    callbacks.Reset();
    return JS_NewBool(ctx, released);
}

static JSValue NativeRetainedComponent(JSContext* ctx, JSValueConst,
                                       int argc, JSValueConst* argv) {
    shell::EntityHandle handle = 0;
    if (argc < 2 || !JsHandle(ctx, argv[1], &handle)) return JS_EXCEPTION;
    Arena* arena = ArenaNew();
    Str name;
    bool converted = JsString(ctx, argv[0], arena, &name);
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    shell::RetainedEntry* entry = impl ? impl->retained.Find(handle) : nullptr;
    shell::Component component = {};
    component.kind = ComponentKindOf(name);
    component.handle = handle;
    shell::RetainedKind expected = shell::RetainedKind::Input;
    if (component.kind == shell::ComponentKind::Textarea) expected = shell::RetainedKind::Textarea;
    else if (component.kind == shell::ComponentKind::Slider ||
             component.kind == shell::ComponentKind::SliderTrack ||
             component.kind == shell::ComponentKind::SliderIndicator ||
             component.kind == shell::ComponentKind::SliderThumb) expected = shell::RetainedKind::Slider;
    else if (component.kind == shell::ComponentKind::OtpInput) expected = shell::RetainedKind::Otp;
    bool valid = converted && entry && entry->kind == expected;
    ArenaDelete(arena);
    if (!converted) return JS_EXCEPTION;
    if (!valid) return JS_ThrowTypeError(ctx, "this retained state has been released or has the wrong type");
    return JS_NewUint32(ctx, impl->scratch->Push(component));
}

static JSValue NativeFocusNew(JSContext* ctx, JSValueConst, int,
                              JSValueConst*) {
    if (RefuseRetainedCreation(ctx, "cx.focus_handle()")) return JS_EXCEPTION;
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    shell::ScopeHostContext host = shell::ScopeCurrentHost();
    if (!impl || !host.IsSet()) return JS_ThrowTypeError(ctx, "cx.focus_handle() needs a live host call");
    if (impl->retained.Len() >= shell::kMaxLiveEntities) {
        return JS_ThrowRangeError(ctx, "the application reached gpui-shell's retained entity limit; release unused handles");
    }
    shell::EntityHandle handle = impl->retained.CreateFocus(
        host.GetApp(), shell::ScopeCurrentView(), shell::ScopeCurrentApplication());
    return handle ? JS_NewInt64(ctx, (int64_t)handle)
                  : JS_ThrowInternalError(ctx, "creating focus handle failed");
}

static JSValue NativeFocusOp(JSContext* ctx, JSValueConst, int argc,
                             JSValueConst* argv, int magic) {
    shell::EntityHandle handle = 0;
    if (argc < 1 || !JsHandle(ctx, argv[0], &handle)) return JS_EXCEPTION;
    shell::RetainedEntry* entry = LiveRetained(
        ctx, handle, shell::RetainedKind::Focus, "focus handle");
    if (!entry) return JS_EXCEPTION;
    shell::ScopeHostContext host = shell::ScopeCurrentHost();
    if (!host.IsSet()) return JS_ThrowTypeError(ctx, "focus operation needs a live host call");
    if (magic == 0) {
        if (RefuseRetainedMutation(ctx, "FocusHandle.focus()")) return JS_EXCEPTION;
        FocusHandleFocus(host.GetWindow(), entry->focus);
        AppInvalidate(host.GetWindow());
        return JS_UNDEFINED;
    }
    return JS_NewBool(ctx, FocusHandleIsFocused(host.GetWindow(), entry->focus));
}

static JSValue NativeVirtualScrollNew(JSContext* ctx, JSValueConst, int,
                                      JSValueConst*) {
    if (RefuseRetainedCreation(ctx, "VirtualListScrollHandle.new()")) return JS_EXCEPTION;
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    shell::ScopeHostContext host = shell::ScopeCurrentHost();
    if (!impl || !host.IsSet()) return JS_ThrowTypeError(ctx, "VirtualListScrollHandle.new() needs a live host call");
    if (impl->retained.Len() >= shell::kMaxLiveEntities) {
        return JS_ThrowRangeError(ctx, "the application reached gpui-shell's retained entity limit; release unused handles");
    }
    shell::EntityHandle handle = impl->retained.CreateVirtualScroll(
        host.GetApp(), shell::ScopeCurrentView(), shell::ScopeCurrentApplication());
    return handle ? JS_NewInt64(ctx, (int64_t)handle)
                  : JS_ThrowInternalError(ctx, "creating virtual scroll handle failed");
}

static JSValue NativeVirtualScrollOp(JSContext* ctx, JSValueConst, int argc,
                                     JSValueConst* argv, int magic) {
    shell::EntityHandle handle = 0;
    if (argc < 1 || !JsHandle(ctx, argv[0], &handle)) return JS_EXCEPTION;
    shell::RetainedEntry* entry = LiveRetained(
        ctx, handle, shell::RetainedKind::VirtualScroll, "scroll handle");
    if (!entry) return JS_EXCEPTION;
    if (magic == 1) {
        VirtualListScrollToBottomDeferred(&entry->scroll);
        return JS_UNDEFINED;
    }
    int32_t index = 0;
    if (argc < 2 || JS_ToInt32(ctx, &index, argv[1]) < 0 || index < 0) {
        return JS_ThrowTypeError(ctx, "scroll_to_item index must be non-negative");
    }
    Arena* arena = ArenaNew();
    Str strategy;
    bool ok = OptionalJsString(ctx, argc > 2 ? argv[2] : JS_UNDEFINED,
                               arena, &strategy);
    ScrollStrategy native = StrEq(strategy, "center") ? ScrollStrategy::Center
                                                       : ScrollStrategy::Top;
    if (strategy && !StrEq(strategy, "top") && !StrEq(strategy, "center")) {
        ok = false;
        JS_ThrowTypeError(ctx, "scroll strategy must be top or center");
    }
    ArenaDelete(arena);
    if (!ok) return JS_EXCEPTION;
    VirtualListScrollToItemDeferred(&entry->scroll, index, native);
    return JS_UNDEFINED;
}

static JSValue NativeVirtualList(JSContext* ctx, JSValueConst, int argc,
                                 JSValueConst* argv, int magic) {
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    if (!impl || argc < 5 || shell::ScopeCurrentPhase() == ScopePhase::Layout) {
        return JS_ThrowTypeError(ctx, "a virtual list cannot be built from inside another list's item renderer");
    }
    Arena* arena = ArenaNew();
    Str id;
    int64_t count64 = 0;
    bool ok = JsString(ctx, argv[0], arena, &id) &&
              JS_ToInt64(ctx, &count64, argv[1]) == 0 && count64 >= 0 &&
              count64 <= 1000000 && JS_IsFunction(ctx, argv[3]) &&
              JS_IsFunction(ctx, argv[4]);
    if (!ok) {
        ArenaDelete(arena);
        return JS_ThrowTypeError(ctx, "virtual list needs an id, a count up to 1000000, item sizes, get_key and render functions");
    }
    int count = (int)count64;
    if (!impl->scratch->ClaimVirtualItems((uint64_t)count, 1000000)) {
        ArenaDelete(arena);
        return JS_ThrowRangeError(ctx, "the virtual lists in one render may describe at most 1000000 items in total");
    }
    Size* sizes = count > 0 ? (Size*)Alloc(
                                  arena, (int)(sizeof(Size) * (size_t)count))
                            : nullptr;
    if (count > 0 && !sizes) {
        ArenaDelete(arena);
        return JS_ThrowOutOfMemory(ctx);
    }
    bool horizontal = magic != 0;
    if (JS_IsArray(argv[2])) {
        int64_t n = 0;
        ok = JS_GetLength(ctx, argv[2], &n) == 0 && n == count;
        for (int i = 0; ok && i < count; i++) {
            JSValue item = JS_GetPropertyUint32(ctx, argv[2], (uint32_t)i);
            double extent = 0;
            ok = !JS_IsException(item) &&
                 JS_ToFloat64(ctx, &extent, item) == 0 && isfinite(extent) &&
                 extent >= 0;
            JS_FreeValue(ctx, item);
            if (ok) sizes[i] = horizontal ? Size{(float)extent, 0}
                                          : Size{0, (float)extent};
        }
    } else {
        double extent = 0;
        ok = JS_ToFloat64(ctx, &extent, argv[2]) == 0 && isfinite(extent) &&
             extent >= 0;
        for (int i = 0; ok && i < count; i++) {
            sizes[i] = horizontal ? Size{(float)extent, 0}
                                  : Size{0, (float)extent};
        }
    }
    if (!ok) {
        ArenaDelete(arena);
        return JS_ThrowTypeError(ctx, "virtual-list item sizes must be one finite non-negative number or one per item");
    }
    shell::CallbackId getKey = impl->callbacks.Push(
        ctx, argv[3], shell::ScopeCurrentView(), shell::ScopeCurrentPolicy(),
        shell::ScopeCurrentGeneration(),
        (AppModule*)shell::ScopeCurrentApplication());
    shell::CallbackId render = impl->callbacks.Push(
        ctx, argv[4], shell::ScopeCurrentView(), shell::ScopeCurrentPolicy(),
        shell::ScopeCurrentGeneration(),
        (AppModule*)shell::ScopeCurrentApplication());
    if (getKey == UINT64_MAX || render == UINT64_MAX) {
        ArenaDelete(arena);
        return JS_ThrowInternalError(ctx, "virtual-list callbacks were registered outside a snapshot build");
    }
    shell::VirtualListSpec list = {};
    list.id = id;
    list.axis = horizontal ? Axis::Horizontal : Axis::Vertical;
    list.sizes = sizes;
    list.sizeCount = count;
    list.getKey = getKey;
    list.renderItems = render;
    shell::Component component = {};
    component.kind = horizontal ? shell::ComponentKind::HVirtualList
                                : shell::ComponentKind::VVirtualList;
    component.virtualList = &list;
    shell::SpecId result = impl->scratch->Push(component);
    ArenaDelete(arena);
    return JS_NewUint32(ctx, result);
}

static JSValue NativeNotify(JSContext* ctx, JSValueConst, int argc,
                            JSValueConst* argv) {
    uint64_t generation = 0;
    if (argc < 1 || JS_ToIndex(ctx, &generation, argv[0]) < 0) {
        return JS_EXCEPTION;
    }
    ShellError error = {};
    shell::ScopeHostContext host =
        shell::ScopeHostForGeneration(generation, &error);
    if (!host.IsSet()) {
        JSValue result = JS_ThrowTypeError(ctx, "%.*s", error.message.len,
                                           error.message.s);
        ShellErrorClear(&error);
        return result;
    }
    if (!ScopePhaseAllowsNotify(shell::ScopeCurrentPhase())) {
        return JS_ThrowTypeError(ctx, "cx.notify() is available only from an event handler or task, not during render or layout");
    }
    EntityId view = shell::ScopeCurrentView();
    if (!view.IsValid()) {
        return JS_ThrowTypeError(ctx, "cx.notify() needs a current script view");
    }
    ShellRuntime* runtime = shell::ScopeCurrentRuntime();
    if (runtime) runtime->InvalidateScriptView(view);
    NotifyEntity(host.GetApp(), view, host.GetWindow());
    return JS_UNDEFINED;
}

static void SetGlobalFunction(JSContext* ctx, JSValueConst global,
                              const char* name, JSCFunction* function,
                              int length) {
    JS_SetPropertyStr(ctx, global, name,
                      JS_NewCFunction(ctx, function, name, length));
}

static void SetGlobalMagicFunction(JSContext* ctx, JSValueConst global,
                                   const char* name,
                                   JSCFunctionMagic* function, int length,
                                   int magic) {
    JS_SetPropertyStr(ctx, global, name,
                      JS_NewCFunctionMagic(ctx, function, name, length,
                                           JS_CFUNC_generic_magic, magic));
}

static const char kPrelude[] = R"JS(
globalThis.__gpui = (() => {
  const explicit = Object.create(null);
  const element = (id) => {
    let object;
    const target = { __id: id };
    object = new Proxy(target, {
      get(receiver, name) {
        if (name in receiver) return receiver[name];
        if (name in explicit) return explicit[name].bind(object);
        if (typeof name !== "string") return undefined;
        return (...args) => { __apply(id, name, args); return object; };
      },
    });
    return object;
  };
  const childId = (child) => {
    if (typeof child?.__id === "number") return child.__id;
    if (["string", "number", "boolean"].includes(typeof child)) {
      return __component("text", String(child));
    }
    throw new TypeError("child(value) expects an element or primitive text");
  };
  explicit.child = function (child) { __attach(this.__id, childId(child)); return this; };
  explicit.children = function (children) {
    for (const child of children) __attach(this.__id, childId(child));
    return this;
  };
  explicit.track_scroll = function (handle) {
    if (typeof handle?.__handle !== "number") throw new TypeError("track_scroll(handle) expects a VirtualListScrollHandle");
    __apply(this.__id, "track_scroll", [handle.__handle]);
    return this;
  };
  for (const name of ["content", "trigger", "input", "decrement_button", "increment_button"]) {
    explicit[name] = function (value) { __slot(this.__id, name, childId(value)); return this; };
  }
  for (const name of ["hover", "active", "focus", "range_style", "cell_style", "cell_active_style", "caret_style"]) {
    explicit[name] = function (declare) {
      if (typeof declare !== "function") throw new TypeError(name + "(declare) expects a function");
      declare(element(__state(this.__id, name)));
      return this;
    };
  }
  class View {}
  globalThis.__construct = (Class) => new Class();
  globalThis.__initialize = (instance, cx) => {
    if (typeof instance.init === "function") instance.init(undefined, cx);
  };
  globalThis.__context = (generation) => Object.freeze({
    notify: () => __cx_notify(generation),
    focus_handle: () => focusHandle(__focus_handle_new()),
  });
  const component = (kind, text, handle, index) =>
    element(__component(kind, text, handle, index));
  const named = (kind) => ({ new: (id) => component(kind, String(id)) });
  const plain = (kind) => ({ new: () => component(kind) });
  const retained = (kind) => ({
    new: (state) => element(__retained_component(kind, state?.__handle)),
  });
  const inputState = (handle) => ({
    __handle: handle,
    value: () => __input_value(handle),
    set_value: (value) => __input_set_value(handle, String(value ?? "")),
    set_step: (value) => __input_set_step(handle, value == null ? null : Number(value)),
    set_min: (value) => __input_set_min(handle, value == null ? null : Number(value)),
    set_max: (value) => __input_set_max(handle, value == null ? null : Number(value)),
    set_masked: (value) => __input_set_masked(handle, Boolean(value)),
    set_loading: (value) => __input_set_loading(handle, Boolean(value)),
    on: (event, handler) => __input_on(handle, String(event), handler),
    release: () => __input_release(handle),
  });
  const textareaState = (handle) => ({
    __handle: handle,
    value: () => __textarea_value(handle),
    set_value: (value) => __textarea_set_value(handle, String(value ?? "")),
    set_rows: (rows) => __textarea_set_rows(handle, Number(rows)),
    set_auto_grow: (min, max) => __textarea_set_auto_grow(handle, Number(min), Number(max)),
    set_soft_wrap: (value) => __textarea_set_soft_wrap(handle, Boolean(value)),
    on: (event, handler) => __textarea_on(handle, String(event), handler),
    release: () => __textarea_release(handle),
  });
  const sliderValues = (value) => Array.isArray(value) ? value : [value];
  const sliderState = (handle) => ({
    __handle: handle,
    value: () => { const values = __slider_value(handle); return values.length === 1 ? values[0] : values; },
    set_value: (value) => __slider_set_value(handle, sliderValues(value)),
    min_value: () => __slider_bounds(handle)[0],
    max_value: () => __slider_bounds(handle)[1],
    step_value: () => __slider_bounds(handle)[2],
    on: (event, handler) => __slider_on(handle, String(event), handler),
    release: () => __slider_release(handle),
  });
  const otpState = (handle) => ({
    __handle: handle,
    value: () => __otp_value(handle),
    set_value: (value) => __otp_set_value(handle, String(value ?? "")),
    len: () => __otp_len(handle),
    is_masked: () => __otp_is_masked(handle),
    set_masked: (value) => __otp_set_masked(handle, Boolean(value)),
    focus: () => __otp_focus(handle),
    on: (event, handler) => __otp_on(handle, String(event), handler),
    release: () => __otp_release(handle),
  });
  const focusHandle = (handle) => ({
    __handle: handle,
    focus: () => __focus_focus(handle),
    is_focused: () => __focus_is_focused(handle),
    release: () => __focus_release(handle),
  });
  const virtualScrollHandle = (handle) => ({
    __handle: handle,
    scroll_to_item: (index, strategy = "top") => __virtual_scroll_to_item(handle, Number(index), String(strategy)),
    scroll_to_bottom: () => __virtual_scroll_to_bottom(handle),
    release: () => __virtual_scroll_release(handle),
  });
  const virtualList = (build, name) => (id, count, sizes, getKey, render) => {
    if (!Number.isInteger(count) || count < 0) throw new TypeError(name + " item_count must be a non-negative whole number");
    if (typeof getKey !== "function" || typeof render !== "function") throw new TypeError(name + " needs get_key and render functions");
    if (Array.isArray(sizes) && sizes.length !== count) throw new TypeError(name + " needs one size per item");
    return element(build(String(id), count, sizes, getKey, render));
  };
  const api = {
    View,
    div: () => component("div"),
    h_flex: () => component("h_flex"),
    v_flex: () => component("v_flex"),
    svg: (path) => component("svg", String(path)),
    image: (path) => component("image", String(path)),
    PathBuilder: Object.freeze({}),
    Background: Object.freeze({ solid: (color) => String(color) }),
    Button: named("Button"), Link: named("Link"),
    Checkbox: named("Checkbox"), Switch: named("Switch"),
    Tabs: named("Tabs"), Tab: named("Tab"), Progress: named("Progress"),
    ProgressTrack: plain("ProgressTrack"), ProgressIndicator: plain("ProgressIndicator"),
    Radio: named("Radio"), Toggle: named("Toggle"),
    RadioGroup: named("RadioGroup"), ToggleGroup: named("ToggleGroup"),
    Table: named("Table"), TableHeader: named("TableHeader"),
    TableBody: named("TableBody"), TableCaption: named("TableCaption"),
    TableRow: { new: (id, index) => component("TableRow", String(id), undefined, index) },
    TableHead: { new: (id, index) => component("TableHead", String(id), undefined, index) },
    TableCell: { new: (id, index) => component("TableCell", String(id), undefined, index) },
    h_resizable: (id) => component("h_resizable", String(id)),
    v_resizable: (id) => component("v_resizable", String(id)),
    resizable_panel: () => component("ResizablePanel"),
    Collapsible: plain("Collapsible"), Popover: named("Popover"),
    HoverCard: named("HoverCard"), Popup: named("Popup"),
    Select: named("Select"), Combobox: named("Combobox"),
    DatePicker: { new: (id, focus) => component("DatePicker", String(id), focus?.__handle) },
    Scrollbar: named("Scrollbar"),
    v_virtual_list: virtualList(__v_virtual_list, "v_virtual_list"),
    h_virtual_list: virtualList(__h_virtual_list, "h_virtual_list"),
    VirtualListScrollHandle: { new: () => virtualScrollHandle(__virtual_scroll_new()) },
    InputState: { new: (options = {}) => inputState(__input_state_new(options.placeholder ?? null, options.value ?? null)) }, Input: retained("Input"),
    NumberInput: retained("NumberInput"),
    TextareaState: { new: (options = {}) => textareaState(__textarea_state_new(options.placeholder ?? null, options.value ?? null, options.rows ?? null)) }, Textarea: retained("Textarea"),
    SliderState: { new: (options = {}) => sliderState(__slider_state_new(options.min ?? 0, options.max ?? 100, options.step ?? 1, String(options.scale ?? "linear"), sliderValues(options.value ?? options.min ?? 0))) }, Slider: retained("Slider"),
    SliderTrack: retained("SliderTrack"), SliderIndicator: retained("SliderIndicator"),
    SliderThumb: retained("SliderThumb"),
    OtpState: { new: (length, options = {}) => otpState(__otp_state_new(Number(length), options.value ?? null, Boolean(options.masked))) }, OtpInput: retained("OtpInput"),
    fps_monitor: () => component("FpsMonitor"),
    set_theme: () => { throw new Error("set_theme is not installed yet"); },
  };
  return Object.freeze(api);
})();
)JS";

static bool InstallRuntime(ShellRuntimeImpl* impl, ShellError* error) {
    JSValue global = JS_GetGlobalObject(impl->context);
    SetGlobalFunction(impl->context, global, "__component", NativeComponent, 4);
    SetGlobalFunction(impl->context, global, "__attach", NativeAttach, 2);
    SetGlobalFunction(impl->context, global, "__state", NativeState, 2);
    SetGlobalFunction(impl->context, global, "__slot", NativeSlot, 3);
    SetGlobalFunction(impl->context, global, "__apply", NativeApply, 3);
    SetGlobalFunction(impl->context, global, "__cx_notify", NativeNotify, 1);
    SetGlobalFunction(impl->context, global, "__input_state_new",
                      NativeInputStateNew, 2);
    SetGlobalFunction(impl->context, global, "__textarea_state_new",
                      NativeTextareaStateNew, 3);
    SetGlobalFunction(impl->context, global, "__input_value", NativeInputValue,
                      1);
    SetGlobalFunction(impl->context, global, "__textarea_value",
                      NativeInputValue, 1);
    SetGlobalFunction(impl->context, global, "__input_set_value",
                      NativeInputSetValue, 2);
    SetGlobalFunction(impl->context, global, "__textarea_set_value",
                      NativeInputSetValue, 2);
    SetGlobalMagicFunction(impl->context, global, "__input_set_step",
                           NativeInputNumberOption, 2, 0);
    SetGlobalMagicFunction(impl->context, global, "__input_set_min",
                           NativeInputNumberOption, 2, 1);
    SetGlobalMagicFunction(impl->context, global, "__input_set_max",
                           NativeInputNumberOption, 2, 2);
    SetGlobalMagicFunction(impl->context, global, "__input_set_masked",
                           NativeInputFlag, 2, 0);
    SetGlobalMagicFunction(impl->context, global, "__input_set_loading",
                           NativeInputFlag, 2, 1);
    SetGlobalMagicFunction(impl->context, global, "__textarea_set_rows",
                           NativeTextareaRows, 2, 0);
    SetGlobalMagicFunction(impl->context, global, "__textarea_set_auto_grow",
                           NativeTextareaRows, 3, 1);
    SetGlobalMagicFunction(impl->context, global, "__textarea_set_soft_wrap",
                           NativeTextareaRows, 2, 2);
    SetGlobalFunction(impl->context, global, "__slider_state_new",
                      NativeSliderStateNew, 5);
    SetGlobalFunction(impl->context, global, "__slider_value",
                      NativeSliderValue, 1);
    SetGlobalFunction(impl->context, global, "__slider_set_value",
                      NativeSliderSetValue, 2);
    SetGlobalFunction(impl->context, global, "__slider_bounds",
                      NativeSliderBounds, 1);
    SetGlobalFunction(impl->context, global, "__otp_state_new",
                      NativeOtpStateNew, 3);
    SetGlobalFunction(impl->context, global, "__otp_value", NativeOtpValue, 1);
    SetGlobalFunction(impl->context, global, "__otp_set_value",
                      NativeOtpSetValue, 2);
    SetGlobalMagicFunction(impl->context, global, "__otp_len",
                           NativeOtpProperty, 1, 0);
    SetGlobalMagicFunction(impl->context, global, "__otp_is_masked",
                           NativeOtpProperty, 1, 1);
    SetGlobalMagicFunction(impl->context, global, "__otp_set_masked",
                           NativeOtpProperty, 2, 2);
    SetGlobalMagicFunction(impl->context, global, "__otp_focus",
                           NativeOtpProperty, 1, 3);
    SetGlobalFunction(impl->context, global, "__input_on", NativeRetainedOn,
                      3);
    SetGlobalFunction(impl->context, global, "__textarea_on", NativeRetainedOn,
                      3);
    SetGlobalFunction(impl->context, global, "__slider_on", NativeRetainedOn,
                      3);
    SetGlobalFunction(impl->context, global, "__otp_on", NativeRetainedOn, 3);
    SetGlobalFunction(impl->context, global, "__input_release",
                      NativeRetainedRelease, 1);
    SetGlobalFunction(impl->context, global, "__textarea_release",
                      NativeRetainedRelease, 1);
    SetGlobalFunction(impl->context, global, "__slider_release",
                      NativeRetainedRelease, 1);
    SetGlobalFunction(impl->context, global, "__otp_release",
                      NativeRetainedRelease, 1);
    SetGlobalFunction(impl->context, global, "__retained_component",
                      NativeRetainedComponent, 2);
    SetGlobalFunction(impl->context, global, "__focus_handle_new",
                      NativeFocusNew, 0);
    SetGlobalMagicFunction(impl->context, global, "__focus_focus",
                           NativeFocusOp, 1, 0);
    SetGlobalMagicFunction(impl->context, global, "__focus_is_focused",
                           NativeFocusOp, 1, 1);
    SetGlobalFunction(impl->context, global, "__focus_release",
                      NativeRetainedRelease, 1);
    SetGlobalFunction(impl->context, global, "__virtual_scroll_new",
                      NativeVirtualScrollNew, 0);
    SetGlobalMagicFunction(impl->context, global,
                           "__virtual_scroll_to_item", NativeVirtualScrollOp,
                           3, 0);
    SetGlobalMagicFunction(impl->context, global,
                           "__virtual_scroll_to_bottom", NativeVirtualScrollOp,
                           1, 1);
    SetGlobalFunction(impl->context, global, "__virtual_scroll_release",
                      NativeRetainedRelease, 1);
    SetGlobalMagicFunction(impl->context, global, "__v_virtual_list",
                           NativeVirtualList, 5, 0);
    SetGlobalMagicFunction(impl->context, global, "__h_virtual_list",
                           NativeVirtualList, 5, 1);
    JS_FreeValue(impl->context, global);
    BeginExecution(impl);
    JSValue result = JS_Eval(impl->context, kPrelude, sizeof(kPrelude) - 1,
                             "<gpui-shell prelude>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) return CaptureException(impl, error);
    JS_FreeValue(impl->context, result);
    return true;
}

ShellRuntime::ShellRuntime() = default;

ShellRuntime::~ShellRuntime() {
    if (control) control->runtime = nullptr;
    if (impl) {
        if (impl->context) {
            Vec<shell::CallbackId> retired;
            impl->retained.Clear(&retired);
            for (int i = 0; i < retired.len; i++) {
                impl->callbacks.RetireId(impl->context, retired[i]);
            }
            retired.Reset();
            impl->callbacks.Clear(impl->context);
        }
        delete impl->scratch;
        for (int i = 0; i < impl->modules.len; i++) {
            StrFree(impl->modules[i]->root);
            delete impl->modules[i];
        }
        impl->modules.Reset();
        impl->views.Reset();
        if (impl->context) {
            JS_SetContextOpaque(impl->context, nullptr);
            JS_FreeContext(impl->context);
        }
        if (impl->jsRuntime) JS_FreeRuntime(impl->jsRuntime);
        delete impl;
    }
    ControlRelease(control);
}

ShellRuntime* ShellRuntime::New(App*, ShellError* error) {
    ShellErrorClear(error);
    ShellRuntime* runtime = new ShellRuntime();
    runtime->impl = new ShellRuntimeImpl();
    runtime->impl->owner = runtime;
    runtime->control = new ShellRuntimeControl();
    runtime->control->runtime = runtime;
    runtime->impl->scratch = new shell::SpecArena();
    runtime->impl->jsRuntime = JS_NewRuntime();
    if (!runtime->impl->jsRuntime) {
        SetError(error, StrL("could not create the QuickJS runtime"));
        runtime->Release();
        return nullptr;
    }
    JS_SetMemoryLimit(runtime->impl->jsRuntime, 256u * 1024u * 1024u);
    JS_SetMaxStackSize(runtime->impl->jsRuntime, 1024u * 1024u);
    JS_SetInterruptHandler(runtime->impl->jsRuntime, Interrupt, runtime->impl);
    JS_SetModuleLoaderFunc(runtime->impl->jsRuntime, ModuleNormalize, ModuleLoad,
                           runtime->impl);
    runtime->impl->context = JS_NewContext(runtime->impl->jsRuntime);
    if (!runtime->impl->context) {
        SetError(error, StrL("could not create the QuickJS context"));
        runtime->Release();
        return nullptr;
    }
    JS_SetContextOpaque(runtime->impl->context, runtime->impl);
    if (!InstallRuntime(runtime->impl, error)) {
        runtime->Release();
        return nullptr;
    }
    return runtime;
}

ShellRuntime* ShellRuntime::Retain() {
    refs++;
    return this;
}

void ShellRuntime::Release() {
    if (--refs == 0) delete this;
}

static ViewType* LoadModule(ShellRuntime* runtime, Str name, Str source,
                            AppModule* application, ShellError* error) {
    ShellRuntimeImpl* impl = ShellRuntimeAccess::Impl(runtime);
    BeginExecution(impl);
    JSValue module = JS_Eval(
        impl->context, source.s, (size_t)source.len, name.s,
        JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    if (JS_IsException(module)) {
        CaptureException(impl, error);
        return nullptr;
    }
    JSModuleDef* definition = (JSModuleDef*)JS_VALUE_GET_PTR(module);
    JSValue evaluated = JS_EvalFunction(impl->context,
                                        JS_DupValue(impl->context, module));
    if (JS_IsException(evaluated)) {
        JS_FreeValue(impl->context, module);
        CaptureException(impl, error);
        return nullptr;
    }
    bool complete = Await(impl, evaluated, error);
    JS_FreeValue(impl->context, evaluated);
    if (!complete) {
        JS_FreeValue(impl->context, module);
        return nullptr;
    }
    JSValue nameSpace = JS_GetModuleNamespace(impl->context, definition);
    JSValue value = JS_GetPropertyStr(impl->context, nameSpace, "default");
    JS_FreeValue(impl->context, nameSpace);
    JS_FreeValue(impl->context, module);
    if (JS_IsException(value)) {
        CaptureException(impl, error);
        return nullptr;
    }
    if (!JS_IsFunction(impl->context, value)) {
        JS_FreeValue(impl->context, value);
        SetError(error, StrL("main.js must `export default` a class that extends View"));
        return nullptr;
    }
    ViewType* type = new ViewType();
    type->runtime = runtime->Retain();
    type->value = value;
    type->application = application;
    return type;
}

ViewType* ShellRuntime::LoadSource(Str name, Str source, ShellError* error) {
    ShellErrorClear(error);
    if (!name.s || name.len == 0) name = StrL("<module>");
    Str ownedName = StrDup(name);
    ViewType* result = LoadModule(this, ownedName, source, nullptr, error);
    StrFree(ownedName);
    return result;
}

ViewType* ShellRuntime::LoadApp(Str directory, Str entry, ShellError* error) {
    ShellErrorClear(error);
    char dir[kMaxPath] = {};
    if (directory.len <= 0 || directory.len >= kMaxPath) {
        SetError(error, StrL("application directory is empty or too long"));
        return nullptr;
    }
    char input[kMaxPath] = {};
    memcpy(input, directory.s, (size_t)directory.len);
    if (!PlatCanonicalPath(input, dir, kMaxPath) || !PlatDirExists(dir)) {
        SetError(error, fmt("application directory `%s` does not exist", directory));
        return nullptr;
    }
    char entryPath[kMaxPath] = {};
    int n = snprintf(entryPath, sizeof(entryPath), "%s/%.*s", dir, entry.len,
                     entry.s);
    char canonical[kMaxPath] = {};
    if (n <= 0 || n >= kMaxPath ||
        !PlatCanonicalPath(entryPath, canonical, kMaxPath) ||
        !PlatFileExists(canonical) || !WithinRoot(Str(dir), Str(canonical))) {
        SetError(error, fmt("entry module `%s` is not a file inside `%s`", entry,
                            directory));
        return nullptr;
    }
    AppModule* application = new AppModule();
    application->root = StrDup(Str(dir));
    application->generation = impl->nextModuleGeneration++;
    if (application->generation == 0) {
        application->generation = impl->nextModuleGeneration++;
    }
    impl->modules.Append(application);

    Str source = {};
    if (!ReadFileBounded(Str(canonical), &source, error)) return nullptr;
    Str tagged = StrDup(fmt("%s?v=%u", Str(canonical), application->generation));
    ViewType* result = LoadModule(this, tagged, source, application, error);
    StrFree(tagged);
    Free(nullptr, source.s);
    return result;
}

ViewObject* ShellRuntime::Instantiate(ViewType* type, Window* window, App* app,
                                      Policy* policy, ShellError* error,
                                      EntityId view) {
    ShellErrorClear(error);
    if (!type || type->runtime != this || !window || !app) {
        SetError(error, StrL("instantiate needs a view type from this runtime and a live Window/App"));
        return nullptr;
    }
    shell::CallScopeGuard scope =
        shell::ScopeEnter(window, app, ScopePhase::Event, view, policy, this,
                          type->application);
    uint32_t retainedCheckpoint = impl->retained.Checkpoint();
    BeginExecution(impl);
    JSValue object =
        JS_CallConstructor(impl->context, type->value, 0, nullptr);
    if (JS_IsException(object)) {
        Vec<shell::CallbackId> retired;
        impl->retained.Rollback(retainedCheckpoint, &retired);
        for (int i = 0; i < retired.len; i++) {
            impl->callbacks.RetireId(impl->context, retired[i]);
        }
        retired.Reset();
        CaptureException(impl, error);
        return nullptr;
    }
    JSValue global = JS_GetGlobalObject(impl->context);
    JSValue initialize = JS_GetPropertyStr(impl->context, global, "__initialize");
    JSValue context = JS_GetPropertyStr(impl->context, global, "__context");
    JSValue generation = JS_NewInt64(impl->context, (int64_t)scope.Generation());
    JSValue cx = JS_Call(impl->context, context, JS_UNDEFINED, 1, &generation);
    JSValue args[2] = {object, cx};
    JSValue initialized = JS_Call(impl->context, initialize, JS_UNDEFINED, 2, args);
    JS_FreeValue(impl->context, generation);
    JS_FreeValue(impl->context, cx);
    JS_FreeValue(impl->context, context);
    JS_FreeValue(impl->context, initialize);
    JS_FreeValue(impl->context, global);
    if (JS_IsException(initialized)) {
        JS_FreeValue(impl->context, object);
        Vec<shell::CallbackId> retired;
        impl->retained.Rollback(retainedCheckpoint, &retired);
        for (int i = 0; i < retired.len; i++) {
            impl->callbacks.RetireId(impl->context, retired[i]);
        }
        retired.Reset();
        CaptureException(impl, error);
        return nullptr;
    }
    JS_FreeValue(impl->context, initialized);
    ViewObject* result = new ViewObject();
    result->runtime = Retain();
    result->value = object;
    result->application = type->application;
    return result;
}

static bool ElementId(JSContext* ctx, JSValueConst value, shell::SpecId* id) {
    if (!JS_IsObject(value)) {
        JS_ThrowTypeError(ctx, "render(cx) must return an element");
        return false;
    }
    JSValue property = JS_GetPropertyStr(ctx, value, "__id");
    bool ok = !JS_IsException(property) && JS_ToUint32(ctx, id, property) == 0;
    JS_FreeValue(ctx, property);
    if (!ok) JS_ThrowTypeError(ctx, "render(cx) must return an element");
    return ok;
}

RenderSnapshot* ShellRuntime::BuildSnapshot(ViewObject* object, Window* window,
                                            App* app, EntityId view,
                                            Policy* policy,
                                            ShellError* error) {
    ShellErrorClear(error);
    if (!object || object->runtime != this || !window || !app) {
        SetError(error, StrL("snapshot build needs a view object from this runtime and a live Window/App"));
        return nullptr;
    }
    impl->scratch->Reset();
    uint64_t callbackGeneration = impl->callbacks.Begin(impl->context);
    double started = TimeNow();
    shell::SpecId root = 0;
    bool succeeded = false;
    {
        shell::CallScopeGuard scope = shell::ScopeEnter(
            window, app, ScopePhase::Render, view, policy, this,
            object->application);
        BeginExecution(impl);
        JSValue render = JS_GetPropertyStr(impl->context, object->value, "render");
        if (!JS_IsException(render) && JS_IsFunction(impl->context, render)) {
            JSValue global = JS_GetGlobalObject(impl->context);
            JSValue context = JS_GetPropertyStr(impl->context, global, "__context");
            JSValue generation =
                JS_NewInt64(impl->context, (int64_t)scope.Generation());
            JSValue cx =
                JS_Call(impl->context, context, JS_UNDEFINED, 1, &generation);
            JSValue produced = JS_Call(impl->context, render, object->value, 1, &cx);
            JS_FreeValue(impl->context, generation);
            JS_FreeValue(impl->context, cx);
            JS_FreeValue(impl->context, context);
            JS_FreeValue(impl->context, global);
            if (!JS_IsException(produced)) {
                succeeded = ElementId(impl->context, produced, &root);
            }
            JS_FreeValue(impl->context, produced);
        } else if (!JS_IsException(render)) {
            JS_ThrowTypeError(impl->context, "view class has no render(cx) method");
        }
        JS_FreeValue(impl->context, render);
    }
    double elapsed = TimeNow() - started;
    shell::MetricsAdd(&impl->metrics, shell::MetricsTimerKind::ScriptRender,
                      elapsed <= 0 ? 0 : (uint64_t)(elapsed * 1e9));
    if (!succeeded) {
        impl->callbacks.Abort(impl->context);
        impl->scratch->Reset();
        CaptureException(impl, error);
        return nullptr;
    }
    impl->callbacks.Commit();
    shell::SpecArena* published = impl->scratch;
    impl->scratch = new shell::SpecArena();
    return new RenderSnapshot(callbackGeneration, root, published,
                              SnapshotLease(this));
}

Str ShellRuntime::RenderToSpec(Arena* into, ViewObject* object, Window* window,
                               App* app, EntityId view, Policy* policy,
                               ShellError* error) {
    RenderSnapshot* snapshot =
        BuildSnapshot(object, window, app, view, policy, error);
    if (!snapshot) return {};
    Str result = snapshot->DebugTree(into);
    delete snapshot;
    return result;
}

bool ShellRuntime::Eval(Str source, Str name, ShellError* error) {
    ShellErrorClear(error);
    BeginExecution(impl);
    Str file = name ? name : StrL("<eval>");
    Str owned = StrDup(file);
    JSValue value = JS_Eval(impl->context, source.s, (size_t)source.len,
                            owned.s, JS_EVAL_TYPE_GLOBAL);
    StrFree(owned);
    if (JS_IsException(value)) return CaptureException(impl, error);
    bool complete = Await(impl, value, error);
    JS_FreeValue(impl->context, value);
    return complete;
}

bool ShellRuntime::DrainJobs(int limit, ShellError* error) {
    ShellErrorClear(error);
    if (limit < 0) limit = 0;
    int count = 0;
    while (JS_IsJobPending(impl->jsRuntime) && count++ < limit) {
        JSContext* context = nullptr;
        if (JS_ExecutePendingJob(impl->jsRuntime, &context) < 0) {
            return CaptureException(impl, error);
        }
    }
    if (JS_IsJobPending(impl->jsRuntime)) {
        SetError(error, StrL("the QuickJS job queue exceeded its drain limit"));
        return false;
    }
    return true;
}

RuntimeMetrics ShellRuntime::ReadMetrics() const {
    return shell::MetricsRead(&impl->metrics);
}

void ShellRuntime::RecordMaterialize(uint64_t nanos) {
    shell::MetricsAdd(&impl->metrics, shell::MetricsTimerKind::Materialize,
                      nanos);
}

int ShellRuntime::LiveCallbacks() const { return impl->callbacks.Live(); }

int ShellRuntime::LiveEntities() const { return impl->retained.Len(); }

shell::RetainedEntry* ShellRuntime::Retained(
    shell::EntityHandle handle) const {
    return impl->retained.Find(handle);
}

void ShellRuntime::RegisterScriptView(EntityId view, bool* dirty) {
    if (!view.IsValid() || !dirty) return;
    for (int i = 0; i < impl->views.len; i++) {
        if (impl->views[i].view == view && impl->views[i].dirty == dirty) return;
    }
    impl->views.Append({view, dirty});
}

void ShellRuntime::UnregisterScriptView(EntityId view, bool* dirty) {
    for (int i = 0; i < impl->views.len; i++) {
        if (impl->views[i].view != view || impl->views[i].dirty != dirty) continue;
        for (int j = i + 1; j < impl->views.len; j++) {
            impl->views[j - 1] = impl->views[j];
        }
        impl->views.len--;
        return;
    }
}

void ShellRuntime::InvalidateScriptView(EntityId view) {
    for (int i = 0; i < impl->views.len; i++) {
        if (impl->views[i].view == view && impl->views[i].dirty) {
            *impl->views[i].dirty = true;
        }
    }
}

void ShellRuntime::ReleaseOwnedEntities(EntityId view) {
    Vec<shell::CallbackId> callbacks;
    impl->retained.ReleaseOwner(view, &callbacks);
    for (int i = 0; i < callbacks.len; i++) {
        impl->callbacks.RetireId(impl->context, callbacks[i]);
    }
    callbacks.Reset();
}

static JSValue ContextObject(ShellRuntimeImpl* impl, uint64_t generation) {
    JSValue global = JS_GetGlobalObject(impl->context);
    JSValue make = JS_GetPropertyStr(impl->context, global, "__context");
    JSValue value = JS_NewInt64(impl->context, (int64_t)generation);
    JSValue result = JS_Call(impl->context, make, JS_UNDEFINED, 1, &value);
    JS_FreeValue(impl->context, value);
    JS_FreeValue(impl->context, make);
    JS_FreeValue(impl->context, global);
    return result;
}

static void Dispatch(ShellRuntime* runtime, shell::CallbackId id,
                     JSValue payload, Window* window, App* app) {
    ShellRuntimeImpl* impl = ShellRuntimeAccess::Impl(runtime);
    CallbackEntry* entry = impl->callbacks.Get(id);
    if (!entry || !window || !app) {
        JS_FreeValue(impl->context, payload);
        return;
    }
    if (entry->view.IsValid() && !EntityGet(app, entry->view)) {
        JS_FreeValue(impl->context, payload);
        return;
    }
    shell::CallScopeGuard scope = shell::ScopeEnter(
        window, app, ScopePhase::Event, entry->view, entry->policy, runtime,
        entry->application);
    BeginExecution(impl);
    JSValue cx = ContextObject(impl, scope.Generation());
    JSValue args[2] = {payload, cx};
    JSValue result =
        JS_Call(impl->context, entry->function, JS_UNDEFINED, 2, args);
    JS_FreeValue(impl->context, cx);
    JS_FreeValue(impl->context, payload);
    if (JS_IsException(result)) {
        Arena* arena = ArenaNew();
        log(ExceptionText(arena, impl->context));
        ArenaDelete(arena);
    } else {
        JS_FreeValue(impl->context, result);
        ShellError error = {};
        runtime->DrainJobs(kMaxJobBatch, &error);
        if (error.IsSet()) {
            log(error.message);
            ShellErrorClear(&error);
        }
    }
}

void ShellRuntime::DispatchClick(shell::CallbackId callback,
                                 const ClickEvent& event, Window* window,
                                 App* app) {
    JSValue payload = JS_NewObject(impl->context);
    JS_SetPropertyStr(impl->context, payload, "click_count",
                      JS_NewInt32(impl->context, event.clickCount));
    JSValue modifiers = JS_NewObject(impl->context);
    JS_SetPropertyStr(impl->context, modifiers, "shift",
                      JS_NewBool(impl->context, event.modifiers.shift));
    JS_SetPropertyStr(impl->context, modifiers, "control",
                      JS_NewBool(impl->context, event.modifiers.control));
    JS_SetPropertyStr(impl->context, modifiers, "alt",
                      JS_NewBool(impl->context, event.modifiers.alt));
    JS_SetPropertyStr(impl->context, payload, "modifiers", modifiers);
    Dispatch(this, callback, payload, window, app);
}

void ShellRuntime::DispatchMouseMove(shell::CallbackId callback,
                                     const MouseMoveEvent& event,
                                     Window* window, App* app) {
    JSValue payload = JS_NewObject(impl->context);
    JS_SetPropertyStr(impl->context, payload, "x",
                      JS_NewFloat64(impl->context, event.x));
    JS_SetPropertyStr(impl->context, payload, "y",
                      JS_NewFloat64(impl->context, event.y));
    JS_SetPropertyStr(impl->context, payload, "dragging",
                      JS_NewBool(impl->context, event.Dragging()));
    JSValue modifiers = JS_NewObject(impl->context);
    JS_SetPropertyStr(impl->context, modifiers, "shift",
                      JS_NewBool(impl->context, event.modifiers.shift));
    JS_SetPropertyStr(impl->context, modifiers, "control",
                      JS_NewBool(impl->context, event.modifiers.control));
    JS_SetPropertyStr(impl->context, modifiers, "alt",
                      JS_NewBool(impl->context, event.modifiers.alt));
    JS_SetPropertyStr(impl->context, payload, "modifiers", modifiers);
    Dispatch(this, callback, payload, window, app);
}

void ShellRuntime::DispatchChange(shell::CallbackId callback, bool value,
                                  Window* window, App* app) {
    Dispatch(this, callback, JS_NewBool(impl->context, value), window, app);
}

void ShellRuntime::DispatchIndex(shell::CallbackId callback, uint32_t value,
                                 Window* window, App* app) {
    Dispatch(this, callback, JS_NewUint32(impl->context, value), window, app);
}

void ShellRuntime::DispatchSignal(shell::CallbackId callback, Window* window,
                                  App* app) {
    Dispatch(this, callback, JS_NewObject(impl->context), window, app);
}

static void RetainedCallbackIds(const shell::RetainedEntry* entry,
                                shell::RetainedEvent event,
                                Vec<shell::CallbackId>* out) {
    if (!entry) return;
    for (int i = 0; i < entry->callbacks.len; i++) {
        if (entry->callbacks[i].event == event) {
            out->Append(entry->callbacks[i].callback);
        }
    }
}

static shell::RetainedEntry* EventRetained(ShellRuntimeImpl* impl,
                                            shell::EntityHandle handle) {
    return (handle >> 32) == 0
               ? impl->retained.FindLocal((uint32_t)handle)
               : impl->retained.Find(handle);
}

void ShellRuntime::DispatchInputEvent(shell::EntityHandle handle,
                                      const InputEvent& event, Window* window,
                                      App* app) {
    shell::RetainedEvent wanted = shell::RetainedEvent::InputChange;
    if (event.kind == InputEventKind::PressEnter) {
        wanted = shell::RetainedEvent::InputSubmit;
    } else if (event.kind == InputEventKind::Focus) {
        wanted = shell::RetainedEvent::InputFocus;
    } else if (event.kind == InputEventKind::Blur) {
        wanted = shell::RetainedEvent::InputBlur;
    }
    Vec<shell::CallbackId> callbacks;
    RetainedCallbackIds(EventRetained(impl, handle), wanted, &callbacks);
    for (int i = 0; i < callbacks.len; i++) {
        JSValue payload = JS_NewObject(impl->context);
        if (event.kind == InputEventKind::PressEnter) {
            JS_SetPropertyStr(impl->context, payload, "secondary",
                              JS_NewBool(impl->context, event.secondary));
            JS_SetPropertyStr(impl->context, payload, "shift",
                              JS_NewBool(impl->context, event.shift));
        }
        Dispatch(this, callbacks[i], payload, window, app);
    }
    callbacks.Reset();
}

void ShellRuntime::DispatchSliderEvent(shell::EntityHandle handle,
                                       const SliderEvent& event,
                                       Window* window, App* app) {
    shell::RetainedEvent wanted =
        event.kind == SliderEventKind::Release
            ? shell::RetainedEvent::SliderRelease
            : shell::RetainedEvent::SliderChange;
    Vec<shell::CallbackId> callbacks;
    RetainedCallbackIds(EventRetained(impl, handle), wanted, &callbacks);
    for (int i = 0; i < callbacks.len; i++) {
        JSValue payload = event.value.range
                              ? SliderValueJs(impl->context, event.value)
                              : JS_NewFloat64(impl->context, event.value.hi);
        Dispatch(this, callbacks[i], payload, window, app);
    }
    callbacks.Reset();
}

void ShellRuntime::DispatchOtpEvent(shell::EntityHandle handle,
                                    const OtpEvent& event, Window* window,
                                    App* app) {
    shell::RetainedEvent wanted = shell::RetainedEvent::OtpChange;
    if (event.kind == OtpEventKind::Complete) {
        wanted = shell::RetainedEvent::OtpComplete;
    } else if (event.kind == OtpEventKind::Focus) {
        wanted = shell::RetainedEvent::OtpFocus;
    } else if (event.kind == OtpEventKind::Blur) {
        wanted = shell::RetainedEvent::OtpBlur;
    }
    Vec<shell::CallbackId> callbacks;
    RetainedCallbackIds(EventRetained(impl, handle), wanted, &callbacks);
    for (int i = 0; i < callbacks.len; i++) {
        Dispatch(this, callbacks[i], JS_NewObject(impl->context), window, app);
    }
    callbacks.Reset();
}

void ShellRuntime::RenderVirtualItems(shell::CallbackId renderId,
                                      shell::CallbackId keyId, int first,
                                      int end, Ctx* cx, El** out) {
    if (!cx || !out || end <= first) return;
    for (int i = 0; i < end - first; i++) out[i] = nullptr;
    CallbackEntry* render = impl->callbacks.Get(renderId);
    CallbackEntry* key = impl->callbacks.Get(keyId);
    if (!render || !key || !cx->win || !cx->app) return;
    if (render->view.IsValid() && !EntityGet(cx->app, render->view)) return;

    double started = TimeNow();
    shell::SpecArena* outer = impl->scratch;
    shell::SpecArena* batch = new shell::SpecArena();
    impl->scratch = batch;
    Vec<shell::SpecId> roots;
    bool succeeded = true;
    {
        shell::CallScopeGuard scope = shell::ScopeEnter(
            cx->win, cx->app, ScopePhase::Layout, render->view,
            render->policy, this, render->application);
        shell::ScopeAdopt(render->registeredIn);
        BeginExecution(impl);
        JSValue payload = JS_NewObject(impl->context);
        JS_SetPropertyStr(impl->context, payload, "start",
                          JS_NewInt32(impl->context, first));
        JS_SetPropertyStr(impl->context, payload, "end",
                          JS_NewInt32(impl->context, end));
        JSValue context = ContextObject(impl, scope.Generation());
        JSValue args[2] = {payload, context};
        JSValue produced = JS_Call(impl->context, render->function,
                                   JS_UNDEFINED, 2, args);
        JS_FreeValue(impl->context, context);
        JS_FreeValue(impl->context, payload);
        if (JS_IsException(produced) || !JS_IsArray(produced)) {
            if (!JS_IsException(produced)) {
                JS_ThrowTypeError(impl->context,
                                  "a virtual-list item renderer must return an array of elements");
            }
            succeeded = false;
        } else {
            int64_t count = 0;
            if (JS_GetLength(impl->context, produced, &count) < 0 ||
                count != end - first) {
                if (!JS_HasException(impl->context)) {
                    JS_ThrowTypeError(impl->context,
                                      "a virtual-list item renderer must return one element per item");
                }
                succeeded = false;
            }
            for (int i = 0; succeeded && i < (int)count; i++) {
                JSValue item = JS_GetPropertyUint32(
                    impl->context, produced, (uint32_t)i);
                shell::SpecId root = 0;
                succeeded = !JS_IsException(item) &&
                            ElementId(impl->context, item, &root);
                JS_FreeValue(impl->context, item);
                if (succeeded) roots.Append(root);
            }
        }
        JS_FreeValue(impl->context, produced);

        Arena* keys = ArenaNew();
        Vec<Str> seen;
        for (int index = first; succeeded && index < end; index++) {
            JSValue value = JS_NewInt32(impl->context, index);
            JSValue result = JS_Call(impl->context, key->function,
                                     JS_UNDEFINED, 1, &value);
            JS_FreeValue(impl->context, value);
            Str text;
            succeeded = !JS_IsException(result) &&
                        JsString(impl->context, result, keys, &text);
            JS_FreeValue(impl->context, result);
            for (int i = 0; succeeded && i < seen.len; i++) {
                if (StrEq(seen[i], text)) {
                    JS_ThrowTypeError(impl->context,
                                      "virtual-list get_key returned a duplicate key");
                    succeeded = false;
                }
            }
            if (succeeded) seen.Append(text);
        }
        seen.Reset();
        ArenaDelete(keys);
    }
    impl->scratch = outer;
    if (!succeeded) {
        Arena* arena = ArenaNew();
        log(ExceptionText(arena, impl->context));
        ArenaDelete(arena);
    } else {
        ShellError error = {};
        for (int i = 0; i < roots.len; i++) {
            out[i] = ShellMaterializeSpec(cx, this, batch, roots[i], &error);
            if (error.IsSet()) {
                log(error.message);
                ShellErrorClear(&error);
            }
        }
    }
    roots.Reset();
    delete batch;
    double elapsed = TimeNow() - started;
    shell::MetricsAdd(&impl->metrics, shell::MetricsTimerKind::VirtualItems,
                      elapsed <= 0 ? 0 : (uint64_t)(elapsed * 1e9));
}

ViewType* ViewTypeRetain(ViewType* type) {
    if (type) type->refs++;
    return type;
}

void ViewTypeRelease(ViewType* type) {
    if (!type || --type->refs != 0) return;
    JS_FreeValue(ShellRuntimeAccess::Impl(type->runtime)->context, type->value);
    type->runtime->Release();
    delete type;
}

ViewObject* ViewObjectRetain(ViewObject* object) {
    if (object) object->refs++;
    return object;
}

void ViewObjectRelease(ViewObject* object) {
    if (!object || --object->refs != 0) return;
    JS_FreeValue(ShellRuntimeAccess::Impl(object->runtime)->context,
                 object->value);
    object->runtime->Release();
    delete object;
}

} // namespace gpui

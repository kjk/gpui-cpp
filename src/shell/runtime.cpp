#include "shell/runtime.h"

#include "quickjs/quickjs.h"
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

struct CallbackEntry {
    shell::CallbackId id = 0;
    uint64_t generation = 0;
    JSValue function = JS_UNDEFINED;
    EntityId view = {};
    Policy* policy = nullptr;
    uint64_t registeredIn = 0;
    bool committed = false;
};

struct CallbackArena {
    Vec<CallbackEntry*> entries;
    uint64_t nextGeneration = 0;
    shell::CallbackId nextCallback = 0;
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
                           uint64_t registeredIn) {
        if (!building || nextCallback == UINT64_MAX) return UINT64_MAX;
        CallbackEntry* entry = new CallbackEntry();
        entry->id = nextCallback++;
        entry->generation = buildingGeneration;
        entry->function = JS_DupValue(ctx, function);
        entry->view = view;
        entry->policy = PolicyRetain(policy);
        entry->registeredIn = registeredIn;
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
    shell::Metrics metrics;
    Vec<AppModule*> modules;
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
            shell::ScopeCurrentPolicy(), shell::ScopeCurrentGeneration());
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
    NotifyEntity(host.GetApp(), view, host.GetWindow());
    return JS_UNDEFINED;
}

static void SetGlobalFunction(JSContext* ctx, JSValueConst global,
                              const char* name, JSCFunction* function,
                              int length) {
    JS_SetPropertyStr(ctx, global, name,
                      JS_NewCFunction(ctx, function, name, length));
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
  });
  const component = (kind, text, handle, index) =>
    element(__component(kind, text, handle, index));
  const named = (kind) => ({ new: (id) => component(kind, String(id)) });
  const plain = (kind) => ({ new: () => component(kind) });
  const retained = (kind) => ({ new: (state) => component(kind, undefined, state?.__handle) });
  const unsupportedState = (name) => ({
    new: () => { throw new Error(name + " retained state is not installed yet"); },
  });
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
    v_virtual_list: () => { throw new Error("v_virtual_list is not installed yet"); },
    h_virtual_list: () => { throw new Error("h_virtual_list is not installed yet"); },
    VirtualListScrollHandle: unsupportedState("VirtualListScrollHandle"),
    InputState: unsupportedState("InputState"), Input: retained("Input"),
    NumberInput: retained("NumberInput"),
    TextareaState: unsupportedState("TextareaState"), Textarea: retained("Textarea"),
    SliderState: unsupportedState("SliderState"), Slider: retained("Slider"),
    SliderTrack: retained("SliderTrack"), SliderIndicator: retained("SliderIndicator"),
    SliderThumb: retained("SliderThumb"),
    OtpState: unsupportedState("OtpState"), OtpInput: retained("OtpInput"),
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
        if (impl->context) impl->callbacks.Clear(impl->context);
        delete impl->scratch;
        for (int i = 0; i < impl->modules.len; i++) {
            StrFree(impl->modules[i]->root);
            delete impl->modules[i];
        }
        impl->modules.Reset();
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
                                      Policy* policy, ShellError* error) {
    ShellErrorClear(error);
    if (!type || type->runtime != this || !window || !app) {
        SetError(error, StrL("instantiate needs a view type from this runtime and a live Window/App"));
        return nullptr;
    }
    shell::CallScopeGuard scope =
        shell::ScopeEnter(window, app, ScopePhase::Event, {}, policy, this,
                          type->application);
    BeginExecution(impl);
    JSValue object =
        JS_CallConstructor(impl->context, type->value, 0, nullptr);
    if (JS_IsException(object)) {
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

int ShellRuntime::LiveCallbacks() const { return impl->callbacks.Live(); }

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
        window, app, ScopePhase::Event, entry->view, entry->policy, runtime);
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

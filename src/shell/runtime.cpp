#include "shell/runtime.h"

#include "quickjs/quickjs.h"
#include "shell/fetch.h"
#include "shell/filesystem.h"
#include "shell/materialize.h"
#include "shell/process.h"
#include "shell/retained.h"
#include "shell/scope.h"
#include "shell/standard.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace gpui {

constexpr uint64_t kMaxModuleBytes = 8ull * 1024ull * 1024ull;
constexpr int kMaxJobBatch = 1024;
constexpr int kMaxShellTasks = 1024;
static bool gShellDevelopmentMode = false;
static ShellExitHandler gShellExitHandler = nullptr;

void ShellSetDevelopmentMode(bool enabled) {
    gShellDevelopmentMode = enabled;
}

bool ShellDevelopmentMode() {
    return gShellDevelopmentMode;
}

void ShellOnExitRequest(ShellExitHandler handler) {
    gShellExitHandler = handler;
}

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

enum class ShellTaskKind : uint8_t {
    Spawn,
    Sleep,
    TimerOnce,
    TimerEvery,
    Process,
    Filesystem,
    Fetch,
};

struct ProcessJob;
struct FsJob;
struct ShellFetchJob;

struct ShellTask {
    uint32_t id = 0;
    ShellTaskKind kind = ShellTaskKind::Spawn;
    JSValue callback = JS_UNDEFINED;
    JSValue reject = JS_UNDEFINED;
    EntityId owner = {};
    Policy* policy = nullptr;
    AppModule* application = nullptr;
    App* app = nullptr;
    Window* window = nullptr;
    int timer = 0;
    ProcessJob* processJob = nullptr;
    FsJob* fsJob = nullptr;
};

struct ShellTaskDriver {
    ShellRuntime* runtime = nullptr;

    static void OnTimer(ShellTaskDriver* self, Ctx* cx, const TickEvent*,
                        intptr_t id) {
        if (self && self->runtime) {
            self->runtime->ResumeTask((uint32_t)id, cx);
        }
    }
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
    Vec<ShellTask*> tasks;
    Entity<ShellTaskDriver> taskDriver = {};
    App* taskApp = nullptr;
    uint32_t nextTask = 1;
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

struct ProcessJob {
    ShellRuntimeControl* control = nullptr;
    uint32_t task = 0;
    shell::ProcessCancellation cancellation;
    Str command;
    Vec<Str> args;
    shell::ProcessOutput output;
    Str error;

    void Free() {
        for (int i = 0; i < args.len; i++) StrFree(args[i]);
        args.Reset();
        StrFree(command);
        output.Free();
        StrFree(error);
    }
};

struct FsJob {
    ShellRuntimeControl* control = nullptr;
    uint32_t task = 0;
    shell::FsOperation operation = shell::FsOperation::Read;
    CapabilityPath path;
    Str input;
    shell::FsResult result;
    Str error;
    bool text = false;
    bool withFileTypes = false;
    bool recursive = false;

    void Free() {
        path.Free();
        StrFree(input);
        result.Free();
        StrFree(error);
    }
};

struct ShellFetchJob {
    ShellRuntimeControl* control = nullptr;
    uint32_t task = 0;
    Str url;
    Capabilities capabilities;
    shell::FetchResult result;

    void Free() {
        StrFree(url);
        result.Free();
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

static bool TaskWindowLive(const ShellTask* task) {
    if (!task || !task->app || !task->window) return false;
    for (int i = 0; i < task->app->windows.len; i++) {
        if (task->app->windows[i] == task->window) return true;
    }
    return false;
}

static ShellTask* FindTask(ShellRuntimeImpl* impl, uint32_t id,
                           int* index = nullptr) {
    if (!impl || id == 0) return nullptr;
    for (int i = 0; i < impl->tasks.len; i++) {
        if (impl->tasks[i]->id != id) continue;
        if (index) *index = i;
        return impl->tasks[i];
    }
    return nullptr;
}

static void DestroyTask(ShellRuntimeImpl* impl, ShellTask* task,
                        bool cancelTimer) {
    if (!task) return;
    if (cancelTimer && task->timer && TaskWindowLive(task)) {
        WindowCancelTimer(task->window, task->timer);
    }
    if (task->processJob) task->processJob->cancellation.Cancel();
    if (impl && impl->context) JS_FreeValue(impl->context, task->callback);
    if (impl && impl->context) JS_FreeValue(impl->context, task->reject);
    PolicyRelease(task->policy);
    delete task;
}

static bool ForgetTask(ShellRuntimeImpl* impl, uint32_t id,
                       bool cancelTimer = true) {
    int at = -1;
    ShellTask* task = FindTask(impl, id, &at);
    if (!task) return false;
    for (int i = at + 1; i < impl->tasks.len; i++) {
        impl->tasks[i - 1] = impl->tasks[i];
    }
    impl->tasks.len--;
    DestroyTask(impl, task, cancelTimer);
    return true;
}

static Entity<ShellTaskDriver> TaskDriver(ShellRuntimeImpl* impl, App* app) {
    if (!impl || !app) return {};
    if (impl->taskDriver.IsValid() && impl->taskApp == app &&
        impl->taskDriver.Get(app)) {
        return impl->taskDriver;
    }
    if (impl->taskDriver.IsValid()) return {};
    impl->taskDriver = EntityNewState<ShellTaskDriver>(app);
    impl->taskApp = app;
    ShellTaskDriver* driver = impl->taskDriver.Get(app);
    if (driver) driver->runtime = impl->owner;
    return driver ? impl->taskDriver : Entity<ShellTaskDriver>{};
}

static uint32_t NewTask(ShellRuntimeImpl* impl, ShellTaskKind kind,
                        JSValueConst callback, App* app = nullptr,
                        Window* window = nullptr, bool ownerless = false,
                        JSValueConst reject = JS_UNDEFINED) {
    if (!impl || impl->tasks.len >= kMaxShellTasks || impl->nextTask == 0) {
        return 0;
    }
    ShellTask* task = new ShellTask();
    task->id = impl->nextTask++;
    task->kind = kind;
    task->callback = JS_DupValue(impl->context, callback);
    task->reject = JS_DupValue(impl->context, reject);
    task->owner = ownerless ? EntityId{} : shell::ScopeCurrentView();
    Policy* policy = shell::ScopeCurrentPolicy();
    task->policy = policy ? PolicyRetain(policy) : PolicyDefault();
    task->application = (AppModule*)shell::ScopeCurrentApplication();
    task->app = app;
    task->window = window;
    impl->tasks.Append(task);
    return task->id;
}

static void BeginExecution(ShellRuntimeImpl* impl);
static Str ExceptionText(Arena* arena, JSContext* ctx);

static void ProcessJobWork(ProcessJob* job) {
    shell::ProcessRunBounded(job->command, job->args.els, job->args.len,
                             &job->cancellation, &job->output, &job->error);
}

static void ProcessJobDone(ProcessJob* job) {
    ShellRuntime* runtime = job->control ? job->control->runtime : nullptr;
    ShellRuntimeImpl* impl = ShellRuntimeAccess::Impl(runtime);
    ShellTask* task = impl ? FindTask(impl, job->task) : nullptr;
    if (task && task->kind == ShellTaskKind::Process &&
        TaskWindowLive(task) &&
        (!task->owner.IsValid() || EntityGet(task->app, task->owner))) {
        Window* window = task->window;
        App* app = task->app;
        EntityId owner = task->owner;
        Policy* policy = PolicyRetain(task->policy);
        AppModule* application = task->application;
        JSValue settle = JS_DupValue(impl->context,
                                     job->error ? task->reject
                                                : task->callback);
        task->processJob = nullptr;
        ForgetTask(impl, task->id, false);

        shell::CallScopeGuard scope = shell::ScopeEnter(
            window, app, ScopePhase::Task, owner, policy, runtime,
            application);
        PolicyRelease(policy);
        BeginExecution(impl);
        JSValue value = JS_UNDEFINED;
        if (job->error) {
            value = JS_NewError(impl->context);
            JS_SetPropertyStr(
                impl->context, value, "message",
                JS_NewStringLen(impl->context, job->error.s,
                                (size_t)job->error.len));
        } else {
            value = JS_NewObject(impl->context);
            JS_SetPropertyStr(impl->context, value, "code",
                              JS_NewInt32(impl->context, job->output.code));
            JS_SetPropertyStr(
                impl->context, value, "stdout",
                JS_NewStringLen(impl->context,
                                job->output.out.s ? job->output.out.s : "",
                                (size_t)job->output.out.len));
            JS_SetPropertyStr(
                impl->context, value, "stderr",
                JS_NewStringLen(impl->context,
                                job->output.err.s ? job->output.err.s : "",
                                (size_t)job->output.err.len));
        }
        JSValue settled = JS_Call(impl->context, settle, JS_UNDEFINED, 1,
                                  &value);
        JS_FreeValue(impl->context, value);
        JS_FreeValue(impl->context, settle);
        if (JS_IsException(settled)) {
            Arena* arena = ArenaNew();
            log(ExceptionText(arena, impl->context));
            ArenaDelete(arena);
        } else {
            JS_FreeValue(impl->context, settled);
            ShellError error = {};
            runtime->DrainJobs(kMaxJobBatch, &error);
            if (error.IsSet()) {
                log(error.message);
                ShellErrorClear(&error);
            }
        }
    } else if (task) {
        task->processJob = nullptr;
        ForgetTask(impl, task->id, false);
    }
    job->Free();
    ControlRelease(job->control);
    delete job;
}

static void FsJobWork(FsJob* job) {
    shell::FsRun(job->operation, job->path.root, job->path.relative,
                 job->input, job->recursive, &job->result, &job->error);
}

static void FetchJobWork(ShellFetchJob* job) {
    shell::FetchGet(job->url, job->capabilities, &job->result);
}

static JSValue FsJobValue(JSContext* context, FsJob* job) {
    if (job->operation == shell::FsOperation::Read) {
        if (job->text) {
            return JS_NewStringLen(context,
                                   job->result.bytes.s
                                       ? job->result.bytes.s
                                       : "",
                                   (size_t)job->result.bytes.len);
        }
        return JS_NewUint8ArrayCopy(
            context, (const uint8_t*)(job->result.bytes.s
                                          ? job->result.bytes.s
                                          : ""),
            (size_t)job->result.bytes.len);
    }
    if (job->operation == shell::FsOperation::Exists) {
        return JS_NewBool(context, job->result.exists);
    }
    if (job->operation != shell::FsOperation::ReadDirectory) {
        return JS_UNDEFINED;
    }
    JSValue array = JS_NewArray(context);
    JSValue global = JS_GetGlobalObject(context);
    JSValue make = job->withFileTypes
                       ? JS_GetPropertyStr(context, global,
                                           "__shell_fs_dirent")
                       : JS_UNDEFINED;
    for (int i = 0; i < job->result.entries.len; i++) {
        const shell::FsEntry& entry = job->result.entries[i];
        JSValue name = JS_NewStringLen(context, entry.name.s,
                                       (size_t)entry.name.len);
        JSValue value = name;
        if (job->withFileTypes) {
            JSValue args[2] = {name,
                               JS_NewBool(context, entry.isDirectory)};
            value = JS_Call(context, make, JS_UNDEFINED, 2, args);
            JS_FreeValue(context, args[0]);
            JS_FreeValue(context, args[1]);
        }
        if (JS_IsException(value) ||
            JS_SetPropertyUint32(context, array, (uint32_t)i, value) < 0) {
            JS_FreeValue(context, array);
            array = JS_EXCEPTION;
            break;
        }
    }
    JS_FreeValue(context, make);
    JS_FreeValue(context, global);
    return array;
}

static void FsJobDone(FsJob* job) {
    ShellRuntime* runtime = job->control ? job->control->runtime : nullptr;
    ShellRuntimeImpl* impl = ShellRuntimeAccess::Impl(runtime);
    ShellTask* task = impl ? FindTask(impl, job->task) : nullptr;
    if (task && task->kind == ShellTaskKind::Filesystem &&
        TaskWindowLive(task) &&
        (!task->owner.IsValid() || EntityGet(task->app, task->owner))) {
        Window* window = task->window;
        App* app = task->app;
        EntityId owner = task->owner;
        Policy* policy = PolicyRetain(task->policy);
        AppModule* application = task->application;
        JSValue settle = JS_DupValue(impl->context,
                                     job->error ? task->reject
                                                : task->callback);
        task->fsJob = nullptr;
        ForgetTask(impl, task->id, false);

        shell::CallScopeGuard scope = shell::ScopeEnter(
            window, app, ScopePhase::Task, owner, policy, runtime,
            application);
        PolicyRelease(policy);
        BeginExecution(impl);
        JSValue value = JS_UNDEFINED;
        if (job->error) {
            value = JS_NewError(impl->context);
            JS_SetPropertyStr(
                impl->context, value, "message",
                JS_NewStringLen(impl->context, job->error.s,
                                (size_t)job->error.len));
        } else {
            value = FsJobValue(impl->context, job);
        }
        JSValue settled = JS_IsException(value)
                              ? JS_EXCEPTION
                              : JS_Call(impl->context, settle, JS_UNDEFINED,
                                        1, &value);
        JS_FreeValue(impl->context, value);
        JS_FreeValue(impl->context, settle);
        if (JS_IsException(settled)) {
            Arena* arena = ArenaNew();
            log(ExceptionText(arena, impl->context));
            ArenaDelete(arena);
        } else {
            JS_FreeValue(impl->context, settled);
            ShellError error = {};
            runtime->DrainJobs(kMaxJobBatch, &error);
            if (error.IsSet()) {
                log(error.message);
                ShellErrorClear(&error);
            }
        }
    } else if (task) {
        task->fsJob = nullptr;
        ForgetTask(impl, task->id, false);
    }
    job->Free();
    ControlRelease(job->control);
    delete job;
}

static JSValue FetchJobValue(JSContext* context, ShellFetchJob* job) {
    JSValue global = JS_GetGlobalObject(context);
    JSValue make = JS_GetPropertyStr(context, global,
                                     "__shell_fetch_response");
    JSValue args[3] = {
        JS_NewInt32(context, job->result.status),
        JS_NewStringLen(context,
                        job->result.url.s ? job->result.url.s : "",
                        (size_t)job->result.url.len),
        JS_NewStringLen(context,
                        job->result.body.s ? job->result.body.s : "",
                        (size_t)job->result.body.len),
    };
    JSValue value = JS_IsException(make)
                        ? JS_EXCEPTION
                        : JS_Call(context, make, JS_UNDEFINED, 3, args);
    for (int i = 0; i < 3; i++) JS_FreeValue(context, args[i]);
    JS_FreeValue(context, make);
    JS_FreeValue(context, global);
    return value;
}

static void FetchJobDone(ShellFetchJob* job) {
    ShellRuntime* runtime = job->control ? job->control->runtime : nullptr;
    ShellRuntimeImpl* impl = ShellRuntimeAccess::Impl(runtime);
    ShellTask* task = impl ? FindTask(impl, job->task) : nullptr;
    if (task && task->kind == ShellTaskKind::Fetch &&
        TaskWindowLive(task) &&
        (!task->owner.IsValid() || EntityGet(task->app, task->owner))) {
        Window* window = task->window;
        App* app = task->app;
        EntityId owner = task->owner;
        Policy* policy = PolicyRetain(task->policy);
        AppModule* application = task->application;
        bool failed = job->result.error.s != nullptr;
        JSValue settle = JS_DupValue(impl->context,
                                     failed ? task->reject
                                            : task->callback);
        ForgetTask(impl, task->id, false);

        shell::CallScopeGuard scope = shell::ScopeEnter(
            window, app, ScopePhase::Task, owner, policy, runtime,
            application);
        PolicyRelease(policy);
        BeginExecution(impl);
        JSValue value = JS_UNDEFINED;
        if (failed) {
            value = JS_NewError(impl->context);
            JS_SetPropertyStr(
                impl->context, value, "message",
                JS_NewStringLen(impl->context, job->result.error.s,
                                (size_t)job->result.error.len));
        } else {
            value = FetchJobValue(impl->context, job);
        }
        JSValue settled = JS_IsException(value)
                              ? JS_EXCEPTION
                              : JS_Call(impl->context, settle, JS_UNDEFINED,
                                        1, &value);
        JS_FreeValue(impl->context, value);
        JS_FreeValue(impl->context, settle);
        if (JS_IsException(settled)) {
            Arena* arena = ArenaNew();
            log(ExceptionText(arena, impl->context));
            ArenaDelete(arena);
        } else {
            JS_FreeValue(impl->context, settled);
            ShellError error = {};
            runtime->DrainJobs(kMaxJobBatch, &error);
            if (error.IsSet()) {
                log(error.message);
                ShellErrorClear(&error);
            }
        }
    } else if (task) {
        ForgetTask(impl, task->id, false);
    }
    job->Free();
    ControlRelease(job->control);
    delete job;
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
           strcmp(name, "gpui-shell") == 0 || strcmp(name, "gpui-fps") == 0 ||
           strcmp(name, "buffer") == 0 || strcmp(name, "console") == 0 ||
           strcmp(name, "crypto") == 0 ||
           strcmp(name, "fs/promises") == 0 || strcmp(name, "os") == 0 ||
           strcmp(name, "path") == 0 || strcmp(name, "process") == 0 ||
           strcmp(name, "url") == 0 || strcmp(name, "zlib") == 0;
}

static const char* const kGpuiExports[] = {
    "View", "div", "svg", "image", "PathBuilder", "Background",
    "with_cx"};
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
static const char* const kBufferExports[] = {"default", "Buffer"};
static const char* const kConsoleExports[] = {"default"};
static const char* const kCryptoExports[] = {
    "default", "createHash", "randomBytes", "randomUUID",
    "getRandomValues", "crypto", "webcrypto"};
static const char* const kFsExports[] = {"default", "readFile", "writeFile",
                                         "readdir", "exists", "unlink",
                                         "rmdir", "mkdir"};
static const char* const kOsExports[] = {"default", "platform", "arch", "EOL"};
static const char* const kPathExports[] = {
    "default", "sep", "delimiter", "basename", "dirname", "extname",
    "isAbsolute", "join", "normalize", "relative", "resolve", "parse",
    "format"};
static const char* const kProcessExports[] = {"default", "run", "nextTick",
                                              "exit", "platform", "arch"};
static const char* const kUrlExports[] = {"default", "URL", "URLSearchParams",
                                          "fileURLToPath", "pathToFileURL"};
static const char* const kZlibExports[] = {
    "default", "deflateSync", "inflateSync", "gzipSync", "gunzipSync"};

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
    } else if (strcmp(name, "buffer") == 0) {
        *values = kBufferExports;
        *count = (int)(sizeof(kBufferExports) / sizeof(kBufferExports[0]));
    } else if (strcmp(name, "console") == 0) {
        *values = kConsoleExports;
        *count = (int)(sizeof(kConsoleExports) / sizeof(kConsoleExports[0]));
    } else if (strcmp(name, "crypto") == 0) {
        *values = kCryptoExports;
        *count = (int)(sizeof(kCryptoExports) / sizeof(kCryptoExports[0]));
    } else if (strcmp(name, "fs/promises") == 0) {
        *values = kFsExports;
        *count = (int)(sizeof(kFsExports) / sizeof(kFsExports[0]));
    } else if (strcmp(name, "os") == 0) {
        *values = kOsExports;
        *count = (int)(sizeof(kOsExports) / sizeof(kOsExports[0]));
    } else if (strcmp(name, "path") == 0) {
        *values = kPathExports;
        *count = (int)(sizeof(kPathExports) / sizeof(kPathExports[0]));
    } else if (strcmp(name, "process") == 0) {
        *values = kProcessExports;
        *count = (int)(sizeof(kProcessExports) / sizeof(kProcessExports[0]));
    } else if (strcmp(name, "url") == 0) {
        *values = kUrlExports;
        *count = (int)(sizeof(kUrlExports) / sizeof(kUrlExports[0]));
    } else if (strcmp(name, "zlib") == 0) {
        *values = kZlibExports;
        *count = (int)(sizeof(kZlibExports) / sizeof(kZlibExports[0]));
    }
}

static const char* BuiltinObject(const char* name) {
    if (strcmp(name, "gpui") == 0 || strcmp(name, "gpui-base") == 0 ||
        strcmp(name, "gpui-shell") == 0 || strcmp(name, "gpui-fps") == 0)
        return "__gpui";
    if (strcmp(name, "buffer") == 0) return "__shell_buffer";
    if (strcmp(name, "console") == 0) return "console";
    if (strcmp(name, "crypto") == 0) return "__shell_crypto";
    if (strcmp(name, "fs/promises") == 0) return "__shell_fs";
    if (strcmp(name, "os") == 0) return "__shell_os";
    if (strcmp(name, "path") == 0) return "__shell_path";
    if (strcmp(name, "process") == 0) return "process";
    if (strcmp(name, "url") == 0) return "__shell_url";
    if (strcmp(name, "zlib") == 0) return "__shell_zlib";
    return "__gpui";
}

static int InitBuiltinModule(JSContext* ctx, JSModuleDef* module) {
    JSAtom atom = JS_GetModuleName(ctx, module);
    const char* name = JS_AtomToCString(ctx, atom);
    const char* const* exports = nullptr;
    int count = 0;
    ModuleExports(name ? name : "", &exports, &count);
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue api = JS_GetPropertyStr(ctx, global,
                                    BuiltinObject(name ? name : ""));
    int result = 0;
    for (int i = 0; i < count; i++) {
        JSValue value = strcmp(exports[i], "default") == 0
                            ? JS_DupValue(ctx, api)
                            : JS_GetPropertyStr(ctx, api, exports[i]);
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

static JSValue NativeNotifyCurrent(JSContext* ctx, JSValueConst, int,
                                   JSValueConst*) {
    uint64_t current = shell::ScopeCurrentGeneration();
    if (current == 0) {
        return JS_ThrowTypeError(ctx, "cx.notify() was called with no host call in progress");
    }
    JSValue generation = JS_NewInt64(ctx, (int64_t)current);
    JSValue result = NativeNotify(ctx, JS_UNDEFINED, 1, &generation);
    JS_FreeValue(ctx, generation);
    return result;
}

static bool TaskOwnerless(JSContext* ctx, int argc, JSValueConst* argv,
                          int at, bool* ownerless) {
    *ownerless = false;
    if (argc <= at || JS_IsUndefined(argv[at]) || JS_IsNull(argv[at])) {
        *ownerless = argc > at && JS_IsNull(argv[at]);
        return true;
    }
    if (!JS_IsObject(argv[at])) {
        JS_ThrowTypeError(ctx, "task options must be an object");
        return false;
    }
    JSValue owner = JS_GetPropertyStr(ctx, argv[at], "owner");
    if (JS_IsException(owner)) return false;
    if (JS_IsNull(owner)) {
        *ownerless = true;
    } else if (!JS_IsUndefined(owner) && !JS_IsObject(owner)) {
        JS_FreeValue(ctx, owner);
        JS_ThrowTypeError(ctx, "opts.owner must be the current view or null");
        return false;
    }
    JS_FreeValue(ctx, owner);
    return true;
}

static bool TaskDelay(JSContext* ctx, JSValueConst value, int* ms) {
    double number = 0;
    if (JS_ToFloat64(ctx, &number, value) < 0 || !isfinite(number) ||
        number < 0 || number > 2147483647.0) {
        JS_ThrowTypeError(ctx, "timer expects a finite non-negative number of milliseconds");
        return false;
    }
    *ms = number < 1 ? 1 : (int)number;
    return true;
}

static JSValue NativeTaskNew(JSContext* ctx, JSValueConst, int argc,
                             JSValueConst* argv) {
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    bool ownerless = false;
    if (!impl || !TaskOwnerless(ctx, argc, argv, 0, &ownerless)) {
        return JS_EXCEPTION;
    }
    uint32_t id = NewTask(impl, ShellTaskKind::Spawn, JS_UNDEFINED, nullptr,
                          nullptr, ownerless);
    if (!id) return JS_ThrowRangeError(ctx, "the runtime reached its 1024 outstanding task limit");
    return JS_NewUint32(ctx, id);
}

static JSValue NativeTaskFinish(JSContext* ctx, JSValueConst, int argc,
                                JSValueConst* argv) {
    uint32_t id = 0;
    if (argc < 1 || JS_ToUint32(ctx, &id, argv[0]) < 0) return JS_EXCEPTION;
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    return JS_NewBool(ctx, ForgetTask(impl, id));
}

static JSValue NativeTaskReject(JSContext* ctx, JSValueConst, int argc,
                                JSValueConst* argv) {
    uint32_t id = 0;
    if (argc < 1 || JS_ToUint32(ctx, &id, argv[0]) < 0) return JS_EXCEPTION;
    if (argc > 1) {
        size_t n = 0;
        const char* message = JS_ToCStringLen(ctx, &n, argv[1]);
        if (message) {
            log(fmt("unhandled rejection in cx.spawn: %s",
                    Str(message, (int)n)));
            JS_FreeCString(ctx, message);
        }
    }
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    ForgetTask(impl, id);
    return JS_UNDEFINED;
}

static JSValue NativeTaskCancel(JSContext* ctx, JSValueConst, int argc,
                                JSValueConst* argv) {
    uint32_t id = 0;
    if (argc < 1 || JS_ToUint32(ctx, &id, argv[0]) < 0) return JS_EXCEPTION;
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    return JS_NewBool(ctx, ForgetTask(impl, id));
}

static JSValue NativeTaskIsDone(JSContext* ctx, JSValueConst, int argc,
                                JSValueConst* argv) {
    uint32_t id = 0;
    if (argc < 1 || JS_ToUint32(ctx, &id, argv[0]) < 0) return JS_EXCEPTION;
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    return JS_NewBool(ctx, FindTask(impl, id) == nullptr);
}

static JSValue NativeSleep(JSContext* ctx, JSValueConst, int argc,
                           JSValueConst* argv) {
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    int ms = 1;
    if (!impl || argc < 1 || !TaskDelay(ctx, argv[0], &ms)) return JS_EXCEPTION;
    shell::ScopeHostContext host = shell::ScopeCurrentHost();
    if (!host.IsSet()) {
        return JS_ThrowTypeError(ctx, "cx.sleep(ms) was called with no host call in progress");
    }
    Entity<ShellTaskDriver> driver = TaskDriver(impl, host.GetApp());
    if (!driver.IsValid()) return JS_ThrowInternalError(ctx, "creating the shell task driver failed");
    JSValue resolving[2] = {JS_UNDEFINED, JS_UNDEFINED};
    JSValue promise = JS_NewPromiseCapability(ctx, resolving);
    if (JS_IsException(promise)) return promise;
    uint32_t id = NewTask(impl, ShellTaskKind::Sleep, resolving[0],
                          host.GetApp(), host.GetWindow());
    JS_FreeValue(ctx, resolving[0]);
    JS_FreeValue(ctx, resolving[1]);
    if (!id) {
        JS_FreeValue(ctx, promise);
        return JS_ThrowRangeError(ctx, "the runtime reached its 1024 outstanding task limit");
    }
    ShellTask* task = FindTask(impl, id);
    task->timer = WindowSetTimeout(
        host.GetWindow(), ms,
        ListenTo(driver, &ShellTaskDriver::OnTimer, (intptr_t)id));
    if (!task->timer) {
        ForgetTask(impl, id, false);
        JS_FreeValue(ctx, promise);
        return JS_ThrowInternalError(ctx, "arming cx.sleep(ms) failed");
    }
    return promise;
}

static JSValue NativeTimer(JSContext* ctx, JSValueConst, int argc,
                           JSValueConst* argv, int magic) {
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    int ms = 1;
    bool ownerless = false;
    if (!impl || argc < 2 || !TaskDelay(ctx, argv[0], &ms) ||
        !JS_IsFunction(ctx, argv[1]) ||
        !TaskOwnerless(ctx, argc, argv, 2, &ownerless)) {
        if (!JS_HasException(ctx)) JS_ThrowTypeError(ctx, "timer needs a delay and callback function");
        return JS_EXCEPTION;
    }
    shell::ScopeHostContext host = shell::ScopeCurrentHost();
    if (!host.IsSet()) {
        return JS_ThrowTypeError(ctx, "cx.timer needs a live host call");
    }
    Entity<ShellTaskDriver> driver = TaskDriver(impl, host.GetApp());
    if (!driver.IsValid()) return JS_ThrowInternalError(ctx, "creating the shell task driver failed");
    ShellTaskKind kind = magic ? ShellTaskKind::TimerEvery
                               : ShellTaskKind::TimerOnce;
    uint32_t id = NewTask(impl, kind, argv[1], host.GetApp(),
                          host.GetWindow(), ownerless);
    if (!id) return JS_ThrowRangeError(ctx, "the runtime reached its 1024 outstanding task limit");
    ShellTask* task = FindTask(impl, id);
    Listener listener = ListenTo(driver, &ShellTaskDriver::OnTimer,
                                 (intptr_t)id);
    task->timer = magic ? WindowSetInterval(host.GetWindow(), ms, listener)
                        : WindowSetTimeout(host.GetWindow(), ms, listener);
    if (!task->timer) {
        ForgetTask(impl, id, false);
        return JS_ThrowInternalError(ctx, "arming the shell timer failed");
    }
    return JS_NewUint32(ctx, id);
}

static Policy* CurrentPolicy(bool* release) {
    Policy* policy = shell::ScopeCurrentPolicy();
    *release = policy == nullptr;
    return policy ? policy : PolicyDefault();
}

static shell::Storage* AllowedStorage(JSContext* ctx, bool session,
                                      Policy** held) {
    bool release = false;
    Policy* policy = CurrentPolicy(&release);
    if (!session && !PolicyCapabilities(policy).HasStorage()) {
        if (release) PolicyRelease(policy);
        JS_ThrowTypeError(ctx, "storage is not granted; set capabilities.storage to true");
        return nullptr;
    }
    *held = release ? policy : nullptr;
    shell::Storage* storage = PolicyStorage(policy, session);
    if (!storage || (!session && !storage->HasPath())) {
        if (release) PolicyRelease(policy);
        *held = nullptr;
        JS_ThrowTypeError(ctx, "localStorage has no backing file; call ShellSetStoragePath before loading the application");
        return nullptr;
    }
    return storage;
}

static JSValue NativeStorageGet(JSContext* ctx, JSValueConst, int argc,
                                JSValueConst* argv) {
    Policy* held = nullptr;
    bool session = argc > 1 && JS_ToBool(ctx, argv[1]) != 0;
    shell::Storage* storage = AllowedStorage(ctx, session, &held);
    if (!storage) return JS_EXCEPTION;
    Arena* arena = ArenaNew();
    Str key;
    bool ok = argc >= 1 && JsString(ctx, argv[0], arena, &key);
    Str value = ok ? storage->Get(key) : Str{};
    JSValue result = !ok ? JS_EXCEPTION
                         : value ? JS_NewStringLen(ctx, value.s, (size_t)value.len)
                                 : JS_NULL;
    ArenaDelete(arena);
    PolicyRelease(held);
    return result;
}

static JSValue NativeStorageSet(JSContext* ctx, JSValueConst, int argc,
                                JSValueConst* argv) {
    Policy* held = nullptr;
    bool session = argc > 2 && JS_ToBool(ctx, argv[2]) != 0;
    shell::Storage* storage = AllowedStorage(ctx, session, &held);
    if (!storage) return JS_EXCEPTION;
    Arena* arena = ArenaNew();
    Str key, value, error;
    bool ok = argc >= 2 && JsString(ctx, argv[0], arena, &key) &&
              JsString(ctx, argv[1], arena, &value) &&
              storage->Set(key, value, &error) && storage->Flush(&error);
    JSValue result = ok ? JS_UNDEFINED
                        : error ? JS_ThrowInternalError(ctx, "%.*s", error.len, error.s)
                                : JS_EXCEPTION;
    StrFree(error);
    ArenaDelete(arena);
    PolicyRelease(held);
    return result;
}

static JSValue NativeStorageRemove(JSContext* ctx, JSValueConst, int argc,
                                   JSValueConst* argv) {
    Policy* held = nullptr;
    bool session = argc > 1 && JS_ToBool(ctx, argv[1]) != 0;
    shell::Storage* storage = AllowedStorage(ctx, session, &held);
    if (!storage) return JS_EXCEPTION;
    Arena* arena = ArenaNew();
    Str key, error;
    bool ok = argc >= 1 && JsString(ctx, argv[0], arena, &key) &&
              storage->Remove(key, &error) && storage->Flush(&error);
    JSValue result = ok ? JS_UNDEFINED
                        : error ? JS_ThrowInternalError(ctx, "%.*s", error.len, error.s)
                                : JS_EXCEPTION;
    StrFree(error);
    ArenaDelete(arena);
    PolicyRelease(held);
    return result;
}

static JSValue NativeStorageClear(JSContext* ctx, JSValueConst, int,
                                  JSValueConst* argv) {
    Policy* held = nullptr;
    bool session = JS_ToBool(ctx, argv[0]) != 0;
    shell::Storage* storage = AllowedStorage(ctx, session, &held);
    if (!storage) return JS_EXCEPTION;
    Str error;
    bool ok = storage->Clear(&error) && storage->Flush(&error);
    JSValue result = ok ? JS_UNDEFINED
                        : JS_ThrowInternalError(ctx, "%.*s", error.len, error.s);
    StrFree(error);
    PolicyRelease(held);
    return result;
}

static JSValue NativeStorageLength(JSContext* ctx, JSValueConst, int,
                                   JSValueConst* argv) {
    Policy* held = nullptr;
    bool session = JS_ToBool(ctx, argv[0]) != 0;
    shell::Storage* storage = AllowedStorage(ctx, session, &held);
    if (!storage) return JS_EXCEPTION;
    JSValue result = JS_NewInt32(ctx, storage->Len());
    PolicyRelease(held);
    return result;
}

static JSValue NativeStorageKey(JSContext* ctx, JSValueConst, int argc,
                                JSValueConst* argv) {
    Policy* held = nullptr;
    bool session = argc > 1 && JS_ToBool(ctx, argv[1]) != 0;
    shell::Storage* storage = AllowedStorage(ctx, session, &held);
    if (!storage) return JS_EXCEPTION;
    int32_t index = -1;
    bool ok = argc >= 1 && JS_ToInt32(ctx, &index, argv[0]) == 0;
    Str key = ok ? storage->Key(index) : Str{};
    JSValue result = !ok ? JS_EXCEPTION
                         : key ? JS_NewStringLen(ctx, key.s, (size_t)key.len)
                               : JS_NULL;
    PolicyRelease(held);
    return result;
}

static JSValue ResolvedPromise(JSContext* ctx) {
    JSValue resolving[2] = {JS_UNDEFINED, JS_UNDEFINED};
    JSValue promise = JS_NewPromiseCapability(ctx, resolving);
    if (!JS_IsException(promise)) {
        JSValue settled = JS_Call(ctx, resolving[0], JS_UNDEFINED, 0, nullptr);
        JS_FreeValue(ctx, settled);
    }
    JS_FreeValue(ctx, resolving[0]);
    JS_FreeValue(ctx, resolving[1]);
    return promise;
}

static JSValue NativeStorageFlush(JSContext* ctx, JSValueConst, int,
                                  JSValueConst* argv) {
    Policy* held = nullptr;
    bool session = JS_ToBool(ctx, argv[0]) != 0;
    shell::Storage* storage = AllowedStorage(ctx, session, &held);
    if (!storage) return JS_EXCEPTION;
    Str error;
    bool ok = storage->Flush(&error);
    PolicyRelease(held);
    if (!ok) {
        JSValue result = JS_ThrowInternalError(ctx, "%.*s", error.len, error.s);
        StrFree(error);
        return result;
    }
    return ResolvedPromise(ctx);
}

static JSValue NativeClipboard(JSContext* ctx, JSValueConst, int argc,
                               JSValueConst* argv, int magic) {
    bool release = false;
    Policy* policy = CurrentPolicy(&release);
    const Capabilities& capabilities = PolicyCapabilities(policy);
    bool allowed = magic == 0 ? capabilities.IsClipboardReadable()
                              : capabilities.IsClipboardWritable();
    if (!allowed) {
        if (release) PolicyRelease(policy);
        return JS_ThrowTypeError(
            ctx, magic == 0
                     ? "reading the clipboard is not granted; declare capabilities.clipboard.read"
                     : "writing the clipboard is not granted; declare capabilities.clipboard.write");
    }
    shell::ScopeHostContext host = shell::ScopeCurrentHost();
    if (!host.IsSet()) {
        if (release) PolicyRelease(policy);
        return JS_ThrowTypeError(ctx, "clipboard access needs a live host call");
    }
    JSValue result = JS_UNDEFINED;
    if (magic == 0) {
        Arena* arena = ArenaNew();
        Str value = ClipboardGetText(arena, host.GetWindow());
        result = value ? JS_NewStringLen(ctx, value.s, (size_t)value.len)
                       : JS_NULL;
        ArenaDelete(arena);
    } else {
        Arena* arena = ArenaNew();
        Str value;
        bool ok = argc >= 1 && JsString(ctx, argv[0], arena, &value);
        if (ok) ClipboardSetText(host.GetWindow(), value);
        ArenaDelete(arena);
        if (!ok) result = JS_EXCEPTION;
    }
    if (release) PolicyRelease(policy);
    return result;
}

static JSValue NativeConsole(JSContext* ctx, JSValueConst, int argc,
                             JSValueConst* argv, int magic) {
    StrBuilder out;
    static const char* levels[] = {"log", "debug", "info", "warn", "error"};
    out.Append(fmt("[script %s]", Str(levels[magic >= 0 && magic < 5 ? magic : 0])));
    for (int i = 0; i < argc; i++) {
        size_t n = 0;
        const char* value = JS_ToCStringLen(ctx, &n, argv[i]);
        out.AppendChar(' ');
        if (value) {
            out.Append(Str(value, (int)n));
            JS_FreeCString(ctx, value);
        } else {
            out.Append(StrL("<value>"));
            JSValue exception = JS_GetException(ctx);
            JS_FreeValue(ctx, exception);
        }
    }
    Str message = out.TakeStr();
    log(message);
    StrFree(message);
    return JS_UNDEFINED;
}

static bool ObjectOnlyOption(JSContext* ctx, JSValueConst object,
                             const char* allowed, const char* what) {
    JSPropertyEnum* properties = nullptr;
    uint32_t count = 0;
    if (JS_GetOwnPropertyNames(ctx, &properties, &count, object,
                               JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) < 0) {
        return false;
    }
    bool ok = true;
    for (uint32_t i = 0; i < count; i++) {
        const char* name = JS_AtomToCString(ctx, properties[i].atom);
        bool matches = name && strcmp(name, allowed) == 0;
        if (!matches) {
            JS_ThrowTypeError(ctx, "unknown option `%s` for %s; expected %s",
                              name ? name : "<symbol>", what, allowed);
            ok = false;
        }
        if (name) JS_FreeCString(ctx, name);
        if (!ok) break;
    }
    JS_FreePropertyEnum(ctx, properties, count);
    return ok;
}

static bool FsReadTextOption(JSContext* ctx, JSValueConst value, Arena* arena,
                             bool* text) {
    *text = false;
    if (JS_IsUndefined(value) || JS_IsNull(value)) return true;
    JSValue encoding = JS_UNDEFINED;
    if (JS_IsString(value)) {
        encoding = JS_DupValue(ctx, value);
    } else if (JS_IsObject(value)) {
        if (!ObjectOnlyOption(ctx, value, "encoding", "fs.readFile options"))
            return false;
        encoding = JS_GetPropertyStr(ctx, value, "encoding");
    } else {
        JS_ThrowTypeError(ctx, "fs.readFile encoding must be \"utf8\" or { encoding: \"utf8\" }");
        return false;
    }
    Str name;
    bool ok = !JS_IsException(encoding) && JsString(ctx, encoding, arena, &name);
    JS_FreeValue(ctx, encoding);
    if (!ok) return false;
    if (!StrEqI(name, StrL("utf8")) && !StrEqI(name, StrL("utf-8"))) {
        JS_ThrowTypeError(ctx, "fs.readFile only supports UTF-8 text decoding");
        return false;
    }
    *text = true;
    return true;
}

static bool FsBoolOption(JSContext* ctx, JSValueConst value,
                         const char* key, const char* what, bool* result) {
    *result = false;
    if (JS_IsUndefined(value) || JS_IsNull(value)) return true;
    if (!JS_IsObject(value)) {
        JS_ThrowTypeError(ctx, "%s expects an options object", what);
        return false;
    }
    if (!ObjectOnlyOption(ctx, value, key, what)) return false;
    JSValue option = JS_GetPropertyStr(ctx, value, key);
    if (JS_IsException(option)) return false;
    if (!JS_IsUndefined(option) && !JS_IsBool(option)) {
        JS_FreeValue(ctx, option);
        JS_ThrowTypeError(ctx, "%s.%s must be boolean", what, key);
        return false;
    }
    if (!JS_IsUndefined(option)) *result = JS_ToBool(ctx, option) != 0;
    JS_FreeValue(ctx, option);
    return true;
}

static JSValue NativeFetch(JSContext* ctx, JSValueConst, int argc,
                           JSValueConst* argv) {
    Arena* arena = ArenaNew();
    Str url;
    bool converted = argc >= 1 && JsString(ctx, argv[0], arena, &url);
    bool release = false;
    Policy* policy = CurrentPolicy(&release);
    Str authorizationError;
    bool allowed = converted && shell::FetchAuthorizeGet(
                                  url, PolicyCapabilities(policy),
                                  &authorizationError);
    if (!allowed) {
        JSValue result = converted
                             ? JS_ThrowTypeError(
                                   ctx, "%.*s", authorizationError.len,
                                   authorizationError.s
                                       ? authorizationError.s
                                       : "fetch URL is not granted")
                             : JS_EXCEPTION;
        StrFree(authorizationError);
        if (release) PolicyRelease(policy);
        ArenaDelete(arena);
        return result;
    }
    shell::ScopeHostContext host = shell::ScopeCurrentHost();
    if (!host.IsSet()) {
        if (release) PolicyRelease(policy);
        ArenaDelete(arena);
        return JS_ThrowTypeError(ctx, "fetch() needs a live host task");
    }

    ShellFetchJob* job = new ShellFetchJob();
    job->url = StrDup(url);
    job->capabilities = PolicyCapabilities(policy);
    if (release) PolicyRelease(policy);
    ArenaDelete(arena);
    if (!job->url.s) {
        job->Free();
        delete job;
        return JS_ThrowOutOfMemory(ctx);
    }

    JSValue resolving[2] = {JS_UNDEFINED, JS_UNDEFINED};
    JSValue promise = JS_NewPromiseCapability(ctx, resolving);
    if (JS_IsException(promise)) {
        job->Free();
        delete job;
        return JS_EXCEPTION;
    }
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    uint32_t task = NewTask(impl, ShellTaskKind::Fetch, resolving[0],
                            host.GetApp(), host.GetWindow(), false,
                            resolving[1]);
    JS_FreeValue(ctx, resolving[0]);
    JS_FreeValue(ctx, resolving[1]);
    if (!task) {
        JS_FreeValue(ctx, promise);
        job->Free();
        delete job;
        return JS_ThrowRangeError(
            ctx, "the runtime reached its outstanding host task limit");
    }
    job->control = ShellRuntimeAccess::Control(impl->owner);
    ControlRetain(job->control);
    job->task = task;
    if (!ExecSpawn(MkFunc0(FetchJobWork, job),
                   MkFunc0(FetchJobDone, job))) {
        ForgetTask(impl, task, false);
        ControlRelease(job->control);
        JS_FreeValue(ctx, promise);
        job->Free();
        delete job;
        return JS_ThrowInternalError(ctx,
                                     "fetch could not start background work");
    }
    return promise;
}

static JSValue NativeFs(JSContext* ctx, JSValueConst, int argc,
                        JSValueConst* argv, int magic) {
    if (magic < 0 || magic > 6) return JS_ThrowInternalError(ctx, "invalid filesystem operation");
    shell::FsOperation operation = (shell::FsOperation)magic;
    CapabilityAccess access = operation == shell::FsOperation::Read ||
                                      operation == shell::FsOperation::ReadDirectory ||
                                      operation == shell::FsOperation::Exists
                                  ? CapabilityAccess::Read
                                  : CapabilityAccess::Write;
    Arena* arena = ArenaNew();
    Str requested;
    bool ok = argc >= 1 && JsString(ctx, argv[0], arena, &requested);
    if (!ok) {
        ArenaDelete(arena);
        return JS_EXCEPTION;
    }
    bool release = false;
    Policy* policy = CurrentPolicy(&release);
    CapabilityPath path;
    CapabilityError capabilityError;
    ok = PolicyCapabilities(policy).ResolvePath(requested, access, &path,
                                                 &capabilityError);
    if (release) PolicyRelease(policy);
    if (!ok) {
        Str message = CapabilityErrorMessage(arena, capabilityError);
        JSValue result = JS_ThrowTypeError(ctx, "%.*s", message.len, message.s);
        CapabilityErrorFree(&capabilityError);
        ArenaDelete(arena);
        return result;
    }

    FsJob* job = new FsJob();
    job->operation = operation;
    job->path = path;
    if (operation == shell::FsOperation::Read) {
        ok = FsReadTextOption(ctx, argc > 1 ? argv[1] : JS_UNDEFINED, arena,
                              &job->text);
    } else if (operation == shell::FsOperation::ReadDirectory) {
        ok = FsBoolOption(ctx, argc > 1 ? argv[1] : JS_UNDEFINED,
                          "withFileTypes", "fs.readdir(path, options)",
                          &job->withFileTypes);
    } else if (operation == shell::FsOperation::MakeDirectory) {
        ok = FsBoolOption(ctx, argc > 1 ? argv[1] : JS_UNDEFINED,
                          "recursive", "fs.mkdir(path, options)",
                          &job->recursive);
    } else if (operation == shell::FsOperation::Write) {
        if (argc < 2) {
            JS_ThrowTypeError(ctx, "fs.writeFile(path, contents) expects a string or Uint8Array");
            ok = false;
        } else if (JS_IsString(argv[1])) {
            Str input;
            ok = JsString(ctx, argv[1], arena, &input);
            if (ok) job->input = StrDup(input);
        } else if (JS_GetTypedArrayType(argv[1]) == JS_TYPED_ARRAY_UINT8) {
            size_t count = 0;
            uint8_t* bytes = JS_GetUint8Array(ctx, &count, argv[1]);
            ok = bytes != nullptr || count == 0;
            if (ok && count <= (size_t)INT_MAX) {
                job->input = StrDup(Str((const char*)bytes, (int)count));
            } else if (ok) {
                JS_ThrowRangeError(ctx, "fs.writeFile contents are too large");
                ok = false;
            }
        } else {
            JS_ThrowTypeError(ctx, "fs.writeFile(path, contents) expects a string or Uint8Array");
            ok = false;
        }
        if (ok && job->input.len > shell::kFsMaxWriteBytes) {
            JS_ThrowRangeError(ctx, "fs.writeFile contents exceed the 8388608-byte write limit");
            ok = false;
        }
        if (ok && !job->input.s && job->input.len != 0) {
            JS_ThrowOutOfMemory(ctx);
            ok = false;
        }
    }
    ArenaDelete(arena);
    if (!ok) {
        job->Free();
        delete job;
        return JS_EXCEPTION;
    }

    shell::ScopeHostContext host = shell::ScopeCurrentHost();
    if (!host.IsSet()) {
        job->Free();
        delete job;
        return JS_ThrowTypeError(ctx, "filesystem access needs a live host task");
    }
    JSValue resolving[2] = {JS_UNDEFINED, JS_UNDEFINED};
    JSValue promise = JS_NewPromiseCapability(ctx, resolving);
    if (JS_IsException(promise)) {
        job->Free();
        delete job;
        return JS_EXCEPTION;
    }
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    uint32_t task = NewTask(impl, ShellTaskKind::Filesystem, resolving[0],
                            host.GetApp(), host.GetWindow(), false,
                            resolving[1]);
    JS_FreeValue(ctx, resolving[0]);
    JS_FreeValue(ctx, resolving[1]);
    if (!task) {
        JS_FreeValue(ctx, promise);
        job->Free();
        delete job;
        return JS_ThrowRangeError(ctx, "the runtime reached its outstanding host task limit");
    }
    job->control = ShellRuntimeAccess::Control(impl->owner);
    ControlRetain(job->control);
    job->task = task;
    ShellTask* shellTask = FindTask(impl, task);
    shellTask->fsJob = job;
    if (!ExecSpawn(MkFunc0(FsJobWork, job), MkFunc0(FsJobDone, job))) {
        shellTask->fsJob = nullptr;
        ForgetTask(impl, task, false);
        ControlRelease(job->control);
        JS_FreeValue(ctx, promise);
        job->Free();
        delete job;
        return JS_ThrowInternalError(ctx, "filesystem operation could not start background work");
    }
    return promise;
}

static JSValue NativeProcessRun(JSContext* ctx, JSValueConst, int argc,
                                JSValueConst* argv) {
    Arena* arena = ArenaNew();
    Str command;
    bool converted = argc >= 1 && JsString(ctx, argv[0], arena, &command);
    bool release = false;
    Policy* policy = CurrentPolicy(&release);
    bool allowed = converted && PolicyCapabilities(policy).MayRun(command);
    if (release) PolicyRelease(policy);
    if (converted && !allowed) {
        JSValue result = JS_ThrowTypeError(
            ctx, "running `%.*s` is not granted; add it to capabilities.fs.execute",
            command.len, command.s);
        ArenaDelete(arena);
        return result;
    }
    if (!converted) {
        ArenaDelete(arena);
        return JS_EXCEPTION;
    }
    shell::ScopeHostContext host = shell::ScopeCurrentHost();
    if (!host.IsSet()) {
        ArenaDelete(arena);
        return JS_ThrowTypeError(ctx, "process.run() needs a live host task");
    }

    ProcessJob* job = new ProcessJob();
    job->command = StrDup(command);
    bool ok = job->command.s != nullptr;
    int64_t argCount = 0;
    if (ok && argc > 1 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
        if (!JS_IsArray(argv[1]) || JS_GetLength(ctx, argv[1], &argCount) < 0) {
            JS_ThrowTypeError(ctx, "process.run(command, args) expects args to be an array of strings");
            ok = false;
        } else if (argCount < 0 || argCount > 4096) {
            JS_ThrowRangeError(ctx, "process.run(command, args) accepts at most 4096 arguments");
            ok = false;
        }
    }
    int totalBytes = 0;
    for (int64_t i = 0; ok && i < argCount; i++) {
        JSValue value = JS_GetPropertyUint32(ctx, argv[1], (uint32_t)i);
        Str argument;
        bool stringOk = !JS_IsException(value) &&
                        JsString(ctx, value, arena, &argument);
        JS_FreeValue(ctx, value);
        if (!stringOk) {
            ok = false;
            break;
        }
        if (argument.len > 1024 * 1024 - totalBytes) {
            JS_ThrowRangeError(ctx, "process.run arguments exceed the 1 MiB limit");
            ok = false;
            break;
        }
        Str copy = StrDup(argument);
        if (!copy.s || !job->args.Append(copy)) {
            StrFree(copy);
            JS_ThrowOutOfMemory(ctx);
            ok = false;
            break;
        }
        totalBytes += argument.len;
    }
    ArenaDelete(arena);
    if (!ok) {
        job->Free();
        delete job;
        return JS_EXCEPTION;
    }

    JSValue resolving[2] = {JS_UNDEFINED, JS_UNDEFINED};
    JSValue promise = JS_NewPromiseCapability(ctx, resolving);
    if (JS_IsException(promise)) {
        job->Free();
        delete job;
        return JS_EXCEPTION;
    }
    ShellRuntimeImpl* impl = (ShellRuntimeImpl*)JS_GetContextOpaque(ctx);
    uint32_t task = NewTask(impl, ShellTaskKind::Process, resolving[0],
                            host.GetApp(), host.GetWindow(), false,
                            resolving[1]);
    JS_FreeValue(ctx, resolving[0]);
    JS_FreeValue(ctx, resolving[1]);
    if (!task) {
        JS_FreeValue(ctx, promise);
        job->Free();
        delete job;
        return JS_ThrowRangeError(ctx, "the runtime reached its outstanding host task limit");
    }
    job->control = ShellRuntimeAccess::Control(impl->owner);
    ControlRetain(job->control);
    job->task = task;
    ShellTask* shellTask = FindTask(impl, task);
    shellTask->processJob = job;
    if (!ExecSpawn(MkFunc0(ProcessJobWork, job),
                   MkFunc0(ProcessJobDone, job))) {
        shellTask->processJob = nullptr;
        ForgetTask(impl, task, false);
        ControlRelease(job->control);
        JS_FreeValue(ctx, promise);
        job->Free();
        delete job;
        return JS_ThrowInternalError(ctx, "process.run could not start background work");
    }
    return promise;
}

static JSValue NativeProcessExit(JSContext* ctx, JSValueConst, int argc,
                                 JSValueConst* argv) {
    int32_t code = 0;
    if (argc > 0 && JS_ToInt32(ctx, &code, argv[0]) < 0) return JS_EXCEPTION;
    bool release = false;
    Policy* policy = CurrentPolicy(&release);
    bool allowed = PolicyCapabilities(policy).MayExit();
    if (release) PolicyRelease(policy);
    if (!allowed) {
        return JS_ThrowTypeError(ctx, "process.exit() is not granted; set capabilities.process.exit to true");
    }
    if (!gShellExitHandler) {
        return JS_ThrowInternalError(ctx, "process.exit() is granted but the host installed no ShellOnExitRequest handler");
    }
    shell::ScopeHostContext host = shell::ScopeCurrentHost();
    if (!host.IsSet()) return JS_ThrowTypeError(ctx, "process.exit() needs a live host call");
    Ctx native = {};
    native.app = host.GetApp();
    native.win = host.GetWindow();
    native.a = host.GetWindow()->frameArena;
    native.self = shell::ScopeCurrentView();
    ShellExitRequest request = {code, native.self};
    gShellExitHandler(request, &native);
    return JS_UNDEFINED;
}

static bool StandardBytes(JSContext* ctx, JSValueConst value,
                          const char* operation, Str* bytes) {
    if (JS_GetTypedArrayType(value) != JS_TYPED_ARRAY_UINT8) {
        JS_ThrowTypeError(ctx, "%s expects a Uint8Array", operation);
        return false;
    }
    size_t count = 0;
    uint8_t* data = JS_GetUint8Array(ctx, &count, value);
    if ((!data && count != 0) || count > (size_t)shell::kStandardDataLimit) {
        JS_ThrowRangeError(ctx, "%s input exceeds the 64 MiB limit",
                           operation);
        return false;
    }
    *bytes = Str((const char*)data, (int)count);
    return true;
}

static JSValue NativeSha256(JSContext* ctx, JSValueConst, int argc,
                            JSValueConst* argv) {
    Str input;
    if (argc < 1 ||
        !StandardBytes(ctx, argv[0], "crypto.createHash", &input)) {
        return JS_EXCEPTION;
    }
    uint8_t digest[32];
    shell::Sha256(input, digest);
    return JS_NewUint8ArrayCopy(ctx, digest, sizeof(digest));
}

static JSValue NativeRandom(JSContext* ctx, JSValueConst, int argc,
                            JSValueConst* argv) {
    double requested = 0;
    if (argc < 1 || JS_ToFloat64(ctx, &requested, argv[0]) < 0) {
        return JS_EXCEPTION;
    }
    if (!isfinite(requested) || requested < 0 ||
        requested > shell::kStandardDataLimit ||
        requested != (double)(int)requested) {
        return JS_ThrowRangeError(
            ctx, "crypto.randomBytes size must be a whole number from 0 to 67108864");
    }
    int count = (int)requested;
    Vec<uint8_t> bytes;
    if ((count > 0 && !bytes.AppendBlanks(count)) ||
        !shell::SecureRandom(bytes.els, count)) {
        return JS_ThrowInternalError(ctx, "the platform secure random generator failed");
    }
    return JS_NewUint8ArrayCopy(ctx, bytes.els, (size_t)bytes.len);
}

static JSValue NativeZlib(JSContext* ctx, JSValueConst, int argc,
                          JSValueConst* argv, int magic) {
    static const char* names[4] = {
        "zlib.deflateSync", "zlib.inflateSync", "zlib.gzipSync",
        "zlib.gunzipSync"};
    if (magic < 0 || magic >= 4) {
        return JS_ThrowInternalError(ctx, "invalid compression operation");
    }
    Str input;
    if (argc < 1 || !StandardBytes(ctx, argv[0], names[magic], &input)) {
        return JS_EXCEPTION;
    }
    Str output;
    Str error;
    bool inflate = (magic & 1) != 0;
    bool gzip = magic >= 2;
    bool ok = inflate ? shell::ZlibInflate(input, gzip, &output, &error)
                      : shell::ZlibDeflate(input, gzip, &output, &error);
    if (!ok) {
        const char* message = error.s ? error.s : "compression operation failed";
        int messageLen = error.s ? error.len : (int)strlen(message);
        JSValue result = JS_ThrowTypeError(ctx, "%.*s", messageLen, message);
        StrFree(error);
        StrFree(output);
        return result;
    }
    JSValue result = JS_NewUint8ArrayCopy(
        ctx, (const uint8_t*)(output.s ? output.s : ""),
        (size_t)output.len);
    StrFree(error);
    StrFree(output);
    return result;
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
  const taskHandle = (id) => Object.freeze({
    cancel: () => __task_cancel(id),
    is_done: () => __task_is_done(id),
  });
  let ambientContext;
  const spawn = (body, options) => {
    if (typeof body !== "function") throw new TypeError("cx.spawn(fn) expects a function");
    const id = __task_new(options);
    let started;
    try {
      started = body(ambientContext);
    } catch (error) {
      __task_reject(id, error);
      return taskHandle(id);
    }
    Promise.resolve(started).then(
      () => __task_finish(id),
      (error) => __task_reject(id, error),
    );
    return taskHandle(id);
  };
  const timer = Object.freeze({
    after: (ms, handler, options) => taskHandle(__timer_after(Number(ms), handler, options)),
    every: (ms, handler, options) => taskHandle(__timer_every(Number(ms), handler, options)),
  });
  const storage = (session) => {
    const object = {
      key: (index) => Number.isInteger(index) && index >= 0 ? __storage_key(index, session) : null,
      getItem: (key) => __storage_get(String(key), session),
      setItem: (key, value) => __storage_set(String(key), String(value), session),
      removeItem: (key) => __storage_remove(String(key), session),
      clear: () => __storage_clear(session),
      flush: () => __storage_flush(session),
    };
    Object.defineProperty(object, "length", { get: () => __storage_length(session) });
    return Object.freeze(object);
  };
  const localStorage = storage(false);
  const sessionStorage = storage(true);
  globalThis.__context = (generation) => Object.freeze({
    notify: () => __cx_notify(generation),
    focus_handle: () => focusHandle(__focus_handle_new()),
    sleep: (ms = 0) => __sleep(Number(ms)),
    spawn,
    timer,
    read_from_clipboard: () => __clipboard_read_text(),
    write_to_clipboard: (text) => __clipboard_write_text(String(text)),
  });
  ambientContext = Object.freeze({
    notify: () => __cx_notify_current(),
    focus_handle: () => focusHandle(__focus_handle_new()),
    sleep: (ms = 0) => __sleep(Number(ms)),
    spawn,
    timer,
    read_from_clipboard: () => __clipboard_read_text(),
    write_to_clipboard: (text) => __clipboard_write_text(String(text)),
  });
  globalThis.__ambient_context = ambientContext;
  globalThis.window = Object.freeze({ localStorage, sessionStorage });
  globalThis.localStorage = localStorage;
  globalThis.sessionStorage = sessionStorage;
)JS"
R"JS(
  globalThis.console = Object.freeze({
    log: (...args) => __console_log(...args),
    debug: (...args) => __console_debug(...args),
    info: (...args) => __console_info(...args),
    warn: (...args) => __console_warn(...args),
    error: (...args) => __console_error(...args),
  });
  globalThis.__shell_fs_dirent = (name, directory) => Object.freeze({
    name,
    isDirectory: () => directory,
  });
  globalThis.__shell_fs = Object.freeze({
    readFile: (path, encoding) => __fs_read(path, encoding),
    writeFile: (path, contents) => __fs_write(path, contents),
    readdir: (path, options) => __fs_readdir(path, options),
    exists: (path) => __fs_exists(path),
    unlink: (path) => __fs_unlink(path),
    rmdir: (path) => __fs_rmdir(path),
    mkdir: (path, options) => __fs_mkdir(path, options),
  });

  const utf8Encode = (text) => {
    const encoded = encodeURIComponent(String(text));
    const bytes = [];
    for (let i = 0; i < encoded.length; i++) {
      if (encoded[i] === "%") {
        bytes.push(parseInt(encoded.slice(i + 1, i + 3), 16));
        i += 2;
      } else bytes.push(encoded.charCodeAt(i));
    }
    return bytes;
  };
  const utf8Decode = (bytes) => {
    let encoded = "";
    for (const byte of bytes) encoded += byte < 128 && byte !== 37
      ? String.fromCharCode(byte)
      : "%" + byte.toString(16).padStart(2, "0");
    return decodeURIComponent(encoded);
  };
  const b64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const base64Encode = (bytes) => {
    let out = "";
    for (let i = 0; i < bytes.length; i += 3) {
      const a = bytes[i], b = i + 1 < bytes.length ? bytes[i + 1] : 0;
      const c = i + 2 < bytes.length ? bytes[i + 2] : 0;
      out += b64chars[a >> 2] + b64chars[((a & 3) << 4) | (b >> 4)] +
        (i + 1 < bytes.length ? b64chars[((b & 15) << 2) | (c >> 6)] : "=") +
        (i + 2 < bytes.length ? b64chars[c & 63] : "=");
    }
    return out;
  };
  const base64Decode = (text) => {
    const clean = String(text).replace(/\s/g, "");
    const out = [];
    for (let i = 0; i < clean.length; i += 4) {
      const a = b64chars.indexOf(clean[i]), b = b64chars.indexOf(clean[i + 1]);
      const c = clean[i + 2] === "=" ? 0 : b64chars.indexOf(clean[i + 2]);
      const d = clean[i + 3] === "=" ? 0 : b64chars.indexOf(clean[i + 3]);
      if (a < 0 || b < 0 || c < 0 || d < 0) throw new TypeError("invalid base64");
      out.push((a << 2) | (b >> 4));
      if (clean[i + 2] !== "=") out.push(((b & 15) << 4) | (c >> 2));
      if (clean[i + 3] !== "=") out.push(((c & 3) << 6) | d);
    }
    return out;
  };
  class Buffer extends Uint8Array {
    static from(value, encoding = "utf8") {
      if (typeof value === "string") {
        const bytes = encoding === "hex"
          ? (value.match(/../g) ?? []).map(part => parseInt(part, 16))
          : encoding === "base64" ? base64Decode(value) : utf8Encode(value);
        return new Buffer(bytes);
      }
      return new Buffer(value instanceof ArrayBuffer ? new Uint8Array(value) : value);
    }
    static alloc(size, fill = 0) { const out = new Buffer(Number(size)); out.fill(fill); return out; }
    static allocUnsafe(size) { return new Buffer(Number(size)); }
    static isBuffer(value) { return value instanceof Buffer; }
    static byteLength(value, encoding) { return Buffer.from(value, encoding).length; }
    static concat(values, length) {
      const size = length == null ? values.reduce((sum, value) => sum + value.length, 0) : Number(length);
      const out = Buffer.alloc(size); let at = 0;
      for (const value of values) { out.set(value.subarray(0, size - at), at); at += value.length; if (at >= size) break; }
      return out;
    }
    toString(encoding = "utf8", start = 0, end = this.length) {
      const bytes = this.subarray(start, end);
      if (encoding === "hex") return Array.from(bytes, byte => byte.toString(16).padStart(2, "0")).join("");
      if (encoding === "base64") return base64Encode(bytes);
      return utf8Decode(bytes);
    }
  }
  globalThis.Buffer = Buffer;
  globalThis.__shell_buffer = Object.freeze({ Buffer });

  const standardBytes = (value, operation) => {
    if (typeof value === "string") return Buffer.from(value);
    if (value instanceof Uint8Array) return value;
    throw new TypeError(operation + " expects a string or Uint8Array");
  };
  class Hash {
    constructor(algorithm) {
      const name = String(algorithm).toLowerCase().replaceAll("-", "");
      if (name !== "sha256") throw new TypeError("'" + algorithm + "' not available");
      this.chunks = [];
      this.size = 0;
      this.done = false;
    }
    update(value) {
      if (this.done) throw new Error("Digest already called");
      const bytes = standardBytes(value, "Hash.update");
      if (bytes.length > 67108864 - this.size) throw new RangeError("hash input exceeds the 64 MiB limit");
      const copy = Buffer.from(bytes);
      this.chunks.push(copy);
      this.size += copy.length;
      return this;
    }
    digest(encoding) {
      if (this.done) throw new Error("Digest already called");
      this.done = true;
      const result = Buffer.from(__crypto_sha256(Buffer.concat(this.chunks, this.size)));
      this.chunks = [];
      if (encoding === undefined) return result;
      const name = String(encoding).toLowerCase();
      if (name !== "hex" && name !== "base64" && name !== "utf8" && name !== "utf-8") {
        throw new TypeError("unsupported digest encoding: " + encoding);
      }
      return result.toString(name);
    }
  }
  const createHash = (algorithm) => new Hash(algorithm);
  const randomBytes = (size) => Buffer.from(__crypto_random(size));
  const getRandomValues = (value) => {
    if (!ArrayBuffer.isView(value) || value instanceof DataView ||
        value instanceof Float32Array || value instanceof Float64Array) {
      throw new TypeError("crypto.getRandomValues expects an integer typed array");
    }
    if (value.byteLength > 65536) throw new RangeError("crypto.getRandomValues accepts at most 65536 bytes");
    new Uint8Array(value.buffer, value.byteOffset, value.byteLength).set(__crypto_random(value.byteLength));
    return value;
  };
  const randomUUID = () => {
    const bytes = randomBytes(16);
    bytes[6] = (bytes[6] & 15) | 64;
    bytes[8] = (bytes[8] & 63) | 128;
    const hex = bytes.toString("hex");
    return hex.slice(0, 8) + "-" + hex.slice(8, 12) + "-" +
      hex.slice(12, 16) + "-" + hex.slice(16, 20) + "-" + hex.slice(20);
  };
  const subtle = Object.freeze({
    digest: (algorithm, value) => {
      const name = String(typeof algorithm === "object" ? algorithm?.name : algorithm)
        .toLowerCase().replaceAll("-", "");
      if (name !== "sha256") return Promise.reject(new TypeError("unsupported digest algorithm"));
      let bytes;
      if (value instanceof ArrayBuffer) bytes = new Uint8Array(value);
      else if (ArrayBuffer.isView(value)) {
        bytes = new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
      } else return Promise.reject(new TypeError("crypto.subtle.digest expects a BufferSource"));
      const digest = __crypto_sha256(bytes);
      return Promise.resolve(digest.buffer);
    },
  });
  const webcrypto = Object.freeze({ getRandomValues, randomUUID, subtle });
  globalThis.crypto = webcrypto;
  globalThis.__shell_crypto = Object.freeze({
    createHash, randomBytes, randomUUID, getRandomValues,
    crypto: webcrypto, webcrypto,
  });

  const compression = (native, name) => (value) =>
    Buffer.from(native(standardBytes(value, "zlib." + name)));
  const deflateSync = compression(__zlib_deflate, "deflateSync");
  const inflateSync = compression(__zlib_inflate, "inflateSync");
  const gzipSync = compression(__zlib_gzip, "gzipSync");
  const gunzipSync = compression(__zlib_gunzip, "gunzipSync");
  globalThis.__shell_zlib = Object.freeze({
    deflateSync, inflateSync, gzipSync, gunzipSync,
  });
)JS"
R"JS(
  const pathSep = __shell_is_windows ? "\\" : "/";
  const pathDelimiter = __shell_is_windows ? ";" : ":";
  const pathParts = (value) => String(value).replace(/\\/g, "/").split("/");
  const pathNormalize = (value) => {
    value = String(value);
    const slash = value.replace(/\\/g, "/");
    const drive = __shell_is_windows && slash.length >= 2 && slash[1] === ":" ? slash.slice(0, 2) : "";
    const absolute = slash.startsWith("/") || drive !== "";
    const out = [];
    for (const part of pathParts(drive ? slash.slice(2) : slash)) {
      if (!part || part === ".") continue;
      if (part === "..") { if (out.length && out[out.length - 1] !== "..") out.pop(); else if (!absolute) out.push(part); }
      else out.push(part);
    }
    let result = (drive ? drive + "/" : absolute ? "/" : "") + out.join("/");
    if (!result) result = absolute ? "/" : ".";
    return __shell_is_windows ? result.replace(/\//g, "\\") : result;
  };
  const pathApi = {
    sep: pathSep, delimiter: pathDelimiter,
    normalize: pathNormalize,
    isAbsolute: (value) => { const text = String(value); return text.startsWith("/") || text.startsWith("\\") || (__shell_is_windows && text.length > 2 && text[1] === ":"); },
    join: (...values) => pathNormalize(values.filter(value => String(value).length).join(pathSep)),
    resolve: (...values) => pathNormalize(values.join(pathSep)),
    basename: (value, suffix = "") => { const parts = pathParts(value).filter(Boolean); let name = parts.pop() ?? ""; suffix = String(suffix); if (suffix && name.endsWith(suffix)) name = name.slice(0, -suffix.length); return name; },
    dirname: (value) => { const text = pathNormalize(value); const at = Math.max(text.lastIndexOf("/"), text.lastIndexOf("\\")); return at < 0 ? "." : at === 0 ? pathSep : text.slice(0, at); },
    extname: (value) => { const name = pathApi.basename(value); const at = name.lastIndexOf("."); return at <= 0 ? "" : name.slice(at); },
    relative: (from, to) => {
      const a = pathParts(pathNormalize(from)).filter(Boolean), b = pathParts(pathNormalize(to)).filter(Boolean);
      let same = 0; while (same < a.length && same < b.length && a[same].toLowerCase() === b[same].toLowerCase()) same++;
      return [...a.slice(same).map(() => ".."), ...b.slice(same)].join(pathSep) || "";
    },
    parse: (value) => { const dir = pathApi.dirname(value), base = pathApi.basename(value), ext = pathApi.extname(value); return { root: pathApi.isAbsolute(value) ? pathSep : "", dir, base, ext, name: ext ? base.slice(0, -ext.length) : base }; },
    format: (parts) => (parts.dir || parts.root || "") + ((parts.dir || parts.root) ? pathSep : "") + (parts.base || ((parts.name || "") + (parts.ext || ""))),
  };
  globalThis.__shell_path = Object.freeze(pathApi);
)JS"
R"JS(
  class URLSearchParams {
    constructor(value = "") { this.items = []; const text = String(value).replace(/^\?/, ""); if (text) for (const part of text.split("&")) { const at = part.indexOf("="); this.append(decodeURIComponent(at < 0 ? part : part.slice(0, at)), decodeURIComponent(at < 0 ? "" : part.slice(at + 1))); } }
    append(key, value) { this.items.push([String(key), String(value)]); }
    get(key) { const found = this.items.find(item => item[0] === String(key)); return found ? found[1] : null; }
    getAll(key) { return this.items.filter(item => item[0] === String(key)).map(item => item[1]); }
    has(key) { return this.items.some(item => item[0] === String(key)); }
    set(key, value) { this.delete(key); this.append(key, value); }
    delete(key) { key = String(key); this.items = this.items.filter(item => item[0] !== key); }
    toString() { return this.items.map(item => encodeURIComponent(item[0]) + "=" + encodeURIComponent(item[1])).join("&"); }
    *entries() { yield* this.items; }
    [Symbol.iterator]() { return this.entries(); }
  }
  class URL {
    constructor(input, base) {
      let text = String(input);
      if (base && !text.includes(":")) text = String(base).replace(/[^/]*$/, "") + text;
      const scheme = text.indexOf(":");
      if (scheme <= 0) throw new TypeError("invalid URL");
      this.protocol = text.slice(0, scheme + 1);
      let rest = text.slice(scheme + 1), authority = "";
      if (rest.startsWith("//")) { rest = rest.slice(2); const end = rest.search(/[\/#?]/); authority = end < 0 ? rest : rest.slice(0, end); rest = end < 0 ? "" : rest.slice(end); }
      const hashAt = rest.indexOf("#"); this.hash = hashAt < 0 ? "" : rest.slice(hashAt); if (hashAt >= 0) rest = rest.slice(0, hashAt);
      const searchAt = rest.indexOf("?"); this.search = searchAt < 0 ? "" : rest.slice(searchAt); this.pathname = searchAt < 0 ? rest : rest.slice(0, searchAt);
      this.pathname ||= "/"; this.host = authority; const portAt = authority.lastIndexOf(":"); this.hostname = portAt > 0 ? authority.slice(0, portAt) : authority; this.port = portAt > 0 ? authority.slice(portAt + 1) : "";
      this.searchParams = new URLSearchParams(this.search);
    }
    get origin() { return this.protocol + "//" + this.host; }
    get href() { const query = this.searchParams.toString(); return this.protocol + (this.host ? "//" + this.host : "") + this.pathname + (query ? "?" + query : "") + this.hash; }
    set href(value) { const parsed = new URL(value); Object.assign(this, parsed); }
    toString() { return this.href; }
    toJSON() { return this.href; }
  }
  const urlApi = {
    URL, URLSearchParams,
    pathToFileURL: (path) => new URL("file://" + (__shell_is_windows ? "/" : "") + String(path).replace(/\\/g, "/")),
    fileURLToPath: (url) => { const parsed = url instanceof URL ? url : new URL(url); if (parsed.protocol !== "file:") throw new TypeError("URL is not file:"); const path = decodeURIComponent(parsed.pathname); return __shell_is_windows ? path.replace(/^\//, "").replace(/\//g, "\\") : path; },
  };
  globalThis.URL = URL;
  globalThis.URLSearchParams = URLSearchParams;
  globalThis.__shell_url = Object.freeze(urlApi);

  globalThis.__shell_fetch_response = (status, url, body) => Object.freeze({
    status,
    ok: status >= 200 && status < 300,
    url,
    text: () => Promise.resolve(body),
    json: () => Promise.resolve().then(() => JSON.parse(body)),
  });
  globalThis.fetch = (url, options) => {
    if (options !== undefined && options !== null) {
      if (typeof options !== "object" || Array.isArray(options)) {
        throw new TypeError("fetch(url, options) expects an options object");
      }
      for (const key of Object.keys(options)) {
        if (key !== "method" && key !== "headers" && key !== "body") {
          throw new TypeError("unknown option `" + key + "` for fetch(url, options)");
        }
      }
      const method = options.method === undefined ? "GET" : String(options.method).toUpperCase();
      if (method !== "GET") {
        throw new TypeError("this port permits GET only; POST is outside the repository network boundary");
      }
      if (options.body !== undefined && options.body !== null) {
        throw new TypeError("a GET fetch may not carry a request body");
      }
      if (options.headers !== undefined && options.headers !== null &&
          Object.keys(options.headers).length !== 0) {
        throw new TypeError("custom fetch headers are outside the repository network boundary");
      }
    }
    return __fetch_get(new URL(String(url)).href);
  };

  globalThis.__shell_os = Object.freeze({
    platform: () => __shell_platform,
    arch: () => __shell_arch,
    EOL: __shell_is_windows ? "\r\n" : "\n",
  });
  globalThis.process = Object.freeze({
    run: (...args) => __process_run(...args),
    nextTick: (callback, ...args) => Promise.resolve().then(() => callback(...args)),
    exit: (code = 0) => __process_exit(Number(code)),
    platform: __shell_platform,
    arch: __shell_arch,
  });
)JS"
R"JS(
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
    with_cx: (body) => {
      if (typeof body !== "function") throw new TypeError("with_cx(fn) expects a function");
      return body(ambientContext);
    },
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

static const char kSandbox[] = R"JS(
(() => {
  const unavailable = (name, hint) => function () {
    throw new TypeError("`" + name + "` is not available in the shell: " + hint);
  };
  for (const [name, hint] of [
    ["setTimeout", "use cx.timer.after(ms, callback)"],
    ["setInterval", "use cx.timer.every(ms, callback)"],
    ["clearTimeout", "cancel the handle returned by cx.timer.after"],
    ["clearInterval", "cancel the handle returned by cx.timer.every"],
    ["require", "this runtime uses ES modules; use `import`"],
  ]) {
    if (!(name in globalThis)) globalThis[name] = unavailable(name, hint);
  }
  const hint = "the shell sandbox withholds dynamic code; enable development mode to allow it";
  const deny = (label) => function () {
    throw new TypeError(label + " is disabled: " + hint);
  };
  const replaceConstructor = (holder, value) => {
    Object.defineProperty(Object.getPrototypeOf(holder), "constructor", {
      value, writable: true, enumerable: false, configurable: true,
    });
  };
  const blocked = deny("the Function constructor");
  blocked.prototype = Function.prototype;
  replaceConstructor(function () {}, blocked);
  replaceConstructor(async function () {}, deny("the AsyncFunction constructor"));
  replaceConstructor(function* () {}, deny("the GeneratorFunction constructor"));
  replaceConstructor(async function* () {}, deny("the AsyncGeneratorFunction constructor"));
  globalThis.Function = blocked;
  delete globalThis.eval;
  for (const proto of [Object.prototype, Array.prototype, Function.prototype,
                       String.prototype, Number.prototype]) Object.freeze(proto);
})();
)JS";

static bool InstallRuntime(ShellRuntimeImpl* impl, ShellError* error) {
    JSValue global = JS_GetGlobalObject(impl->context);
#if GPUI_OS_WINDOWS
    const char* platform = "windows";
#elif GPUI_OS_MAC
    const char* platform = "macos";
#elif GPUI_OS_WASM
    const char* platform = "emscripten";
#else
    const char* platform = "linux";
#endif
#if defined(_M_ARM64) || defined(__aarch64__)
    const char* architecture = "aarch64";
#elif defined(__wasm32__)
    const char* architecture = "wasm32";
#elif defined(_M_IX86) || defined(__i386__)
    const char* architecture = "x86";
#else
    const char* architecture = "x86_64";
#endif
    JS_SetPropertyStr(impl->context, global, "__shell_is_windows",
                      JS_NewBool(impl->context, GPUI_OS_WINDOWS));
    JS_SetPropertyStr(impl->context, global, "__shell_platform",
                      JS_NewString(impl->context, platform));
    JS_SetPropertyStr(impl->context, global, "__shell_arch",
                      JS_NewString(impl->context, architecture));
    SetGlobalFunction(impl->context, global, "__component", NativeComponent, 4);
    SetGlobalFunction(impl->context, global, "__attach", NativeAttach, 2);
    SetGlobalFunction(impl->context, global, "__state", NativeState, 2);
    SetGlobalFunction(impl->context, global, "__slot", NativeSlot, 3);
    SetGlobalFunction(impl->context, global, "__apply", NativeApply, 3);
    SetGlobalFunction(impl->context, global, "__cx_notify", NativeNotify, 1);
    SetGlobalFunction(impl->context, global, "__cx_notify_current",
                      NativeNotifyCurrent, 0);
    SetGlobalFunction(impl->context, global, "__task_new", NativeTaskNew, 1);
    SetGlobalFunction(impl->context, global, "__task_finish",
                      NativeTaskFinish, 1);
    SetGlobalFunction(impl->context, global, "__task_reject",
                      NativeTaskReject, 2);
    SetGlobalFunction(impl->context, global, "__task_cancel",
                      NativeTaskCancel, 1);
    SetGlobalFunction(impl->context, global, "__task_is_done",
                      NativeTaskIsDone, 1);
    SetGlobalFunction(impl->context, global, "__sleep", NativeSleep, 1);
    SetGlobalMagicFunction(impl->context, global, "__timer_after",
                           NativeTimer, 3, 0);
    SetGlobalMagicFunction(impl->context, global, "__timer_every",
                           NativeTimer, 3, 1);
    SetGlobalFunction(impl->context, global, "__storage_get",
                      NativeStorageGet, 2);
    SetGlobalFunction(impl->context, global, "__storage_set",
                      NativeStorageSet, 3);
    SetGlobalFunction(impl->context, global, "__storage_remove",
                      NativeStorageRemove, 2);
    SetGlobalFunction(impl->context, global, "__storage_clear",
                      NativeStorageClear, 1);
    SetGlobalFunction(impl->context, global, "__storage_length",
                      NativeStorageLength, 1);
    SetGlobalFunction(impl->context, global, "__storage_key",
                      NativeStorageKey, 2);
    SetGlobalFunction(impl->context, global, "__storage_flush",
                      NativeStorageFlush, 1);
    SetGlobalMagicFunction(impl->context, global, "__clipboard_read_text",
                           NativeClipboard, 0, 0);
    SetGlobalMagicFunction(impl->context, global, "__clipboard_write_text",
                           NativeClipboard, 1, 1);
    SetGlobalMagicFunction(impl->context, global, "__console_log",
                           NativeConsole, 1, 0);
    SetGlobalMagicFunction(impl->context, global, "__console_debug",
                           NativeConsole, 1, 1);
    SetGlobalMagicFunction(impl->context, global, "__console_info",
                           NativeConsole, 1, 2);
    SetGlobalMagicFunction(impl->context, global, "__console_warn",
                           NativeConsole, 1, 3);
    SetGlobalMagicFunction(impl->context, global, "__console_error",
                           NativeConsole, 1, 4);
    SetGlobalFunction(impl->context, global, "__crypto_sha256",
                      NativeSha256, 1);
    SetGlobalFunction(impl->context, global, "__crypto_random",
                      NativeRandom, 1);
    SetGlobalMagicFunction(impl->context, global, "__zlib_deflate",
                           NativeZlib, 1, 0);
    SetGlobalMagicFunction(impl->context, global, "__zlib_inflate",
                           NativeZlib, 1, 1);
    SetGlobalMagicFunction(impl->context, global, "__zlib_gzip",
                           NativeZlib, 1, 2);
    SetGlobalMagicFunction(impl->context, global, "__zlib_gunzip",
                           NativeZlib, 1, 3);
    SetGlobalFunction(impl->context, global, "__fetch_get", NativeFetch, 1);
    SetGlobalMagicFunction(impl->context, global, "__fs_read", NativeFs, 2,
                           (int)shell::FsOperation::Read);
    SetGlobalMagicFunction(impl->context, global, "__fs_write", NativeFs, 2,
                           (int)shell::FsOperation::Write);
    SetGlobalMagicFunction(impl->context, global, "__fs_readdir", NativeFs,
                           2, (int)shell::FsOperation::ReadDirectory);
    SetGlobalMagicFunction(impl->context, global, "__fs_exists", NativeFs, 1,
                           (int)shell::FsOperation::Exists);
    SetGlobalMagicFunction(impl->context, global, "__fs_unlink", NativeFs, 1,
                           (int)shell::FsOperation::RemoveFile);
    SetGlobalMagicFunction(impl->context, global, "__fs_rmdir", NativeFs, 1,
                           (int)shell::FsOperation::RemoveDirectory);
    SetGlobalMagicFunction(impl->context, global, "__fs_mkdir", NativeFs, 2,
                           (int)shell::FsOperation::MakeDirectory);
    SetGlobalFunction(impl->context, global, "__process_run",
                      NativeProcessRun, 2);
    SetGlobalFunction(impl->context, global, "__process_exit",
                      NativeProcessExit, 1);
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
    if (!ShellDevelopmentMode()) {
        BeginExecution(impl);
        result = JS_Eval(impl->context, kSandbox, sizeof(kSandbox) - 1,
                         "<gpui-shell sandbox>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(result)) return CaptureException(impl, error);
        JS_FreeValue(impl->context, result);
    }
    return true;
}

ShellRuntime::ShellRuntime() = default;

ShellRuntime::~ShellRuntime() {
    if (control) control->runtime = nullptr;
    if (impl) {
        if (impl->context) {
            for (int i = impl->tasks.len - 1; i >= 0; i--) {
                DestroyTask(impl, impl->tasks[i], true);
            }
            impl->tasks.Reset();
            if (impl->taskDriver.IsValid() && impl->taskApp) {
                ShellTaskDriver* driver = impl->taskDriver.Get(impl->taskApp);
                if (driver) driver->runtime = nullptr;
                EntityDrop(impl->taskApp, impl->taskDriver.id);
                impl->taskDriver = {};
            }
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
    shell::CallScopeGuard scope = shell::ScopeEnter(
        nullptr, nullptr, ScopePhase::Task, {}, nullptr, runtime, application);
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

int ShellRuntime::LiveTasks() const { return impl->tasks.len; }

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
    for (int i = impl->tasks.len - 1; i >= 0; i--) {
        if (impl->tasks[i]->owner == view) {
            ForgetTask(impl, impl->tasks[i]->id);
        }
    }
    Vec<shell::CallbackId> callbacks;
    impl->retained.ReleaseOwner(view, &callbacks);
    for (int i = 0; i < callbacks.len; i++) {
        impl->callbacks.RetireId(impl->context, callbacks[i]);
    }
    callbacks.Reset();
}

void ShellRuntime::ResumeTask(uint32_t id, Ctx* cx) {
    if (!cx || !cx->app || !cx->win) return;
    ShellTask* task = FindTask(impl, id);
    if (!task || task->kind == ShellTaskKind::Spawn) return;
    if (task->owner.IsValid() && !EntityGet(cx->app, task->owner)) {
        ForgetTask(impl, id, false);
        return;
    }

    ShellTaskKind kind = task->kind;
    EntityId owner = task->owner;
    Policy* policy = PolicyRetain(task->policy);
    AppModule* application = task->application;
    JSValue callback = JS_DupValue(impl->context, task->callback);
    bool repeating = kind == ShellTaskKind::TimerEvery;
    if (!repeating) ForgetTask(impl, id, false);

    shell::CallScopeGuard scope = shell::ScopeEnter(
        cx->win, cx->app, ScopePhase::Task, owner, policy, this, application);
    PolicyRelease(policy);
    JSValue result = JS_UNDEFINED;
    if (kind == ShellTaskKind::Sleep) {
        result = JS_Call(impl->context, callback, JS_UNDEFINED, 0, nullptr);
    } else {
        JSValue global = JS_GetGlobalObject(impl->context);
        JSValue ambient = JS_GetPropertyStr(impl->context, global,
                                             "__ambient_context");
        result = JS_Call(impl->context, callback, JS_UNDEFINED, 1, &ambient);
        JS_FreeValue(impl->context, ambient);
        JS_FreeValue(impl->context, global);
    }
    JS_FreeValue(impl->context, callback);
    if (JS_IsException(result)) {
        Arena* arena = ArenaNew();
        log(ExceptionText(arena, impl->context));
        ArenaDelete(arena);
    } else {
        JS_FreeValue(impl->context, result);
    }
    ShellError error = {};
    DrainJobs(kMaxJobBatch, &error);
    if (error.IsSet()) {
        log(error.message);
        ShellErrorClear(&error);
    }
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

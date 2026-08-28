/* Engine-independent shell semantics, ported from crates/shell/src/{value,
 * metrics, capability, policy, scope, spec, snapshot}.rs. */

#include "Test.h"

#include <stdio.h>
#if !GPUI_OS_WINDOWS
#include <unistd.h>
#endif

using namespace gpui::shell;

static void BridgedValuesMatchJavaScriptConversions() {
    utassert(!BridgedIsTruthy(Bridged::Nil()));
    utassert(!BridgedIsTruthy(Bridged::Bool(false)));
    utassert(BridgedIsTruthy(Bridged::Bool(true)));
    utassert(!BridgedIsTruthy(Bridged::Number(0)));
    utassert(!BridgedIsTruthy(Bridged::Number(NAN)));
    utassert(BridgedIsTruthy(Bridged::Number(-2)));
    utassert(!BridgedIsTruthy(Bridged::String(StrL(""))));
    utassert(BridgedIsTruthy(Bridged::String(StrL("0"))));

    Hsla color = {};
    utassert(BridgedAsColor(Bridged::String(StrL("#123")), &color));
    Rgba rgb = HslaToRgba(color);
    utassert(rgb.r <= 0x11 && 0x11 - rgb.r <= 1);
    utassert(rgb.g <= 0x22 && 0x22 - rgb.g <= 1);
    utassert(rgb.b <= 0x33 && 0x33 - rgb.b <= 1 && rgb.a == 0xff);
    utassert(BridgedAsColor(Bridged::String(StrL("#10203080")), &color));
    rgb = HslaToRgba(color);
    utassert(rgb.r <= 0x10 && 0x10 - rgb.r <= 1);
    utassert(rgb.g <= 0x20 && 0x20 - rgb.g <= 1);
    utassert(rgb.b <= 0x30 && 0x30 - rgb.b <= 1);
    utassert(rgb.a == 0x80);

    ShellError error = {};
    utassert(!BridgedAsColor(Bridged::String(StrL("#12")), &color, &error));
    utassert(error.IsSet());
    ShellErrorClear(&error);
    utassert(!BridgedAsF32(Bridged::Bool(true), nullptr, &error));
    utassert(StrStartsWith(error.message, StrL("expected a number")));
    ShellErrorClear(&error);
}

static void RuntimeMetricsSeparateScriptNativeAndFrames() {
    Metrics metrics = {};
    MetricsAdd(&metrics, MetricsTimerKind::ScriptRender, 100);
    MetricsAdd(&metrics, MetricsTimerKind::ScriptRender, 300);
    MetricsAdd(&metrics, MetricsTimerKind::Native, 80);
    MetricsAdd(&metrics, MetricsTimerKind::Materialize, 40);
    MetricsAdd(&metrics, MetricsTimerKind::VirtualItems, 10);
    RuntimeMetrics reading = MetricsRead(&metrics);
    utassert(reading.scriptRenders == 2);
    utassert(reading.MeanScriptRenderNanos() == 200);
    utassert(reading.MeanNativeNanos() == 40);
    utassert(reading.MeanScriptOnlyNanos() == 160);
    utassert(reading.slowestScriptRenderNanos == 300);
    utassert(reading.materializations == 1);
    utassert(reading.MeanMaterializeNanos() == 50);

    RuntimeMetrics earlier = {};
    earlier.scriptRenders = 1;
    earlier.scriptRenderNanos = 120;
    earlier.nativeNanos = 100; // Later counter is smaller: saturate.
    earlier.materializations = 4;
    RuntimeMetrics delta = reading.Since(earlier);
    utassert(delta.scriptRenders == 1);
    utassert(delta.scriptRenderNanos == 280);
    utassert(delta.nativeNanos == 0);
    utassert(delta.materializations == 0);
    utassert(delta.slowestScriptRenderNanos == 300);
}

static void CapabilitiesAreDenyFirstAndScoped() {
    Capabilities denied;
    utassert(!denied.HasReadAccess());
    utassert(!denied.HasWriteAccess());
    utassert(!denied.HasStorage());
    utassert(!denied.MayReach(StrL("example.com")));
    utassert(!denied.MayRun(StrL("git")));
    utassert(!denied.IsClipboardReadable());
    utassert(!denied.IsClipboardWritable());
    utassert(!denied.MayExit());

    Str commands[] = {StrL("git"), StrL("bun")};
    ExecuteGrant execute = ExecuteGrant::Allowed(commands, 2);
    Capabilities allowed;
    allowed.SetExecute(execute)
        .AddNetworkHost(StrL("EXAMPLE.COM"))
        .Storage(true)
        .ClipboardRead(true)
        .ClipboardWrite(true)
        .Exit(true);
    utassert(allowed.MayRun(StrL("git")));
    utassert(!allowed.MayRun(StrL("Git")));
    utassert(allowed.MayReach(StrL("example.com")));
    utassert(allowed.MayRequest(StrL("https"), StrL("example.com"), 443, true,
                                StrL("DELETE"), StrL("/anything")));
    utassert(allowed.HasStorage() && allowed.IsClipboardReadable());
    utassert(allowed.IsClipboardWritable() && allowed.MayExit());

    HttpRequestGrant api(StrL("api.example.com"));
    api.AddMethod(StrL("get"))
        .AddMethod(StrL("POST"))
        .AddPath(StrL("/health"))
        .AddPathPrefix(StrL("/v1/items"));
    Capabilities scoped;
    scoped.AddHttpRequest(api);
    utassert(scoped.MayRequest(StrL("HTTPS"), StrL("API.EXAMPLE.COM"), 0, false,
                               StrL("GET"), StrL("/health")));
    utassert(scoped.MayRequest(StrL("https"), StrL("api.example.com"), 443,
                               true, StrL("post"), StrL("/v1/items/7")));
    utassert(!scoped.MayRequest(StrL("https"), StrL("api.example.com"), 444,
                                true, StrL("GET"), StrL("/health")));
    utassert(!scoped.MayRequest(StrL("https"), StrL("api.example.com"), 443,
                                true, StrL("PATCH"), StrL("/health")));
    utassert(!scoped.MayRequest(StrL("https"), StrL("api.example.com"), 443,
                                true, StrL("GET"), StrL("/v1/itemset")));
}

static void FilesystemGrantsReturnRootRelativeAuthority() {
#if GPUI_OS_WINDOWS
    Str root = StrL("C:/safe");
    Str absolute = StrL("c:\\safe\\dir\\file.txt");
#else
    Str root = StrL("/safe");
    Str absolute = StrL("/safe/dir/file.txt");
#endif
    Capabilities capabilities;
    capabilities.AddReadRoot(root).AddWriteRoot(root);
    CapabilityPath path = {};
    CapabilityError error = {};
    utassert(capabilities
                 .ResolvePath(absolute, CapabilityAccess::Read, &path, &error));
    utassert(StrEq(path.root, root));
    utassert(StrEq(path.relative, "dir/file.txt"));
    path.Free();
    utassert(capabilities.ResolvePath(StrL("inside/../file.txt"),
                                      CapabilityAccess::Write, &path, &error));
    utassert(StrEq(path.root, root));
    utassert(StrEq(path.relative, "file.txt"));
    path.Free();
    utassert(!capabilities.ResolvePath(StrL("../escape.txt"),
                                       CapabilityAccess::Read, &path, &error));
    utassert(error.kind == CapabilityErrorKind::OutsideRoots);
    CapabilityErrorFree(&error);

    Capabilities none;
    utassert(!none.ResolvePath(StrL("file.txt"), CapabilityAccess::Read, &path,
                               &error));
    utassert(error.kind == CapabilityErrorKind::NotGranted);
    Arena* arena = ArenaNew();
    utassert(StrStartsWith(CapabilityErrorMessage(arena, error),
                           StrL("filesystem read is not granted")));
    ArenaDelete(arena);
    CapabilityErrorFree(&error);
}

static void PoliciesFreezeCapabilityGrants() {
    PolicySetDefault(nullptr);
    Policy* old = PolicyDefault();
    utassert(!PolicyCapabilities(old).HasStorage());

    Capabilities wider;
    wider.Storage(true).ClipboardRead(true);
    PolicyUpdateDefaultCapabilities(wider);
    Policy* fresh = PolicyDefault();
    utassert(!PolicyCapabilities(old).HasStorage());
    utassert(PolicyCapabilities(fresh).HasStorage());
    utassert(PolicyCapabilities(fresh).IsClipboardReadable());
    PolicyRelease(old);
    PolicyRelease(fresh);
    PolicySetDefault(nullptr);
}

static void ScopeGenerationsExpireAdoptAndRefuseReentry() {
    App app;
    Window window;
    uint64_t first = 0;
    {
        CallScopeGuard outer = ScopeEnter(&window, &app, ScopePhase::Render);
        first = outer.Generation();
        utassert(first != 0 && ScopeCurrentGeneration() == first);
        utassert(ScopeCurrentPhase() == ScopePhase::Render);
        utassert(!ScopePhaseAllowsNotify(ScopeCurrentPhase()));
        ScopeHostContext host = ScopeHostForGeneration(first);
        utassert(host.IsSet() && host.GetWindow() == &window &&
                 host.GetApp() == &app);
        utassert(!ScopeCurrentHost().IsSet());
        ShellError error = {};
        utassert(!ScopeHostForGeneration(first, &error).IsSet());
        utassert(error.IsSet());
        ShellErrorClear(&error);
    }
    ShellError stale = {};
    utassert(!ScopeHostForGeneration(first, &stale).IsSet());
    utassert(StrEq(stale.message, ScopeStaleContextMessage()));
    ShellErrorClear(&stale);

    {
        CallScopeGuard layout = ScopeEnter(&window, &app, ScopePhase::Layout);
        utassert(layout.Generation() != first);
        ScopeAdopt(first);
        ScopeHostContext adopted = ScopeHostForGeneration(first);
        utassert(adopted.IsSet());
    }
    utassert(!ScopeHasCurrent());
}

static Component Kind(ComponentKind kind, Str text = {}) {
    Component component = {};
    component.kind = kind;
    component.text = text;
    return component;
}

static void SpecElementsAreSingleUseValues() {
    SpecArena arena;
    SpecId parent = arena.Push(Kind(ComponentKind::Div));
    SpecId other = arena.Push(Kind(ComponentKind::Div));
    SpecId child = arena.Push(Kind(ComponentKind::Text, StrL("hi")));
    SpecError error = {};
    utassert(arena.Attach(parent, child));
    utassert(!arena.Attach(other, child, &error));
    utassert(error.kind == SpecErrorKind::AlreadyParented);
    utassert(StrEq(error.component, "text"));
    SpecOp style = {};
    style.kind = SpecOpKind::NullaryStyle;
    style.name = StrL("flex");
    utassert(!arena.PushOp(child, style, &error));
    utassert(error.kind == SpecErrorKind::AlreadyParented);

    SpecId detached = arena.Push(Kind(ComponentKind::Div));
    utassert(arena.Claim(detached));
    utassert(arena.PushOp(detached, style));
    utassert(!arena.Attach(parent, detached, &error));
    utassert(error.kind == SpecErrorKind::Claimed);
    utassert(!arena.Claim(detached, &error));
    utassert(error.kind == SpecErrorKind::Claimed);
    utassert(!arena.Attach(parent, parent, &error));
    utassert(error.kind == SpecErrorKind::SelfParent);

    Component view = Kind(ComponentKind::ChildView);
    view.handle = 42;
    SpecId mounted = 0;
    utassert(arena.PushChildView(view, &mounted));
    utassert(!arena.PushChildView(view, nullptr, &error));
    utassert(error.kind == SpecErrorKind::DuplicateChildView);
    utassert(arena.ClaimVirtualItems(8, 10));
    utassert(!arena.ClaimVirtualItems(3, 10));

    SpecId expired = parent;
    arena.Reset();
    utassert(arena.IsEmpty());
    utassert(!arena.PushOp(expired, style, &error));
    utassert(error.kind == SpecErrorKind::Expired);
}

static void SpecsAndSnapshotsDumpWithoutEnteringTheVm() {
    SpecArena* specs = new SpecArena();
    SpecId root = specs->Push(Kind(ComponentKind::VFlex));
    SpecOp flex = {};
    flex.kind = SpecOpKind::NullaryStyle;
    flex.name = StrL("flex");
    utassert(specs->PushOp(root, flex));
    SpecId label = specs->Push(Kind(ComponentKind::Text, StrL("Save")));
    utassert(specs->Attach(root, label));
    SpecId collapsible = specs->Push(Kind(ComponentKind::Collapsible));
    SpecId body = specs->Push(Kind(ComponentKind::Text, StrL("body")));
    utassert(specs->Claim(body));
    SpecOp slot = {};
    slot.kind = SpecOpKind::Slot;
    slot.name = StrL("content");
    slot.node = body;
    utassert(specs->PushOp(collapsible, slot));
    utassert(specs->Attach(root, collapsible));

    struct LeaseState {
        int refs = 0;
        int retired = 0;
        uint64_t generation = 0;
    } lease;
    SnapshotRuntimeLease runtime = {};
    runtime.state = &lease;
    runtime.retain = [](void* state) { ((LeaseState*)state)->refs++; };
    runtime.release = [](void* state) { ((LeaseState*)state)->refs--; };
    runtime.retireCallbacks = [](void* state, uint64_t generation) {
        LeaseState* lease = (LeaseState*)state;
        lease->retired++;
        lease->generation = generation;
    };
    RenderSnapshot* snapshot = new RenderSnapshot(77, root, specs, runtime);
    utassert(snapshot->Len() == 4 && !snapshot->IsEmpty());
    utassert(lease.refs == 1 && lease.retired == 0);
    Arena* output = ArenaNew();
    Str tree = snapshot->DebugTree(output);
    utassert(StrEq(tree,
                   "v_flex .flex\n"
                   "  text \"Save\"\n"
                   "  Collapsible\n"
                   "    @content\n"
                   "      text \"body\"\n"));
    ArenaDelete(output);
    delete snapshot;
    utassert(lease.refs == 0 && lease.retired == 1);
    utassert(lease.generation == 77);
}

static void ThemeTokenNamesAndValuesComeFromTheTheme() {
    utassert(SeqStrCount(ThemeColorTokenNames()) == 17);
    utassert(SeqStrCount(ThemeSpacingTokenNames()) == 7);
    utassert(SeqStrCount(ThemeRadiusTokenNames()) == 6);
    App app;
    component::Init(&app);
    ThemeTokensSync(&app);
    float value = -1;
    utassert(ThemeTokenSpacing(StrL("md"), &value));
    utassert(value >= 0);
    utassert(!ThemeTokenSpacing(StrL("middle"), &value));
    Hsla color = {};
    utassert(ThemeTokenColor(StrL("background"), &color));
    utassert(!ThemeTokenColor(StrL("not_a_token"), &color));
    AppGlobalClear(&app);
}

static shell::CallbackId FirstCallback(const RenderSnapshot* snapshot) {
    if (!snapshot || !snapshot->Specs()) return UINT64_MAX;
    const SpecNode* root = snapshot->Specs()->Node(snapshot->Root());
    if (!root) return UINT64_MAX;
    for (SpecId childId : root->children) {
        const SpecNode* child = snapshot->Specs()->Node(childId);
        if (!child) continue;
        for (const SpecOp& op : child->ops) {
            if (op.kind == SpecOpKind::Callback) return op.callback;
        }
    }
    return UINT64_MAX;
}

static void RuntimeLoadsRendersAndRetiresCallbacks() {
    App app;
    Window window;
    window.app = &app;
    ShellError error = {};
    ShellRuntime* runtime = ShellRuntime::New(&app, &error);
    utassert(runtime != nullptr && !error.IsSet());
    if (!runtime) return;

    Str source = StrL(
        "import { View } from 'gpui';\n"
        "import { v_flex, Button } from 'gpui-base';\n"
        "export default class Main extends View {\n"
        "  render(cx) {\n"
        "    return v_flex().id('root').p(12).items_center()\n"
        "      .child('hello')\n"
        "      .child(Button.new('save')\n"
        "        .on_click((event) => { globalThis.shellClicks = event.click_count; })\n"
        "        .child('Save'));\n"
        "  }\n"
        "}\n");
    ViewType* type = runtime->LoadSource(StrL("runtime-test.js"), source,
                                         &error);
    utassert(type != nullptr && !error.IsSet());
    ViewObject* object = type
                             ? runtime->Instantiate(type, &window, &app,
                                                    nullptr, &error)
                             : nullptr;
    utassert(object != nullptr && !error.IsSet());
    RenderSnapshot* snapshot = object
                                   ? runtime->BuildSnapshot(
                                         object, &window, &app, {}, nullptr,
                                         &error)
                                   : nullptr;
    utassert(snapshot != nullptr && !error.IsSet());
    if (snapshot) {
        Arena* arena = ArenaNew();
        Str tree = snapshot->DebugTree(arena);
        utassert(StrEq(tree,
                       "v_flex :id(\"root\") .p(12) .items_center\n"
                       "  text \"hello\"\n"
                       "  Button \"save\" :on_click(fn)\n"
                       "    text \"Save\"\n"));
        ArenaDelete(arena);
        utassert(runtime->LiveCallbacks() == 1);
        shell::CallbackId callback = FirstCallback(snapshot);
        utassert(callback != UINT64_MAX);
        if (callback != UINT64_MAX) {
            ClickEvent event = {};
            event.clickCount = 3;
            runtime->DispatchClick(callback, event, &window, &app);
            utassert(runtime->Eval(
                StrL("if (globalThis.shellClicks !== 3) throw new Error('callback did not run')"),
                StrL("callback-check.js"), &error));
            utassert(!error.IsSet());
        }
        delete snapshot;
        utassert(runtime->LiveCallbacks() == 0);
    }

    ViewObjectRelease(object);
    ViewTypeRelease(type);
    runtime->Release();
    ShellErrorClear(&error);
    AppGlobalClear(&app);
}

static void RuntimeAbortsFailedSnapshotTransactions() {
    App app;
    Window window;
    window.app = &app;
    ShellError error = {};
    ShellRuntime* runtime = ShellRuntime::New(&app, &error);
    utassert(runtime != nullptr);
    if (!runtime) return;
    Str source = StrL(
        "import { View, div } from 'gpui';\n"
        "export default class Main extends View {\n"
        "  render(cx) {\n"
        "    const child = div();\n"
        "    return div().on_click(() => {}).child(child).child(child);\n"
        "  }\n"
        "}\n");
    ViewType* type = runtime->LoadSource(StrL("failed-render.js"), source,
                                         &error);
    ViewObject* object = type
                             ? runtime->Instantiate(type, &window, &app,
                                                    nullptr, &error)
                             : nullptr;
    RenderSnapshot* snapshot = object
                                   ? runtime->BuildSnapshot(
                                         object, &window, &app, {}, nullptr,
                                         &error)
                                   : nullptr;
    utassert(snapshot == nullptr);
    utassert(error.IsSet());
    utassert(StrFind(error.message, StrL("already added to a parent")) >= 0);
    utassert(StrFind(error.message, StrL("failed-render.js")) >= 0);
    utassert(runtime->LiveCallbacks() == 0);
    delete snapshot;
    ViewObjectRelease(object);
    ViewTypeRelease(type);
    runtime->Release();
    ShellErrorClear(&error);
    AppGlobalClear(&app);
}

static bool WriteTestModule(const char* name, const char* source) {
    FILE* file = fopen(name, "wb");
    if (!file) return false;
    size_t len = strlen(source);
    bool ok = fwrite(source, 1, len, file) == len;
    fclose(file);
    return ok;
}

static void ShellSourceWatchReloadsAtomically() {
    const char* mainName = "shell_watch_main.js";
    const char* notesName = "shell_watch_notes.md";
    remove(mainName);
    remove(notesName);
    utassert(WriteTestModule(
        mainName,
        "import { View, div } from 'gpui'; export default class Main extends View { render() { return div().child('old'); } }\n"));

    SourceWatcher watcher;
    ShellError error = {};
    utassert(watcher.Init(StrL("."), &error, 0));
    utassert(!error.IsSet());
    utassert(WriteTestModule(notesName, "not source\n"));
    bool changed = true;
    utassert(watcher.PollAt(1, &changed, &error));
    utassert(!changed);
    utassert(WriteTestModule(
        mainName,
        "import { View, div } from 'gpui'; export default class Main extends View { render() { return div().child('old but changed'); } }\n"));
    utassert(watcher.PollAt(2, &changed, &error));
    utassert(changed);
    utassert(watcher.PollAt(3, &changed, &error));
    utassert(!changed);

    App app;
    Window window;
    window.app = &app;
    component::Init(&app);
    ShellRuntime* runtime = ShellRuntime::New(&app, &error);
    ViewType* type = runtime ? runtime->LoadApp(StrL("."), Str(mainName), &error)
                             : nullptr;
    Entity<ScriptView> entity =
        type ? ScriptView::New(&app, runtime, type) : Entity<ScriptView>{};
    ViewTypeRelease(type);
    Arena* frame = ArenaNew();
    El* initial = entity.IsValid()
                      ? EntityRender(&app, &window, frame, entity.id)
                      : nullptr;
    utassert(initial != nullptr);
    ScriptView* view = entity.Get(&app);
    Arena* text = ArenaNew();
    utassert(view && view->snapshot);
    utassert(StrFind(view->snapshot->DebugTree(text), StrL("old but changed")) >= 0);

    utassert(WriteTestModule(mainName, "export default class { render( {\n"));
    Ctx cx = {&app, &window, frame, entity.id};
    utassert(!ScriptView::Reload(view, &cx, StrL("."), Str(mainName), &error));
    utassert(error.IsSet());
    utassert(view->snapshot != nullptr);
    text->Reset();
    utassert(StrFind(view->snapshot->DebugTree(text), StrL("old but changed")) >= 0);

    utassert(WriteTestModule(
        mainName,
        "import { View, div } from 'gpui'; export default class Main extends View { render() { return div().child('new live view'); } }\n"));
    utassert(ScriptView::Reload(view, &cx, StrL("."), Str(mainName), &error));
    utassert(!error.IsSet());
    frame->Reset();
    utassert(EntityRender(&app, &window, frame, entity.id) != nullptr);
    text->Reset();
    utassert(StrFind(view->snapshot->DebugTree(text), StrL("new live view")) >= 0);

    EntityDrop(&app, entity.id);
    if (runtime) runtime->Release();
    ArenaDelete(text);
    ArenaDelete(frame);
    ShellErrorClear(&error);
    AppGlobalClear(&app);
    remove(mainName);
    remove(notesName);
}

static void HostIncrement(HostCall* call) {
    double value = 0;
    if (!call || !call->arguments ||
        !call->arguments->Number(0, &value, &call->error))
        return;
    call->result.SetNumber(value + 1);
}

static void HostEcho(HostCall* call) {
    const HostValue* value = nullptr;
    if (!call || !call->arguments ||
        !call->arguments->Value(0, &value, &call->error))
        return;
    if (!call->result.CopyFrom(*value))
        call->error.Set(StrL("copying the host value failed"));
}

static void HostDouble(HostCall* call) {
    double value = 0;
    if (!call || !call->arguments ||
        !call->arguments->Number(0, &value, &call->error))
        return;
    call->result.SetNumber(value * 2);
}

static void ShellHostModulesBridgePlainDataAndPromises() {
    ShellClearExportedModules();
    HostError hostError;
    HostModule* reserved = HostModule::New(StrL("path"));
    utassert(!ShellExportModule(reserved, &hostError));
    utassert(hostError.IsSet());
    hostError.Clear();
    reserved->Release();

    HostModule* mismatched =
        HostModule::New(StrL("bad-types"))
            ->Function(StrL("actual"), MkFunc1Void(HostIncrement))
            ->Declarations(StrL("export function declared(): number;"));
    utassert(!ShellExportModule(mismatched, &hostError));
    utassert(StrFind(hostError.message, StrL("actual")) >= 0);
    utassert(StrFind(hostError.message, StrL("declared")) >= 0);
    hostError.Clear();
    mismatched->Release();

    HostModule* module =
        HostModule::New(StrL("calculator"))
            ->Function(StrL("increment"), MkFunc1Void(HostIncrement))
            ->Function(StrL("echo"), MkFunc1Void(HostEcho))
            ->AsyncFunction(StrL("double"), MkFunc1Void(HostDouble))
            ->Declarations(StrL(
                "export function increment(value: number): number;\n"
                "export function echo(value: unknown): unknown;\n"
                "export function double(value: number): Promise<number>;\n"));
    utassert(ShellExportModule(module, &hostError));
    utassert(!hostError.IsSet());
    module->Release();

    App app;
    Window window;
    window.app = &app;
    app.windows.Append(&window);
    component::Init(&app);
    ExecInit();
    ShellError error = {};
    ShellRuntime* runtime = ShellRuntime::New(&app, &error);
    Str source = StrL(
        "import { View, div } from 'gpui';\n"
        "import { increment, echo, double } from 'calculator';\n"
        "globalThis.hostAsync = 'pending';\n"
        "globalThis.hostSync = JSON.stringify(echo({answer:[increment(41), true, 'ok']}));\n"
        "export default class Main extends View {\n"
        "  init(props, cx) { cx.spawn(async cx => { hostAsync = String(await double(21)); cx.notify(); }); }\n"
        "  render(cx) { let live; try { live = increment(1); } catch (e) { live = 'refused:' + e.message; } return div().child(hostSync + '|' + hostAsync + '|' + live); }\n"
        "}\n");
    ViewType* type = runtime
                         ? runtime->LoadSource(StrL("host-module.js"), source,
                                               &error)
                         : nullptr;
    Entity<ScriptView> view =
        type ? ScriptView::New(&app, runtime, type) : Entity<ScriptView>{};
    ViewTypeRelease(type);
    Arena* frame = ArenaNew();
    window.frameArena = frame;
    utassert(view.IsValid() &&
             EntityRender(&app, &window, frame, view.id) != nullptr);
    utassert(!error.IsSet());
    utassert(runtime && runtime->Eval(
        StrL("if (hostSync !== '{\"answer\":[42,true,\"ok\"]}') throw new Error(hostSync)"),
        StrL("host-sync-check.js"), &error));
    utassert(ExecWaitIdle(5000));
    utassert(runtime && runtime->Eval(
        StrL("if (hostAsync !== '42') throw new Error(hostAsync)"),
        StrL("host-async-check.js"), &error));
    utassert(runtime->LiveTasks() == 0);

    ShellClearExportedModules();
    ScriptView* live = view.Get(&app);
    Ctx cx = {&app, &window, frame, view.id};
    ScriptView::Refresh(live, &cx);
    frame->Reset();
    utassert(EntityRender(&app, &window, frame, view.id) != nullptr);
    Arena* text = ArenaNew();
    utassert(live && live->snapshot &&
             StrFind(live->snapshot->DebugTree(text),
                     StrL("refused:")) >= 0);
    utassert(StrFind(live->snapshot->DebugTree(text),
                     StrL("registered none")) >= 0);

    EntityDrop(&app, view.id);
    app.windows.len = 0;
    ArenaDelete(text);
    ArenaDelete(frame);
    if (runtime) runtime->Release();
    ShellErrorClear(&error);
    AppGlobalClear(&app);
    ShellClearExportedModules();
}

static void RuntimeLoadsOnlyModulesInsideTheApplicationRoot() {
    const char* depName = "shell_runtime_dep.js";
    const char* mainName = "shell_runtime_main.js";
    const char* badName = "shell_runtime_bad.js";
    const char* outsideName = "../shell_runtime_outside.js";
    utassert(WriteTestModule(depName, "export const label = 'from dependency';\n"));
    utassert(WriteTestModule(
        mainName,
        "import { View, div } from 'gpui';\n"
        "import { label } from './shell_runtime_dep.js';\n"
        "export default class Main extends View { render(cx) { return div().child(label); } }\n"));
    utassert(WriteTestModule(outsideName, "export const escaped = true;\n"));
    utassert(WriteTestModule(
        badName,
        "import { View, div } from 'gpui';\n"
        "import { escaped } from '../shell_runtime_outside.js';\n"
        "export default class Main extends View { render(cx) { return div(); } }\n"));

    App app;
    Window window;
    window.app = &app;
    ShellError error = {};
    ShellRuntime* runtime = ShellRuntime::New(&app, &error);
    ViewType* type = runtime
                         ? runtime->LoadApp(StrL("."), Str(mainName), &error)
                         : nullptr;
    utassert(type != nullptr && !error.IsSet());
    ViewObject* object = type
                             ? runtime->Instantiate(type, &window, &app,
                                                    nullptr, &error)
                             : nullptr;
    Arena* arena = ArenaNew();
    Str tree = object ? runtime->RenderToSpec(arena, object, &window, &app,
                                              {}, nullptr, &error)
                      : Str{};
    utassert(StrEq(tree, "div\n  text \"from dependency\"\n"));

    ViewType* escaped = runtime
                            ? runtime->LoadApp(StrL("."), Str(badName), &error)
                            : nullptr;
    utassert(escaped == nullptr && error.IsSet());
    utassert(StrFind(error.message, StrL("outside the application directory")) >= 0);

    ViewTypeRelease(escaped);
    ViewObjectRelease(object);
    ViewTypeRelease(type);
    if (runtime) runtime->Release();
    ShellErrorClear(&error);
    ArenaDelete(arena);
    AppGlobalClear(&app);
    remove(depName);
    remove(mainName);
    remove(badName);
    remove(outsideName);
}

static void PublishedSnapshotsMaterializeToNativeElements() {
    App app;
    Window window;
    window.app = &app;
    component::Init(&app);
    ShellError error = {};
    ShellRuntime* runtime = ShellRuntime::New(&app, &error);
    Str source = StrL(
        "import { View } from 'gpui';\n"
        "import { v_flex, Button } from 'gpui-base';\n"
        "export default class Main extends View { render(cx) {\n"
        "  return v_flex().w(320).h(80).gap_2().p(12).bg('#123456').rounded(8)\n"
        "    .child('native')\n"
        "    .child(Button.new('disabled').disabled(true).child('No'));\n"
        "} }\n");
    ViewType* type = runtime
                         ? runtime->LoadSource(StrL("materialize.js"), source,
                                               &error)
                         : nullptr;
    ViewObject* object = type
                             ? runtime->Instantiate(type, &window, &app,
                                                    nullptr, &error)
                             : nullptr;
    RenderSnapshot* snapshot = object
                                   ? runtime->BuildSnapshot(object, &window,
                                                            &app, {}, nullptr,
                                                            &error)
                                   : nullptr;
    Arena* frame = ArenaNew();
    Ctx cx = {&app, &window, frame, {}};
    El* root = snapshot
                   ? ShellMaterialize(&cx, runtime, snapshot, &error)
                   : nullptr;
    utassert(root != nullptr && !error.IsSet());
    if (root) {
        utassert(root->style.display == Display::Flex);
        utassert(root->style.dir == FlexDir::Col);
        utassertnear(root->style.width, 320);
        utassertnear(root->style.height, 80);
        utassertnear(root->style.gapX, 8);
        utassertnear(root->style.gapY, 8);
        utassertnear(root->style.pad.top, 12);
        utassertnear(root->style.radius, 8);
        utassert(root->style.hasBg);
        utassert(abs((int)root->style.bg.color.r - 0x12) <= 1);
        utassert(abs((int)root->style.bg.color.g - 0x34) <= 1);
        utassert(abs((int)root->style.bg.color.b - 0x56) <= 1);
        utassert(root->first && root->first->kind == ElKind::Text);
        utassert(StrEq(root->first->text, "native"));
        utassert(root->first->next != nullptr);
        utassert(root->first->next->accessibility.disabled);
    }
    ArenaDelete(frame);
    delete snapshot;
    ViewObjectRelease(object);
    ViewTypeRelease(type);
    if (runtime) runtime->Release();
    ShellErrorClear(&error);
    AppGlobalClear(&app);
}

static void ShellMaterializesStateTemplatesInputsAndPaths() {
    App app;
    Window window;
    window.app = &app;
    component::Init(&app);
    ShellError error = {};
    ShellRuntime* runtime = ShellRuntime::New(&app, &error);
    Str source = StrL(
        "import { View, div, PathBuilder, Background } from 'gpui';\n"
        "import { InputState, NumberInput, OtpState, OtpInput } from 'gpui-base';\n"
        "globalThis.numberStep = '';\n"
        "export default class Main extends View {\n"
        "  init() { this.number = InputState.new({ value: '4' }); this.number.set_step(2); this.otp = OtpState.new(3, { value: '1' }); }\n"
        "  render(cx) {\n"
        "    const path = PathBuilder.fill().move_to(0, 0).line_to('100%', 0).curve_to('100%', '100%', '50%', '50%').close().build();\n"
        "    return div().children([\n"
        "      div().id('states').w(100).transition('width', { duration: 0 }).hover(s => s.bg('#112233').p(4)).active(s => s.bg('#223344')).focus(s => s.opacity(0.5)),\n"
        "      NumberInput.new(this.number).controls_right().decrement_button(div().size(10).child('-')).increment_button(div().size(10).child('+')).on_step(action => { globalThis.numberStep = action; }),\n"
        "      OtpInput.new(this.otp).cell_style(cell => cell.size(20).bg('#334455')).cell_active_style(cell => cell.border(2)),\n"
        "      window.paint_path(path, Background.linear_gradient(90, Background.stop('#000000', 0.25), '#ffffff')).w(100).h(80),\n"
        "    ]);\n"
        "  }\n"
        "}\n");
    ViewType* type = runtime
                         ? runtime->LoadSource(StrL("material-components.js"),
                                               source, &error)
                         : nullptr;
    Entity<ScriptView> view =
        type ? ScriptView::New(&app, runtime, type) : Entity<ScriptView>{};
    ViewTypeRelease(type);
    Arena* frame = ArenaNew();
    window.frameArena = frame;
    El* root = view.IsValid()
                   ? EntityRender(&app, &window, frame, view.id)
                   : nullptr;
    utassert(root != nullptr && !error.IsSet());
    El* states = root ? root->first : nullptr;
    El* number = states ? states->next : nullptr;
    El* otp = number ? number->next : nullptr;
    El* path = otp ? otp->next : nullptr;
    utassert(states && states->hoverSet & StyleFieldBg);
    utassert(states && states->hoverSet & StyleFieldPad);
    utassert(states && states->activeSet & StyleFieldBg);
    utassert(states && states->focusSet & StyleFieldOpacity);
    utassert(states && states->style.width == 100);
    utassert(number && number->accessibility.role ==
                           AccessibilityRole::SpinButton);
    El* controls = number && number->first ? number->first->next : nullptr;
    El* increment = controls ? controls->first : nullptr;
    utassert(increment && increment->onClick.IsValid());
    if (increment && increment->onClick.IsValid()) increment->onClick.Call();
    utassert(runtime && runtime->Eval(
        StrL("if (globalThis.numberStep !== 'increment') throw new Error('number step was not dispatched')"),
        StrL("number-step-check.js"), &error));
    utassert(otp && otp->first && otp->first->next &&
             otp->first->next->next);
    utassert(otp && otp->first && otp->first->style.hasBg);
    utassert(path && path->customPaint != nullptr);
    utassert(path && path->style.width == 100 && path->style.height == 80);
    EntityDrop(&app, view.id);
    ArenaDelete(frame);
    if (runtime) runtime->Release();
    ShellErrorClear(&error);
    AppGlobalClear(&app);
}

static void ScriptViewsReuseSnapshotsUntilNotified() {
    App app;
    Window window;
    window.app = &app;
    component::Init(&app);
    ShellError error = {};
    ShellRuntime* runtime = ShellRuntime::New(&app, &error);
    Str source = StrL(
        "import { View, div } from 'gpui';\n"
        "globalThis.scriptRenderCount = 0;\n"
        "export default class Main extends View { render(cx) {\n"
        "  globalThis.scriptRenderCount += 1;\n"
        "  return div().child(String(globalThis.scriptRenderCount));\n"
        "} }\n");
    ViewType* type = runtime
                         ? runtime->LoadSource(StrL("cached-view.js"), source,
                                               &error)
                         : nullptr;
    Entity<ScriptView> view = type
                                  ? ScriptView::New(&app, runtime, type)
                                  : Entity<ScriptView>{};
    ViewTypeRelease(type);
    Arena* frame = ArenaNew();
    El* first = EntityRender(&app, &window, frame, view.id);
    utassert(first != nullptr);
    frame->Reset();
    El* second = EntityRender(&app, &window, frame, view.id);
    utassert(second != nullptr);
    utassert(runtime->Eval(
        StrL("if (globalThis.scriptRenderCount !== 1) throw new Error('snapshot was rebuilt')"),
        StrL("cached-check.js"), &error));
    runtime->InvalidateScriptView(view.id);
    frame->Reset();
    El* third = EntityRender(&app, &window, frame, view.id);
    utassert(third != nullptr);
    utassert(runtime->Eval(
        StrL("if (globalThis.scriptRenderCount !== 2) throw new Error('dirty view was not rebuilt')"),
        StrL("dirty-check.js"), &error));
    EntityDrop(&app, view.id);
    ArenaDelete(frame);
    runtime->Release();
    ShellErrorClear(&error);
    AppGlobalClear(&app);
}

static void RetainedScriptStateSurvivesFramesAndDispatchesEvents() {
    App app;
    Window window;
    window.app = &app;
    component::Init(&app);
    ShellError error = {};
    ShellRuntime* runtime = ShellRuntime::New(&app, &error);
    Str source = StrL(
        "import { View } from 'gpui';\n"
        "import { v_flex, InputState, Input, SliderState, Slider, OtpState, OtpInput } from 'gpui-base';\n"
        "globalThis.retainedEvents = 0;\n"
        "export default class Main extends View {\n"
        "  init(props, cx) {\n"
        "    this.input = InputState.new({ value: 'first', placeholder: 'type' });\n"
        "    this.slider = SliderState.new({ min: 0, max: 10, step: 1, value: 3 });\n"
        "    this.otp = OtpState.new(6, { value: '12' });\n"
        "    this.input.on('change', () => { globalThis.retainedEvents += 1; this.input.set_value('second'); });\n"
        "    this.slider.on('release', () => { globalThis.retainedEvents += 10; });\n"
        "    this.otp.on('complete', () => { globalThis.retainedEvents += 100; });\n"
        "    globalThis.retainedInput = this.input;\n"
        "  }\n"
        "  render(cx) { return v_flex().children([\n"
        "    Input.new(this.input), Slider.new(this.slider), OtpInput.new(this.otp)\n"
        "  ]); }\n"
        "}\n");
    ViewType* type = runtime
                         ? runtime->LoadSource(StrL("retained-state.js"),
                                               source, &error)
                         : nullptr;
    Entity<ScriptView> view = type
                                  ? ScriptView::New(&app, runtime, type)
                                  : Entity<ScriptView>{};
    ViewTypeRelease(type);
    Arena* frame = ArenaNew();
    El* root = view.IsValid()
                   ? EntityRender(&app, &window, frame, view.id)
                   : nullptr;
    utassert(root != nullptr && !error.IsSet());
    utassert(runtime && runtime->LiveEntities() == 3);
    InputState* input = root && root->first ? root->first->input : nullptr;
    utassert(input != nullptr && StrEq(InputValue(input), "first"));
    utassert(input && input->onChange.IsValid());
    if (input) {
        InputEvent changed = {InputEventKind::Change};
        ListenerCall(&app, &window, input->onChange, &changed);
    }
    utassert(runtime->Eval(
        StrL("if (globalThis.retainedEvents !== 1) throw new Error('retained event was not dispatched')"),
        StrL("retained-event-check.js"), &error));
    utassert(input && StrEq(InputValue(input), "second"));
    utassert(runtime->Eval(StrL("globalThis.retainedInput.release()"),
                           StrL("retained-release.js"), &error));
    utassert(runtime->LiveEntities() == 2);
    EntityDrop(&app, view.id);
    utassert(runtime->LiveEntities() == 0);
    ArenaDelete(frame);
    runtime->Release();
    ShellErrorClear(&error);
    AppGlobalClear(&app);
}

static void NestedScriptViewsRetainUpdateRollbackAndRelease() {
    App app;
    Window window;
    window.app = &app;
    component::Init(&app);
    ShellError error = {};
    ShellRuntime* runtime = ShellRuntime::New(&app, &error);
    Str source = StrL(
        "import { View, div } from 'gpui';\n"
        "import { Button, InputState } from 'gpui-base';\n"
        "globalThis.nestedAction = 'none';\n"
        "globalThis.nestedNext = undefined;\n"
        "class Leaf extends View {\n"
        "  init(props) { this.label = props.label; }\n"
        "  render(cx) { globalThis.nestedLeafRendered = this.label; return div().child(this.label); }\n"
        "}\n"
        "class Child extends View {\n"
        "  init(props, cx) {\n"
        "    this.state = { label: props.label };\n"
        "    this.leaf = cx.new(Leaf, { label: 'leaf' });\n"
        "  }\n"
        "  update(props) {\n"
        "    if (props.append) this.state.label += props.append;\n"
        "    else this.state.label = props.label;\n"
        "    if (props.fail) {\n"
        "      this.temporary = InputState.new({ value: 'temporary' });\n"
        "      throw new Error('nested update failed');\n"
        "    }\n"
        "  }\n"
        "  render(cx) {\n"
        "    globalThis.nestedRendered = this.state.label;\n"
        "    return div().children([this.state.label, this.leaf]);\n"
        "  }\n"
        "}\n"
        "export default class Main extends View {\n"
        "  init(props, cx) { this.child = cx.new(Child, { label: 'one' }); }\n"
        "  render(cx) {\n"
        "    return div().children([\n"
        "      Button.new('nested-action').on_click(() => {\n"
        "        if (globalThis.nestedAction === 'release')\n"
        "          globalThis.nestedReleased = this.child.release();\n"
        "        else this.child.set_props(globalThis.nestedNext);\n"
        "      }),\n"
        "      this.child,\n"
        "    ]);\n"
        "  }\n"
        "}\n");
    ViewType* type = runtime
                         ? runtime->LoadSource(StrL("nested-view.js"), source,
                                               &error)
                         : nullptr;
    Entity<ScriptView> parent =
        type ? ScriptView::New(&app, runtime, type) : Entity<ScriptView>{};
    ViewTypeRelease(type);
    Arena* frame = ArenaNew();
    window.frameArena = frame;
    El* root = parent.IsValid()
                   ? EntityRender(&app, &window, frame, parent.id)
                   : nullptr;
    ScriptView* parentView = parent.Get(&app);
    shell::CallbackId callback =
        parentView ? FirstCallback(parentView->snapshot) : UINT64_MAX;
    utassert(root != nullptr && !error.IsSet());
    utassert(runtime && runtime->LiveNestedViews() == 2);
    utassert(callback != UINT64_MAX);
    utassert(runtime && runtime->Eval(
        StrL("if (globalThis.nestedRendered !== 'one' || globalThis.nestedLeafRendered !== 'leaf') throw new Error('nested init props were not rendered')"),
        StrL("nested-init-check.js"), &error));

    utassert(runtime && runtime->Eval(
        StrL("globalThis.nestedNext = { label: 'two' }"),
        StrL("nested-update-input.js"), &error));
    if (callback != UINT64_MAX) {
        ClickEvent click = {};
        runtime->DispatchClick(callback, click, &window, &app);
    }
    ArenaDelete(frame);
    frame = ArenaNew();
    window.frameArena = frame;
    root = EntityRender(&app, &window, frame, parent.id);
    utassert(root != nullptr);
    utassert(runtime && runtime->Eval(
        StrL("if (globalThis.nestedRendered !== 'two') throw new Error('set_props did not rebuild only the child')"),
        StrL("nested-update-check.js"), &error));

    utassert(runtime && runtime->Eval(
        StrL("globalThis.nestedNext = { label: 'bad', fail: true }"),
        StrL("nested-failure-input.js"), &error));
    if (callback != UINT64_MAX) {
        ClickEvent click = {};
        runtime->DispatchClick(callback, click, &window, &app);
    }
    utassert(runtime && runtime->LiveEntities() == 0);
    utassert(runtime && runtime->LiveNestedViews() == 2);
    utassert(runtime && runtime->Eval(
        StrL("globalThis.nestedNext = { append: '!' }"),
        StrL("nested-rollback-probe.js"), &error));
    if (callback != UINT64_MAX) {
        ClickEvent click = {};
        runtime->DispatchClick(callback, click, &window, &app);
    }
    ArenaDelete(frame);
    frame = ArenaNew();
    window.frameArena = frame;
    root = EntityRender(&app, &window, frame, parent.id);
    utassert(root != nullptr);
    utassert(runtime && runtime->Eval(
        StrL("if (globalThis.nestedRendered !== 'two!') throw new Error('failed update state was not restored')"),
        StrL("nested-rollback-check.js"), &error));

    utassert(runtime && runtime->Eval(
        StrL("globalThis.nestedAction = 'release'"),
        StrL("nested-release-input.js"), &error));
    if (callback != UINT64_MAX) {
        ClickEvent click = {};
        runtime->DispatchClick(callback, click, &window, &app);
    }
    utassert(runtime && runtime->LiveNestedViews() == 0);
    utassert(runtime && runtime->Eval(
        StrL("if (globalThis.nestedReleased !== true) throw new Error('release failed')"),
        StrL("nested-release-check.js"), &error));
    EntityDrop(&app, parent.id);
    ArenaDelete(frame);
    runtime->Release();
    ShellErrorClear(&error);
    AppGlobalClear(&app);
}

static void VirtualListsRenderOneVisibleBatch() {
    App app;
    Window window;
    window.app = &app;
    component::Init(&app);
    ShellError error = {};
    ShellRuntime* runtime = ShellRuntime::New(&app, &error);
    Str source = StrL(
        "import { View, div } from 'gpui';\n"
        "import { v_virtual_list, VirtualListScrollHandle } from 'gpui-base';\n"
        "globalThis.virtualBatches = 0; globalThis.virtualClick = '';\n"
        "export default class Main extends View {\n"
        "  init(props, cx) { this.scroll = VirtualListScrollHandle.new(); }\n"
        "  render(cx) { return v_virtual_list('rows', 20, 24,\n"
        "    index => 'row-' + index,\n"
        "    range => { globalThis.virtualBatches += 1; const out = [];\n"
        "      for (let i = range.start; i < range.end; i++) out.push(div().child('row ' + i));\n"
        "      return out;\n"
        "    }).track_scroll(this.scroll).on_item_click(key => { globalThis.virtualClick = key; }); }\n"
        "}\n");
    ViewType* type = runtime
                         ? runtime->LoadSource(StrL("virtual-list.js"), source,
                                               &error)
                         : nullptr;
    Entity<ScriptView> view = type
                                  ? ScriptView::New(&app, runtime, type)
                                  : Entity<ScriptView>{};
    ViewTypeRelease(type);
    Arena* frame = ArenaNew();
    El* root = view.IsValid()
                   ? EntityRender(&app, &window, frame, view.id)
                   : nullptr;
    utassert(root != nullptr && !error.IsSet());
    utassert(runtime->Eval(
        StrL("if (globalThis.virtualBatches !== 1) throw new Error('virtual list did not render one range')"),
        StrL("virtual-list-check.js"), &error));
    El* firstRow = root && root->first ? root->first->first : nullptr;
    utassert(firstRow && firstRow->listener.IsValid());
    if (firstRow && firstRow->listener.IsValid()) {
        ClickEvent click = {};
        ListenerCall(&app, &window, firstRow->listener, &click);
    }
    utassert(runtime->Eval(
        StrL("if (globalThis.virtualClick !== 'row-0') throw new Error('virtual item key was not dispatched')"),
        StrL("virtual-list-click-check.js"), &error));
    utassert(runtime->LiveEntities() == 1);
    EntityDrop(&app, view.id);
    utassert(runtime->LiveEntities() == 0);
    ArenaDelete(frame);
    runtime->Release();
    ShellErrorClear(&error);
    AppGlobalClear(&app);
}

static void ShellSandboxWithholdsCompilersAndSharedPrototypeWrites() {
    ShellError error = {};
    ShellSetDevelopmentMode(false);
    ShellRuntime* runtime = ShellRuntime::New(nullptr, &error);
    utassert(runtime != nullptr && !error.IsSet());
    utassert(runtime && runtime->Eval(
        StrL("if (typeof eval !== 'undefined' || !Object.isFrozen(Object.prototype) || "
             "typeof std !== 'undefined') throw new Error('sandbox surface is open')"),
        StrL("sandbox-check.js"), &error));
    utassert(runtime && !runtime->Eval(
        StrL("new Function('return 1')()"), StrL("sandbox-function.js"),
        &error));
    utassert(error.IsSet() && StrContains(error.message, StrL("disabled")));
    ShellErrorClear(&error);
    if (runtime) runtime->Release();

    ShellSetDevelopmentMode(true);
    runtime = ShellRuntime::New(nullptr, &error);
    utassert(runtime && runtime->Eval(
        StrL("if (eval('1 + 1') !== 2 || Function('return 3')() !== 3) "
             "throw new Error('development compiler missing')"),
        StrL("development-mode.js"), &error));
    if (runtime) runtime->Release();
    ShellSetDevelopmentMode(false);
    ShellErrorClear(&error);
}

static void ShellSchedulerResumesPromisesInTaskScope() {
    App app;
    Window window;
    window.app = &app;
    component::Init(&app);
    ShellError error = {};
    ShellRuntime* runtime = ShellRuntime::New(&app, &error);
    Str source = StrL(
        "import { View, div } from 'gpui';\n"
        "globalThis.taskEvents = 0;\n"
        "export default class Main extends View {\n"
        "  init(props, cx) {\n"
        "    this.once = cx.timer.after(1, cx => { taskEvents += 1; cx.notify(); });\n"
        "    globalThis.every = cx.timer.every(1, () => { taskEvents += 10; });\n"
        "    cx.sleep(1).then(() => { taskEvents += 100; });\n"
        "    cx.spawn(async cx => { await cx.sleep(1); taskEvents += 1000; cx.notify(); });\n"
        "  }\n"
        "  render(cx) { return div().child(String(taskEvents)); }\n"
        "}\n");
    ViewType* type = runtime
                         ? runtime->LoadSource(StrL("scheduler.js"), source,
                                               &error)
                         : nullptr;
    Entity<ScriptView> view = type
                                  ? ScriptView::New(&app, runtime, type)
                                  : Entity<ScriptView>{};
    ViewTypeRelease(type);
    Arena* frame = ArenaNew();
    window.frameArena = frame;
    El* root = view.IsValid()
                   ? EntityRender(&app, &window, frame, view.id)
                   : nullptr;
    utassert(root != nullptr && !error.IsSet());
    utassert(runtime && runtime->LiveTasks() == 5);
    for (int i = 0; i < window.timers.len; i++) window.timers[i].dueAt = 0;
    WindowTimerTick(&window);
    utassert(runtime->Eval(
        StrL("if (taskEvents !== 1111) throw new Error('task resumptions were not drained')"),
        StrL("scheduler-result.js"), &error));
    utassert(runtime->LiveTasks() == 1);
    utassert(runtime->Eval(StrL("globalThis.every.cancel()"),
                           StrL("scheduler-cancel.js"), &error));
    utassert(runtime->LiveTasks() == 0);
    EntityDrop(&app, view.id);
    window.timers.Reset();
    ArenaDelete(frame);
    runtime->Release();
    ShellErrorClear(&error);
    AppGlobalClear(&app);
}

static void ShellStorageAndAuthorityFreeModulesWork() {
    const char* storagePath = "shell_storage_test.json";
    remove(storagePath);
    Capabilities granted;
    granted.Storage(true);
    PolicyUpdateDefaultCapabilities(granted);
    Str storageError;
    utassert(ShellSetStoragePath(Str(storagePath), &storageError));
    utassert(!storageError);
    ExecInit();

    ShellError error = {};
    ShellRuntime* runtime = ShellRuntime::New(nullptr, &error);
    utassert(runtime != nullptr && !error.IsSet());
    utassert(runtime && runtime->Eval(
        StrL("sessionStorage.setItem('temporary', 'yes');"
             "localStorage.setItem('theme', 'dark');"
             "if (sessionStorage.getItem('temporary') !== 'yes' || "
             "localStorage.getItem('theme') !== 'dark' || localStorage.length !== 1) "
             "throw new Error('storage did not round trip')"),
        StrL("storage.js"), &error));
    utassert(ExecWaitIdle(5000));

    Str source = StrL(
        "import { View, div } from 'gpui';\n"
        "import { Buffer } from 'buffer';\n"
        "import path from 'path';\n"
        "import { URL } from 'url';\n"
        "import os from 'os';\n"
        "if (Buffer.from('hello').toString('hex') !== '68656c6c6f') throw new Error('buffer');\n"
        "if (path.basename(path.join('a', 'b.txt')) !== 'b.txt') throw new Error('path');\n"
        "if (new URL('https://example.com/a?q=1').searchParams.get('q') !== '1') throw new Error('url');\n"
        "if (typeof os.platform() !== 'string' || !os.EOL) throw new Error('os');\n"
        "export default class Main extends View { render(cx) { return div().child('standard'); } }\n");
    ViewType* type = runtime
                         ? runtime->LoadSource(StrL("standard-modules.js"),
                                               source, &error)
                         : nullptr;
    utassert(type != nullptr && !error.IsSet());
    ViewTypeRelease(type);
    if (runtime) runtime->Release();
    ShellErrorClear(&error);
    StrFree(storageError);

    FILE* stored = fopen(storagePath, "rb");
    utassert(stored != nullptr);
    if (stored) fclose(stored);
    remove(storagePath);
    Capabilities denied;
    PolicyUpdateDefaultCapabilities(denied);
}

struct StorageTestSettlement {
    int calls = 0;
    bool ok = false;
};

static void RecordStorageSettlement(StorageTestSettlement* state,
                                    StorageOutcome outcome) {
    state->calls++;
    state->ok = outcome.ok;
}

static void SettleStorageTestWaiters(Vec<StorageWaiter*>* ready,
                                     StorageOutcome outcome) {
    for (int i = 0; i < ready->len; i++) {
        StorageWaiter* waiter = (*ready)[i];
        Func1<StorageOutcome> settle = waiter->settle;
        delete waiter;
        settle.Call(outcome);
    }
    ready->Reset();
}

static void ShellStorageWritesRevisionsInOrderAndFlushes() {
    remove("shell_storage_queue_test.json");
    Storage storage(true);
    Str storageError;
    utassert(storage.SetPath(StrL("shell_storage_queue_test.json"),
                             &storageError));
    utassert(storage.Set(StrL("revision"), StrL("one"), &storageError));
    StorageWrite first;
    utassert(storage.BeginWrite(&first, &storageError));
    utassert(first.revision == 1 && storage.HasWriteInFlight() &&
             StrContains(first.body, StrL("one")));
    utassert(storage.Set(StrL("revision"), StrL("two"), &storageError));

    StorageTestSettlement settlement;
    StorageWaiter* waiter = nullptr;
    bool immediate = false;
    utassert(storage.Wait(MkFunc1(RecordStorageSettlement, &settlement),
                          &waiter, &immediate, &storageError));
    utassert(waiter != nullptr && !immediate && settlement.calls == 0);
    Vec<StorageWaiter*> ready;
    storage.FinishWrite(first.revision, true, &ready);
    utassert(ready.len == 0 && storage.IsDirty());
    first.Free();

    StorageWrite second;
    utassert(storage.BeginWrite(&second, &storageError));
    utassert(second.revision == 2 && StrContains(second.body, StrL("two")));
    storage.FinishWrite(second.revision, true, &ready);
    SettleStorageTestWaiters(&ready, StorageOutcome{true, {}});
    utassert(settlement.calls == 1 && settlement.ok && !storage.IsDirty());
    second.Free();

    StorageWaiter* already = nullptr;
    immediate = false;
    utassert(storage.Wait(MkFunc1(RecordStorageSettlement, &settlement),
                          &already, &immediate, &storageError));
    utassert(immediate && already == nullptr);

    utassert(storage.Set(StrL("revision"), StrL("three"), &storageError));
    StorageWrite failed;
    utassert(storage.BeginWrite(&failed, &storageError));
    StorageTestSettlement failedSettlement;
    utassert(storage.Wait(MkFunc1(RecordStorageSettlement, &failedSettlement),
                          &waiter, &immediate, &storageError));
    utassert(!immediate && waiter != nullptr);
    storage.FinishWrite(failed.revision, false, &ready);
    SettleStorageTestWaiters(&ready,
                             StorageOutcome{false, StrL("disk full")});
    utassert(failedSettlement.calls == 1 && !failedSettlement.ok);
    failed.Free();
    StorageWrite parked;
    utassert(storage.BeginWrite(&parked, &storageError));
    utassert(parked.revision == 0 && storage.IsDirty());
    StorageTestSettlement retrySettlement;
    utassert(storage.Wait(MkFunc1(RecordStorageSettlement, &retrySettlement),
                          &waiter, &immediate, &storageError));
    utassert(!immediate && waiter != nullptr);
    utassert(storage.BeginWrite(&parked, &storageError));
    utassert(parked.revision == 3);
    storage.FinishWrite(parked.revision, true, &ready);
    SettleStorageTestWaiters(&ready, StorageOutcome{true, {}});
    utassert(retrySettlement.calls == 1 && retrySettlement.ok &&
             !storage.IsDirty());
    parked.Free();
    StrFree(storageError);

    const char* path = "shell_storage_flush_test.json";
    remove(path);
    Capabilities granted;
    granted.Storage(true);
    PolicyUpdateDefaultCapabilities(granted);
    utassert(ShellSetStoragePath(Str(path), &storageError));
    App app;
    Window window;
    window.app = &app;
    app.windows.Append(&window);
    component::Init(&app);
    ExecInit();
    ShellError error = {};
    ShellRuntime* runtime = ShellRuntime::New(&app, &error);
    Str source = StrL(
        "import { View, div } from 'gpui';\n"
        "globalThis.storageFlush = 'pending';\n"
        "export default class Main extends View {\n"
        "  init(props, cx) { cx.spawn(async cx => { localStorage.setItem('revision', 'final'); await localStorage.flush(); storageFlush = 'flushed'; cx.notify(); }); }\n"
        "  render(cx) { return div().child(storageFlush); }\n"
        "}\n");
    ViewType* type = runtime
                         ? runtime->LoadSource(StrL("storage-flush.js"),
                                               source, &error)
                         : nullptr;
    Entity<ScriptView> view = type
                                  ? ScriptView::New(&app, runtime, type)
                                  : Entity<ScriptView>{};
    ViewTypeRelease(type);
    Arena* frame = ArenaNew();
    window.frameArena = frame;
    El* root = view.IsValid()
                   ? EntityRender(&app, &window, frame, view.id)
                   : nullptr;
    utassert(root != nullptr && !error.IsSet());
    utassert(ExecWaitIdle(5000));
    utassert(runtime && runtime->Eval(
        StrL("if (storageFlush !== 'flushed') throw new Error(storageFlush)"),
        StrL("storage-flush-result.js"), &error));
    utassert(runtime && runtime->LiveTasks() == 0);
    FILE* file = fopen(path, "rb");
    utassert(file != nullptr);
    if (file) fclose(file);
    EntityDrop(&app, view.id);
    app.windows.len = 0;
    ArenaDelete(frame);
    runtime->Release();
    ShellErrorClear(&error);
    AppGlobalClear(&app);
    remove(path);
    StrFree(storageError);
    Capabilities denied;
    PolicyUpdateDefaultCapabilities(denied);
}

static void ShellProcessRunIsBoundedAndPromiseBased() {
#if GPUI_OS_WINDOWS
    Str args[] = {StrL("/D"), StrL("/C"),
                  StrL("echo out & echo err 1>&2 & exit /B 7")};
    ProcessCancellation cancellation;
    ProcessOutput output;
    Str processError;
    utassert(ProcessRunBounded(StrL("cmd.exe"), args, 3, &cancellation,
                               &output, &processError));
    utassert(!processError && output.code == 7);
    utassert(StrEq(StrTrimAscii(output.out), "out") &&
             StrEq(StrTrimAscii(output.err), "err"));
    output.Free();
    StrFree(processError);

    App app;
    Window window;
    window.app = &app;
    app.windows.Append(&window);
    component::Init(&app);
    ExecInit();
    Str commands[] = {StrL("cmd.exe")};
    ExecuteGrant execute = ExecuteGrant::Allowed(commands, 1);
    Capabilities granted;
    granted.SetExecute(execute);
    PolicyUpdateDefaultCapabilities(granted);
    ShellError error = {};
    ShellRuntime* runtime = ShellRuntime::New(&app, &error);
    Str source = StrL(
        "import { View, div } from 'gpui';\n"
        "import process from 'process';\n"
        "globalThis.processResult = 'pending';\n"
        "export default class Main extends View {\n"
        "  init(props, cx) { cx.spawn(async cx => {\n"
        "    const result = await process.run('cmd.exe', ['/D', '/C', 'echo jsout & echo jserr 1>&2 & exit /B 9']);\n"
        "    processResult = `${result.code}|${result.stdout.trim()}|${result.stderr.trim()}`; cx.notify();\n"
        "  }); }\n"
        "  render(cx) { return div().child(processResult); }\n"
        "}\n");
    ViewType* type = runtime
                         ? runtime->LoadSource(StrL("process-run.js"), source,
                                               &error)
                         : nullptr;
    Entity<ScriptView> view = type
                                  ? ScriptView::New(&app, runtime, type)
                                  : Entity<ScriptView>{};
    ViewTypeRelease(type);
    Arena* frame = ArenaNew();
    window.frameArena = frame;
    El* root = view.IsValid()
                   ? EntityRender(&app, &window, frame, view.id)
                   : nullptr;
    utassert(root != nullptr && !error.IsSet());
    utassert(ExecWaitIdle(5000));
    utassert(runtime && runtime->Eval(
        StrL("if (processResult !== '9|jsout|jserr') throw new Error(processResult)"),
        StrL("process-result.js"), &error));
    utassert(runtime && runtime->LiveTasks() == 0);
    EntityDrop(&app, view.id);
    app.windows.len = 0;
    ArenaDelete(frame);
    runtime->Release();
    ShellErrorClear(&error);
    AppGlobalClear(&app);
    Capabilities denied;
    PolicyUpdateDefaultCapabilities(denied);
#endif
}

static void ShellFilesystemUsesGrantedHandleRelativePaths() {
    const char* rootName = "shell_fs_test_root";
#if GPUI_OS_WINDOWS
    RemoveDirectoryA(rootName);
#else
    rmdir(rootName);
#endif
    FsResult result;
    Str fsError;
    utassert(FsRun(FsOperation::MakeDirectory, Str(rootName),
                   StrL("nested/child"), {}, true, &result, &fsError));
    utassert(FsRun(FsOperation::Write, Str(rootName),
                   StrL("nested/child/note.txt"), StrL("hello"), false,
                   &result, &fsError));
    utassert(FsRun(FsOperation::Read, Str(rootName),
                   StrL("nested/child/note.txt"), {}, false, &result,
                   &fsError));
    utassert(StrEq(result.bytes, "hello"));
    utassert(FsRun(FsOperation::ReadDirectory, Str(rootName),
                   StrL("nested/child"), {}, false, &result, &fsError));
    utassert(result.entries.len == 1 &&
             StrEq(result.entries[0].name, "note.txt") &&
             !result.entries[0].isDirectory);
    utassert(FsRun(FsOperation::Exists, Str(rootName),
                   StrL("nested/child/note.txt"), {}, false, &result,
                   &fsError) && result.exists);
    utassert(FsRun(FsOperation::RemoveFile, Str(rootName),
                   StrL("nested/child/note.txt"), {}, false, &result,
                   &fsError));
    utassert(FsRun(FsOperation::RemoveDirectory, Str(rootName),
                   StrL("nested/child"), {}, false, &result, &fsError));
    utassert(FsRun(FsOperation::RemoveDirectory, Str(rootName), StrL("nested"),
                   {}, false, &result, &fsError));
    result.Free();
    StrFree(fsError);
#if GPUI_OS_WINDOWS
    utassert(RemoveDirectoryA(rootName) != 0);
#else
    utassert(rmdir(rootName) == 0);
#endif

    const char* jsRoot = "shell_fs_js_test_root";
#if GPUI_OS_WINDOWS
    RemoveDirectoryA(jsRoot);
#else
    rmdir(jsRoot);
#endif
    Capabilities granted;
    granted.AddReadRoot(Str(jsRoot)).AddWriteRoot(Str(jsRoot));
    PolicyUpdateDefaultCapabilities(granted);
    App app;
    Window window;
    window.app = &app;
    app.windows.Append(&window);
    component::Init(&app);
    ExecInit();
    ShellError error = {};
    ShellRuntime* runtime = ShellRuntime::New(&app, &error);
    Str source = StrL(
        "import { View, div } from 'gpui';\n"
        "import fs from 'fs/promises';\n"
        "globalThis.fsResult = 'pending';\n"
        "export default class Main extends View {\n"
        "  init(props, cx) { cx.spawn(async cx => {\n"
        "    await fs.mkdir('nested', { recursive: true });\n"
        "    await fs.writeFile('nested/note.txt', 'hello');\n"
        "    const text = await fs.readFile('nested/note.txt', 'utf8');\n"
        "    const entries = await fs.readdir('nested', { withFileTypes: true });\n"
        "    const exists = await fs.exists('nested/note.txt');\n"
        "    fsResult = `${text}|${entries[0].name}|${entries[0].isDirectory()}|${exists}`;\n"
        "    await fs.unlink('nested/note.txt'); await fs.rmdir('nested'); cx.notify();\n"
        "  }); }\n"
        "  render(cx) { return div().child(fsResult); }\n"
        "}\n");
    ViewType* type = runtime
                         ? runtime->LoadSource(StrL("filesystem.js"), source,
                                               &error)
                         : nullptr;
    Entity<ScriptView> view = type
                                  ? ScriptView::New(&app, runtime, type)
                                  : Entity<ScriptView>{};
    ViewTypeRelease(type);
    Arena* frame = ArenaNew();
    window.frameArena = frame;
    El* root = view.IsValid()
                   ? EntityRender(&app, &window, frame, view.id)
                   : nullptr;
    utassert(root != nullptr && !error.IsSet());
    utassert(ExecWaitIdle(10000));
    utassert(runtime && runtime->Eval(
        StrL("if (fsResult !== 'hello|note.txt|false|true') throw new Error(fsResult)"),
        StrL("filesystem-result.js"), &error));
    utassert(runtime && runtime->LiveTasks() == 0);
    EntityDrop(&app, view.id);
    app.windows.len = 0;
    ArenaDelete(frame);
    runtime->Release();
    ShellErrorClear(&error);
    AppGlobalClear(&app);
#if GPUI_OS_WINDOWS
    utassert(RemoveDirectoryA(jsRoot) != 0);
#else
    utassert(rmdir(jsRoot) == 0);
#endif
    Capabilities denied;
    PolicyUpdateDefaultCapabilities(denied);
}

static void ShellCryptoAndCompressionMatchStandardRuntime() {
    static const uint8_t expected[32] = {
        0xce, 0x63, 0x5c, 0x4e, 0xab, 0xff, 0x5e, 0x4f,
        0x56, 0xdb, 0xa8, 0xfb, 0x1e, 0x39, 0xca, 0x23,
        0x55, 0x30, 0xaa, 0x2b, 0x6b, 0x18, 0x53, 0x3e,
        0xef, 0x1a, 0xf3, 0x86, 0x20, 0x16, 0xc5, 0x77,
    };
    uint8_t digest[32];
    Sha256(StrL("shell"), digest);
    utassert(memcmp(digest, expected, sizeof(expected)) == 0);

    for (int gzip = 0; gzip < 2; gzip++) {
        Str compressed;
        Str inflated;
        Str compressionError;
        utassert(ZlibDeflate(StrL("stored compression round trip"),
                             gzip != 0, &compressed, &compressionError));
        utassert(!compressionError && compressed.len > 0);
        utassert(ZlibInflate(compressed, gzip != 0, &inflated,
                             &compressionError));
        utassert(!compressionError &&
                 StrEq(inflated, "stored compression round trip"));
        compressed.s[compressed.len - 1] ^= 1;
        utassert(!ZlibInflate(compressed, gzip != 0, &inflated,
                              &compressionError));
        utassert(compressionError);
        StrFree(compressionError);
        StrFree(inflated);
        StrFree(compressed);
    }

    ShellError error = {};
    ShellRuntime* runtime = ShellRuntime::New(nullptr, &error);
    Str source = StrL(
        "import { View, div } from 'gpui';\n"
        "import { Buffer } from 'buffer';\n"
        "import { createHash, randomBytes, randomUUID, webcrypto } from 'crypto';\n"
        "import { deflateSync, inflateSync, gzipSync, gunzipSync } from 'zlib';\n"
        "const input = Buffer.from('shell', 'utf8');\n"
        "if (inflateSync(deflateSync(input)).toString() !== 'shell') throw new Error('deflate');\n"
        "if (gunzipSync(gzipSync(input)).toString() !== 'shell') throw new Error('gzip');\n"
        "if (createHash('sha256').update(input).digest('hex') !== 'ce635c4eabff5e4f56dba8fb1e39ca235530aa2b6b18533eef1af3862016c577') throw new Error('sha256');\n"
        "if (Buffer.from(await webcrypto.subtle.digest('SHA-256', input)).toString('hex') !== 'ce635c4eabff5e4f56dba8fb1e39ca235530aa2b6b18533eef1af3862016c577') throw new Error('subtle.digest');\n"
        "if (inflateSync(Buffer.from('7801cb48cdc9c957c8402701680308b1', 'hex')).toString() !== 'hello hello hello hello') throw new Error('fixed Huffman');\n"
        "const words = ['alpha','bravo','charlie','delta','echo','foxtrot','golf','hotel','india','juliet','kilo','lima','mike','november','oscar','papa','quebec','romeo','sierra','tango','uniform','victor','whiskey','xray','yankee','zulu'];\n"
        "let seed = 1, text = ''; for (let i = 0; i < 1000; i++) { seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0; text += words[seed % words.length] + ' '; } text = text.slice(0, 1000);\n"
        "const dynamic = Buffer.from('789c6d526d76843008bc0a57635d5c53a3d818eddad3f795011b7dfd978f61981998d2283468954c6b9252983eb69ca4523770c949e85df820e906c5e9e07914a19c2626cecbc0f4bde58dbe86b48e72d09ebaaa2550bdbe6bd11ad75977991e52e8a5b90fe836a75ecb4495e797a211e404e5c20b539a9fc95bb9cce6d9c404fc5178d700798fcf4d1ed20137fd1a0e6161d27171f5080cfa945cbdaae824da1e43bb119b29a021ebb43ba61ca674edb8c087bd426d680f5940c5cd320110895bbbd0fa7fcd657a21131c1e8669009fbb3703a7687c612a4110ec4e716f76d1854ae36cb523a030ec41fb7ed848a357933bea83b89982b9b31ced84d82fcb7cddc76a9a5c3d70f7d5345e9785488dba19ae1dcdaad7de01defa9eced9c2ffadeccf8693c1dd75996d01cef208c806d8ca85fd7b5b9fe00f0fa876ce', 'hex');\n"
        "if (inflateSync(dynamic).toString() !== text) throw new Error('dynamic Huffman');\n"
        "const random = randomBytes(32); if (random.length !== 32) throw new Error('randomBytes');\n"
        "const values = new Uint32Array(4); if (webcrypto.getRandomValues(values) !== values) throw new Error('getRandomValues');\n"
        "if (!/^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/.test(randomUUID())) throw new Error('randomUUID');\n"
        "export default class Main extends View { render(cx) { return div().child('standard'); } }\n");
    ViewType* type = runtime
                         ? runtime->LoadSource(StrL("standard-runtime.js"),
                                               source, &error)
                         : nullptr;
    utassert(type != nullptr && !error.IsSet());
    ViewTypeRelease(type);
    if (runtime) runtime->Release();
    ShellErrorClear(&error);
}

static int gShellFetchCalls = 0;
static int gShellFetchMode = 0;

static bool ShellFetchFixture(Str url, HttpRsp* out) {
    gShellFetchCalls++;
    if (gShellFetchMode == 1 && StrEq(url, "https://api.example.test/start")) {
        out->status = 302;
        out->redirectUrl = StrDup(StrL("https://cdn.example.test/result"));
        return true;
    }
    if (gShellFetchMode == 2 && StrEq(url, "https://api.example.test/start")) {
        out->status = 302;
        out->redirectUrl = StrDup(StrL("http://api.example.test/result"));
        return true;
    }
    if (StrEq(url, "https://cdn.example.test/result")) {
        out->status = 201;
        const char* body = "redirected";
        memcpy(out->body.AppendBlanks(10), body, 10);
        return true;
    }
    if (StrEq(url, "https://api.example.test/data")) {
        out->status = 200;
        const char* body = "{\"answer\":42}";
        memcpy(out->body.AppendBlanks(13), body, 13);
        return true;
    }
    return false;
}

static void ShellFetchChecksEveryGetTargetBeforeContact() {
    HttpRequestGrant exact(StrL("api.example.test"));
    exact.AddMethod(StrL("GET")).AddPath(StrL("/v1/quote"));
    Capabilities scoped;
    scoped.AddHttpRequest(exact);
    Str fetchError;
    utassert(FetchAuthorizeGet(
        StrL("https://api.example.test/v1/quote?currency=usd"), scoped,
        &fetchError));
    utassert(!fetchError);
    utassert(!FetchAuthorizeGet(StrL("https://api.example.test/v1/private"),
                                scoped, &fetchError));
    utassert(fetchError);
    StrFree(fetchError);

    Capabilities both;
    both.AddNetworkHost(StrL("api.example.test"))
        .AddNetworkHost(StrL("cdn.example.test"));
    FetchSetHttpGetForTests(ShellFetchFixture);
    gShellFetchCalls = 0;
    gShellFetchMode = 1;
    FetchResult result;
    utassert(FetchGet(StrL("https://api.example.test/start"), both, &result));
    utassert(!result.error && result.status == 201 &&
             StrEq(result.url, "https://cdn.example.test/result") &&
             StrEq(result.body, "redirected"));
    utassert(gShellFetchCalls == 2);
    result.Free();

    Capabilities initialOnly;
    initialOnly.AddNetworkHost(StrL("api.example.test"));
    gShellFetchCalls = 0;
    gShellFetchMode = 1;
    utassert(!FetchGet(StrL("https://api.example.test/start"), initialOnly,
                       &result));
    utassert(result.error && gShellFetchCalls == 1);
    result.Free();

    gShellFetchCalls = 0;
    gShellFetchMode = 2;
    utassert(!FetchGet(StrL("https://api.example.test/start"), initialOnly,
                       &result));
    utassert(result.error &&
             StrContains(result.error, StrL("HTTPS downgrade")) &&
             gShellFetchCalls == 1);
    result.Free();

    App app;
    Window window;
    window.app = &app;
    app.windows.Append(&window);
    component::Init(&app);
    ExecInit();
    PolicyUpdateDefaultCapabilities(initialOnly);
    gShellFetchMode = 0;
    gShellFetchCalls = 0;
    ShellError error = {};
    ShellRuntime* runtime = ShellRuntime::New(&app, &error);
    Str source = StrL(
        "import { View, div } from 'gpui';\n"
        "globalThis.fetchResult = 'pending';\n"
        "let postRefused = false; try { fetch('https://api.example.test/data', { method: 'POST' }); } catch (error) { postRefused = error.message.includes('GET only'); }\n"
        "if (!postRefused) throw new Error('POST was not refused');\n"
        "export default class Main extends View {\n"
        "  init(props, cx) { cx.spawn(async cx => {\n"
        "    const response = await fetch('https://api.example.test/data');\n"
        "    const text = response.text(), json = response.json();\n"
        "    fetchResult = `${response.status}|${response.ok}|${response.url}|${text instanceof Promise}|${await text}|${(await json).answer}`; cx.notify();\n"
        "  }); }\n"
        "  render(cx) { return div().child(fetchResult); }\n"
        "}\n");
    ViewType* type = runtime
                         ? runtime->LoadSource(StrL("fetch.js"), source,
                                               &error)
                         : nullptr;
    Entity<ScriptView> view = type
                                  ? ScriptView::New(&app, runtime, type)
                                  : Entity<ScriptView>{};
    ViewTypeRelease(type);
    Arena* frame = ArenaNew();
    window.frameArena = frame;
    El* root = view.IsValid()
                   ? EntityRender(&app, &window, frame, view.id)
                   : nullptr;
    utassert(root != nullptr && !error.IsSet());
    utassert(ExecWaitIdle(5000));
    utassert(runtime && runtime->Eval(
        StrL("if (fetchResult !== '200|true|https://api.example.test/data|true|{\"answer\":42}|42') throw new Error(fetchResult)"),
        StrL("fetch-result.js"), &error));
    utassert(runtime && runtime->LiveTasks() == 0 && gShellFetchCalls == 1);
    EntityDrop(&app, view.id);
    app.windows.len = 0;
    ArenaDelete(frame);
    runtime->Release();
    ShellErrorClear(&error);
    AppGlobalClear(&app);
    FetchSetHttpGetForTests(nullptr);
    Capabilities denied;
    PolicyUpdateDefaultCapabilities(denied);
}

void TestShellCore() {
    TestSuite("shell_core");
    BridgedValuesMatchJavaScriptConversions();
    RuntimeMetricsSeparateScriptNativeAndFrames();
    CapabilitiesAreDenyFirstAndScoped();
    FilesystemGrantsReturnRootRelativeAuthority();
    PoliciesFreezeCapabilityGrants();
    ScopeGenerationsExpireAdoptAndRefuseReentry();
    SpecElementsAreSingleUseValues();
    SpecsAndSnapshotsDumpWithoutEnteringTheVm();
    ThemeTokenNamesAndValuesComeFromTheTheme();
    RuntimeLoadsRendersAndRetiresCallbacks();
    RuntimeAbortsFailedSnapshotTransactions();
    RuntimeLoadsOnlyModulesInsideTheApplicationRoot();
    ShellSourceWatchReloadsAtomically();
    ShellHostModulesBridgePlainDataAndPromises();
    PublishedSnapshotsMaterializeToNativeElements();
    ShellMaterializesStateTemplatesInputsAndPaths();
    ScriptViewsReuseSnapshotsUntilNotified();
    RetainedScriptStateSurvivesFramesAndDispatchesEvents();
    NestedScriptViewsRetainUpdateRollbackAndRelease();
    VirtualListsRenderOneVisibleBatch();
    ShellSandboxWithholdsCompilersAndSharedPrototypeWrites();
    ShellSchedulerResumesPromisesInTaskScope();
    ShellStorageAndAuthorityFreeModulesWork();
    ShellStorageWritesRevisionsInOrderAndFlushes();
    ShellProcessRunIsBoundedAndPromiseBased();
    ShellFilesystemUsesGrantedHandleRelativePaths();
    ShellCryptoAndCompressionMatchStandardRuntime();
    ShellFetchChecksEveryGetTargetBeforeContact();
}

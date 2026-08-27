/* Engine-independent shell semantics, ported from crates/shell/src/{value,
 * metrics, capability, policy, scope, spec, snapshot}.rs. */

#include "Test.h"

#include <stdio.h>

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
        "globalThis.virtualBatches = 0;\n"
        "export default class Main extends View {\n"
        "  init(props, cx) { this.scroll = VirtualListScrollHandle.new(); }\n"
        "  render(cx) { return v_virtual_list('rows', 20, 24,\n"
        "    index => 'row-' + index,\n"
        "    range => { globalThis.virtualBatches += 1; const out = [];\n"
        "      for (let i = range.start; i < range.end; i++) out.push(div().child('row ' + i));\n"
        "      return out;\n"
        "    }).track_scroll(this.scroll); }\n"
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
    utassert(runtime->LiveEntities() == 1);
    EntityDrop(&app, view.id);
    utassert(runtime->LiveEntities() == 0);
    ArenaDelete(frame);
    runtime->Release();
    ShellErrorClear(&error);
    AppGlobalClear(&app);
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
    PublishedSnapshotsMaterializeToNativeElements();
    ScriptViewsReuseSnapshotsUntilNotified();
    RetainedScriptStateSurvivesFramesAndDispatchesEvents();
    VirtualListsRenderOneVisibleBatch();
}

/* Engine-independent shell semantics, ported from crates/shell/src/{value,
 * metrics, capability, policy, scope, spec, snapshot}.rs. */

#include "Test.h"

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
}

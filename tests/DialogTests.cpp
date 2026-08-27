/* Ported from crates/base/src/dialog.rs.
 *
 * Rust's own cases there build a window; the two rules worth pinning are the
 * key bindings — which live in the "Dialog" key context, so they exist only
 * while `keyboard` is on and the dialog declares it — and the four conditions
 * a backdrop press has to satisfy before it dismisses. */

#include "Test.h"

static DialogAction ForChord(const char* spec) {
    KeyChord c = {};
    utassert(KeyChordParse(Str(spec), &c));
    uint32_t ctx = KeyContextOf(DialogContext());
    return DialogActionOf(KeymapMatch(c, &ctx, 1).action);
}

static void EscapeCancelsAndEnterConfirms() {
    DialogInitKeys();
    utassert(ForChord("escape") == DialogAction::Cancel);
    utassert(ForChord("enter") == DialogAction::Confirm);
    utassert(ForChord("tab") == DialogAction::None);
    utassert(ForChord("space") == DialogAction::None);
}

static void KeyboardOffRemovesTheBindings() {
    // Rust hangs the whole key context off `keyboard`, and so does this: a
    // dialog with it off never declares the context, so the chords resolve
    // against whatever is outside the dialog instead — which, with nothing
    // bound out there, is nothing at all.
    DialogInitKeys();
    KeyChord escape = {};
    utassert(KeyChordParse(StrL("escape"), &escape));
    utassert(KeymapMatch(escape, nullptr, 0).action == 0);
    uint32_t other = KeyContextOf(StrL("SomethingElse"));
    utassert(KeymapMatch(escape, &other, 1).action == 0);
}

// The two handlers a dialog keeps for its actions: on_cancel falls back to
// what the x and the backdrop do, since Rust's default on_cancel closes.
static int gRan = 0;
struct DialogRecorder {
    static El* Render(DialogRecorder*, Ctx* cx) { return Div(cx->a); }
    static void Cancel(DialogRecorder*, Ctx*, const ClickEvent*) { gRan = 1; }
    static void Ok(DialogRecorder*, Ctx*, const ClickEvent*) { gRan = 2; }
    static void Close(DialogRecorder*, Ctx*, const ClickEvent*) { gRan = 3; }
};

static void TheActionsRunTheSameHandlersTheButtonsDo() {
    App app;
    Window* win = new Window();
    win->app = &app;
    Entity<DialogRecorder> rec = EntityNew<DialogRecorder>(&app);
    Entity<DialogKeys> keys = EntityNewState<DialogKeys>(&app);
    DialogKeys* k = keys.Get(&app);
    k->onCancel = ListenTo(rec, &DialogRecorder::Cancel);
    k->onOk = ListenTo(rec, &DialogRecorder::Ok);
    k->onClose = ListenTo(rec, &DialogRecorder::Close);

    ActionEvent ev;
    ev.action = ActionOf(StrL("ui::Cancel"));
    gRan = 0;
    ListenerCall(&app, win, ListenTo(keys, &DialogKeys::OnAction), &ev);
    utassert(gRan == 1);

    ev.action = ActionOf(StrL("ui::Confirm"));
    gRan = 0;
    ListenerCall(&app, win, ListenTo(keys, &DialogKeys::OnAction), &ev);
    utassert(gRan == 2);

    // With no cancel of its own, escape closes.
    k->onCancel = {};
    ev.action = ActionOf(StrL("ui::Cancel"));
    gRan = 0;
    ListenerCall(&app, win, ListenTo(keys, &DialogKeys::OnAction), &ev);
    utassert(gRan == 3);

    delete win;
}

static void ABackdropPressDismissesOnlyWhenAllFourHold() {
    // The ordinary case: left button, closable, topmost, below the band.
    utassert(DialogBackdropCloses(true, true, MouseButton::Left, 100, 34));
    // Above the reserved band, where a title bar still is.
    utassert(!DialogBackdropCloses(true, true, MouseButton::Left, 10, 34));
    // A secondary press is not a dismissal.
    utassert(!DialogBackdropCloses(true, true, MouseButton::Right, 100, 34));
    // overlay_closable off.
    utassert(!DialogBackdropCloses(false, true, MouseButton::Left, 100, 34));
    // Under another dialog, so the press belongs to the one on top.
    utassert(!DialogBackdropCloses(true, false, MouseButton::Left, 100, 34));
    // Exactly on the boundary counts as below it, as Rust's `<` says.
    utassert(DialogBackdropCloses(true, true, MouseButton::Left, 34, 34));
}

namespace {
struct DialogHandleRecorder {
    int changes = 0;
    int triggerCalls = 0;
    DialogOpenChangeEvent last = {};

    static void OnChange(DialogHandleRecorder* self, Ctx*,
                         const DialogOpenChangeEvent* ev) {
        self->changes++;
        self->last = *ev;
    }
    static void OnTrigger(DialogHandleRecorder* self, Ctx*,
                          const MouseDownEvent*) {
        self->triggerCalls++;
    }
};
} // namespace

// DialogHandle's Entity projection preserves Rust's shared-clone behavior:
// imperative and trigger changes reach every copy and carry their reason.
static void ASharedHandleControlsTriggersAndHosts() {
    App app = {};
    Window* win = new Window();
    win->app = &app;
    Arena* arena = ArenaNew();
    Ctx cx = {&app, win, arena, {}};
    Entity<DialogHandleRecorder> recorder =
        EntityNewState<DialogHandleRecorder>(&app);
    DialogHandle handle = DialogHandle::New(&cx, false);
    DialogHandle copy = handle;
    handle.OnOpenChange(&app,
                        ListenTo(recorder, &DialogHandleRecorder::OnChange));

    utassert(!handle.IsOpen(&app) && !copy.IsOpen(&app));
    utassert(copy.Open(&cx));
    DialogHandleRecorder* seen = recorder.Get(&app);
    utassert(seen && seen->changes == 1 && seen->last.open);
    utassert(seen->last.reason == DialogChangeReason::Imperative);
    utassert(handle.IsOpen(&app));
    // Replacing true with true is the source's no-op.
    utassert(!handle.Open(&cx) && seen->changes == 1);

    utassert(handle.Close(&cx));
    El* closed = Dialog::New(&cx)->Handle(copy)->IntoEl();
    utassert(closed->accessibility.role == AccessibilityRole::None);
    El* closedAlert = AlertDialog::New(&cx)->Handle(copy)->IntoEl();
    utassert(closedAlert->accessibility.role == AccessibilityRole::None);

    El* trigger = DialogTrigger::New(
        &cx, ListenTo(recorder, &DialogHandleRecorder::OnTrigger), copy);
    utassert(trigger->onMouseDown.IsValid());
    MouseDownEvent right = {};
    right.button = MouseButton::Right;
    ListenerCall(&app, win, trigger->onMouseDown, &right);
    utassert(!handle.IsOpen(&app) && seen->triggerCalls == 0);
    MouseDownEvent down = {};
    ListenerCall(&app, win, trigger->onMouseDown, &down);
    utassert(handle.IsOpen(&app));
    utassert(seen->changes == 3 && seen->last.open);
    utassert(seen->last.reason == DialogChangeReason::TriggerPress);
    utassert(seen->triggerCalls == 1);

    El* open = Dialog::New(&cx)->Handle(handle)->IntoEl();
    utassert(open->accessibility.role == AccessibilityRole::Dialog);
    El* openAlert = AlertDialog::New(&cx)->Handle(handle)->IntoEl();
    utassert(openAlert->accessibility.role == AccessibilityRole::AlertDialog);

    EntityDropAll(&app);
    ArenaDelete(arena);
    delete win;
}

void TestDialog() {
    TestSuite("dialog");
    EscapeCancelsAndEnterConfirms();
    TheActionsRunTheSameHandlersTheButtonsDo();
    KeyboardOffRemovesTheBindings();
    ABackdropPressDismissesOnlyWhenAllFourHold();
    ASharedHandleControlsTriggersAndHosts();
}

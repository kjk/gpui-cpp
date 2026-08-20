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

void TestDialog() {
    TestSuite("dialog");
    EscapeCancelsAndEnterConfirms();
    TheActionsRunTheSameHandlersTheButtonsDo();
    KeyboardOffRemovesTheBindings();
    ABackdropPressDismissesOnlyWhenAllFourHold();
}

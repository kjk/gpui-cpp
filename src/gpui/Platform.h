/* The seam between the portable window logic (WindowCommon.cpp) and the OS
   window (Window_win.cpp / Window_linux.cpp).

   The platform file owns the message loop and translates native events into
   the Window* calls below; everything those calls do — hit-testing, focus,
   listener dispatch, drawing — is shared. */

#include "gpui/Gpui.h"

namespace gpui {

// ─── called by the platform window ───────────────────────────────────────

// Build, lay out and paint one frame onto `native` — the HDC on Windows, the
// cairo surface on Linux.
void WindowDrawFrame(Window* win, void* native, int pxW, int pxH, float dipW,
                     float dipH);

// `key` is one of the Key* codes in Gpui.h.
void WindowKeyDown(Window* win, int key, bool shift, bool ctrl, bool alt);
// A typed character, already decoded to a codepoint.
void WindowChar(Window* win, uint32_t ch, bool ctrl, bool alt);
void WindowMouseMove(Window* win, float x, float y);
void WindowMouseDown(Window* win, float x, float y, int button);
void WindowMouseUp(Window* win, float x, float y, int button);
void WindowMouseLeave(Window* win);
void WindowWheel(Window* win, float x, float y, float delta);
void WindowDoubleClick(Window* win, float x, float y);
// The timer fired: run whatever is due — the caret flip and any armed
// timers — repaint if that changed anything, and re-arm through PlatSetTimer
// for the next deadline. The platform never computes an interval itself.
void WindowTimerTick(Window* win);
// The window chrome under the cursor: ClickWinMin / Max / Close / Caption, or
// 0 for ordinary content. Windows answers WM_NCHITTEST with it; X11 uses it
// to route a press on the custom title bar.
int WindowChromeHit(Window* win, float x, float y);

// Milliseconds until the window next wants waking, or 0 if nothing does.
int WindowTimerMs(Window* win);

// The OS window went away. Frees the paint target and clears `plat`.
void WindowClosed(Window* win);
bool AppAnyWindowOpen(App* app);
// Allocate and register the Window, minus its OS half. WindowOpen fills in
// `plat` and shows it.
Window* WindowAlloc(App* app, WinOpts opts);
// Cap a requested window size at 85% of the display, the way Rust's
// create_new_window_with_size does. Each WindowOpen calls it with the metrics
// its platform reports.
void WindowClampToDisplay(int* dipW, int* dipH, int screenW, int screenH);

// ─── implemented per platform ────────────────────────────────────────────

// Process-wide setup: DPI awareness and the window class on Windows, the X11
// display on Linux. False aborts AppNew.
bool PlatInit(App* app);
void PlatShutdown(App* app);
// Restart the window's repeating timer at `ms`; 0 stops it.
void PlatSetTimer(Window* win, int ms);
// Ask the OS for a pointer shape. Only called when it changes.
void PlatSetCursor(Window* win, CursorKind kind);

} // namespace gpui

/* Runtime seam behind Window::use_keyed_state. The component WindowExt store
 * relies on its object and owned Vec/entity handles dying with the window. */

#include "Test.h"

namespace {
struct KeyedLifetime {
    static int made;
    static int dropped;
    int initial = 73;

    KeyedLifetime() { made++; }
    ~KeyedLifetime() { dropped++; }
};
int KeyedLifetime::made = 0;
int KeyedLifetime::dropped = 0;
} // namespace

static void TheFirstObjectIsKeptAndLaterCandidatesAreDropped() {
    KeyedLifetime::made = 0;
    KeyedLifetime::dropped = 0;
    Window window;
    Ctx cx = {};
    cx.win = &window;

    KeyedLifetime* first = KeyedState<KeyedLifetime>(&cx, 41);
    KeyedLifetime* again = KeyedState<KeyedLifetime>(&cx, 41);
    utassert(first != nullptr && again == first);
    utassert(first->initial == 73);
    utassert(KeyedLifetime::made == 2);
    utassert(KeyedLifetime::dropped == 1);

    WindowKeyedFree(&window);
    utassert(KeyedLifetime::dropped == 2);
    utassert(window.keyed.len == 0);
}

void TestKeyedState() {
    TestSuite("keyed state");
    TheFirstObjectIsKeptAndLaterCandidatesAreDropped();
}

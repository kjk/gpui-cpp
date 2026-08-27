/* Ported from crates/ui/src/avatar/avatar.rs.
 *
 * `extract_text_initials` is what `Avatar::name` shows when there is no
 * image: the first letter of each of the first two words, upper-cased, and
 * the first two letters of a single word instead. */

#include "Test.h"

using namespace gpui::component;

static void CheckInitials(const char* name, const char* want) {
    char buf[8];
    Str got = AvatarInitials(buf, (int)sizeof(buf), Str(name));
    // Not StrEqI: the point of the last step is the case.
    utassert(base::StrEq(got, want));
}

void TestAvatar() {
    TestSuite("avatar");
    CheckInitials("Jason Lee", "JL");
    CheckInitials("Foo Bar Dar", "FB");
    CheckInitials("huacnlee", "HU");
}

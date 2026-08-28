#ifndef GPUI_SHELL_A11Y_H_
#define GPUI_SHELL_A11Y_H_

#include "gpui/gpui.h"

namespace gpui::shell {

AccessibilityRole AccessibilityRoleFromName(Str name);
int AccessibilityRoleNameCount();

} // namespace gpui::shell

#endif // GPUI_SHELL_A11Y_H_

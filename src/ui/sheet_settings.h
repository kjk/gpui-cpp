/* SheetSettings — crates/ui/src/sheet.rs.

   Kept in its own leaf header because Theme owns one and Sheet consumes it;
   neither module should have to include the other's component surface. */

#pragma once

namespace gpui {
namespace component {

constexpr float kSheetDefaultMarginTop = 34.f;

struct SheetSettings {
    // The top margin for every placement except Bottom. Upstream defaults it
    // to TITLE_BAR_HEIGHT so a sheet begins below custom window chrome.
    float marginTop = kSheetDefaultMarginTop;
};

} // namespace component
} // namespace gpui

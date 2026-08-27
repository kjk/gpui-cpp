#ifndef GPUI_UI_NOTIFICATION_SETTINGS_H_
#define GPUI_UI_NOTIFICATION_SETTINGS_H_
/* NotificationSettings and NotificationType — crates/ui/src/notification.rs.

   This is a leaf because Theme owns the settings and Notification consumes
   them. Keep the public values here so neither header includes the other. */

#pragma once

#include "gpui/gpui.h"

namespace gpui {
namespace component {

enum class NotificationType : uint8_t {
    Info,
    Success,
    Warning,
    Error
};

enum class NotificationDelivery : uint8_t {
    InApp,
    System,
    InAppAndSystem
};

struct NotificationSettings {
    Anchor placement = Anchor::TopRight;
    // TITLE_BAR_HEIGHT + 16 at the top, 16 on every other window edge.
    Edges margins = Edges::New(16.f, 16.f, 50.f, 16.f);
    int maxItems = 10;
    float width = 382.f;
    NotificationDelivery delivery = NotificationDelivery::InApp;
};

} // namespace component
} // namespace gpui
#endif // GPUI_UI_NOTIFICATION_SETTINGS_H_

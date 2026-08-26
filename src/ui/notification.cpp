#include "ui/notification.h"
#include "base/motion.h"
#include "sys/notify.h"
#include "ui/alert.h"
#include "ui/title_bar.h"

namespace gpui {

namespace component {

Notification* Notification::New(Ctx* cx, Str title, Str message) {
    Arena* a = cx->a;
    Notification* n = ArenaNew<Notification>(a);
    n->a = a;
    n->cx = cx;
    n->title = title;
    n->message = message;
    return n;
}
Notification* Notification::Kind(NotificationKind k) {
    kind = k;
    return this;
}
Notification* Notification::Action(El* e) {
    action = e;
    return this;
}
Notification* Notification::Content(El* e) {
    content = e;
    return this;
}
Notification* Notification::Placement(NotificationAnchor p) {
    anchor = p;
    return this;
}
Notification* Notification::OnClose(Listener fn) {
    onClose = fn;
    return this;
}
Notification* Notification::OnClick(Listener fn) {
    onClick = fn;
    return this;
}

bool NotificationDeliveryIncludesInApp(NotificationDelivery d) {
    return d == NotificationDelivery::InApp ||
           d == NotificationDelivery::InAppAndSystem;
}

bool NotificationDeliveryIncludesSystem(NotificationDelivery d) {
    return d == NotificationDelivery::System ||
           d == NotificationDelivery::InAppAndSystem;
}

// --- the system half ------------------------------------------------------

// SYSTEM_TAG_PREFIX. A response to a tag without it belongs to whoever posted
// it, and is not ours to retract or dispatch.
static const char kSystemTagPrefix[] = "gpui-component/notification/";

Str NotificationSystemTag(char* buf, int cap, int id) {
    if (!buf || cap <= 0) {
        return {};
    }
    TempStr s = fmt("%d", id);
    int prefixLen = (int)sizeof(kSystemTagPrefix) - 1;
    int n = prefixLen + s.len;
    if (n > cap - 1) {
        n = cap - 1;
    }
    int take = n < prefixLen ? n : prefixLen;
    memcpy(buf, kSystemTagPrefix, (size_t)take);
    if (n > take) {
        memcpy(buf + take, s.s, (size_t)(n - take));
    }
    buf[n] = 0;
    return Str(buf, n);
}

bool NotificationTagId(Str tag, int* outId) {
    int prefixLen = (int)sizeof(kSystemTagPrefix) - 1;
    if (!tag.s || tag.len <= prefixLen) {
        return false;
    }
    if (memcmp(tag.s, kSystemTagPrefix, (size_t)prefixLen) != 0) {
        return false;
    }
    int id = 0;
    for (int i = prefixLen; i < tag.len; i++) {
        char c = tag.s[i];
        if (c < '0' || c > '9') {
            return false;
        }
        id = id * 10 + (c - '0');
    }
    if (id <= 0) {
        return false;
    }
    if (outId) {
        *outId = id;
    }
    return true;
}

static App* gSysApp = nullptr;
static bool gSysInited = false;

struct NotificationSystemState {
    App* app = nullptr;
    Vec<NotificationSystemEntry> entries;

    ~NotificationSystemState() {
        entries.Reset();
        if (gSysApp == app) {
            gSysApp = nullptr;
        }
    }
};

static NotificationSystemState* SysState(App* app) {
    NotificationSystemState* state =
        AppGlobalEnsure<NotificationSystemState>(app);
    if (state && !state->app) {
        state->app = app;
    }
    return state;
}

static NotificationSystemState* SysState(Window* win) {
    return SysState(win ? win->app : gSysApp);
}

static void SysEntryRemoveAt(NotificationSystemState* state, int ix) {
    for (int i = ix; i < state->entries.len - 1; i++) {
        state->entries[i] = state->entries[i + 1];
    }
    state->entries.len--;
}

// The platform's response, on the main thread. Only the tag comes back, so
// everything else is what the registry kept.
static void OnSysNotifyResponse(Str tag, void*) {
    NotificationSystemResponse(tag);
}

void NotificationInitSystem(App* app) {
    if (!app) {
        return;
    }
    (void)SysState(app);
    gSysApp = app;
    if (!gSysInited) {
        gSysInited = true;
        SysNotifyOnResponse(OnSysNotifyResponse, nullptr);
        // Whatever the platform is holding — on Windows a notification area
        // icon — is given back when the process goes.
        AppOnShutdown(SysNotifyShutdown);
    }
}

void NotificationSystemInsert(const NotificationSystemEntry& e) {
    NotificationSystemState* state = SysState(e.win);
    if (!state) {
        return;
    }
    for (int i = 0; i < state->entries.len; i++) {
        if (state->entries[i].id == e.id) {
            SysEntryRemoveAt(state, i);
            break;
        }
    }
    if (state->entries.len >= kNotificationSystemMax) {
        SysEntryRemoveAt(state, 0);
    }
    state->entries.Append(e);
}

const NotificationSystemEntry* NotificationSystemFind(int id, Window* win) {
    NotificationSystemState* state = SysState(win);
    if (!state) {
        return nullptr;
    }
    for (int i = 0; i < state->entries.len; i++) {
        if (state->entries[i].id == id && state->entries[i].win == win) {
            return &state->entries[i];
        }
    }
    return nullptr;
}

int NotificationSystemCount() {
    return NotificationSystemCount(gSysApp);
}

int NotificationSystemCount(const App* app) {
    NotificationSystemState* state =
        AppGlobalGet<NotificationSystemState>(app);
    return state ? state->entries.len : 0;
}

static void SysDismissTag(int id) {
    char buf[64];
    SysNotifyDismiss(NotificationSystemTag(buf, (int)sizeof(buf), id));
}

static bool SystemIdentityMatches(const NotificationSystemEntry& e,
                                  NotificationTypeId type, bool hasKey,
                                  uint32_t key) {
    return e.identityType == type &&
           (!hasKey || (e.identityHasKey && e.identityKey == key));
}

static int NotificationSystemIdentityId(const NotificationItem& item,
                                        Window* win) {
    if (!item.identityType) {
        return 0;
    }
    NotificationSystemState* state = SysState(win);
    if (!state) {
        return 0;
    }
    for (int i = 0; i < state->entries.len; i++) {
        const NotificationSystemEntry& e = state->entries[i];
        if (e.win == win && e.identityType == item.identityType &&
            e.identityHasKey == item.identityHasKey &&
            (!e.identityHasKey || e.identityKey == item.identityKey)) {
            return e.id;
        }
    }
    return 0;
}

void NotificationSystemDismiss(int id, Window* win) {
    NotificationSystemState* state = SysState(win);
    if (!state) {
        return;
    }
    for (int i = 0; i < state->entries.len; i++) {
        if (state->entries[i].id == id && state->entries[i].win == win) {
            SysEntryRemoveAt(state, i);
            SysDismissTag(id);
            return;
        }
    }
}

static void NotificationSystemDismissIdentity(NotificationTypeId type,
                                              bool hasKey, uint32_t key,
                                              Window* win) {
    NotificationSystemState* state = SysState(win);
    if (!state || !type) {
        return;
    }
    for (int i = state->entries.len - 1; i >= 0; i--) {
        if (state->entries[i].win != win ||
            !SystemIdentityMatches(state->entries[i], type, hasKey, key)) {
            continue;
        }
        int id = state->entries[i].id;
        SysEntryRemoveAt(state, i);
        SysDismissTag(id);
    }
}

void NotificationSystemDismissByType(NotificationTypeId type, Window* win) {
    NotificationSystemDismissIdentity(type, false, 0, win);
}

void NotificationSystemDismissByTypeKey(NotificationTypeId type,
                                         uint32_t key, Window* win) {
    NotificationSystemDismissIdentity(type, true, key, win);
}

void NotificationSystemDismissAll(Window* win) {
    NotificationSystemState* state = SysState(win);
    if (!state) {
        return;
    }
    for (int i = state->entries.len - 1; i >= 0; i--) {
        if (state->entries[i].win == win) {
            int id = state->entries[i].id;
            SysEntryRemoveAt(state, i);
            SysDismissTag(id);
        }
    }
}

void NotificationSystemResponse(Str tag) {
    int id = 0;
    if (!NotificationTagId(tag, &id)) {
        return;
    }
    // Platforms usually take a clicked notification away themselves; the ones
    // that keep it are why this is asked for at all.
    SysNotifyDismiss(tag);
    NotificationSystemState* state = SysState(gSysApp);
    const NotificationSystemEntry* found = nullptr;
    if (state) {
        for (int i = 0; i < state->entries.len; i++) {
            if (state->entries[i].id == id) {
                found = &state->entries[i];
                break;
            }
        }
    }
    // The platform can deliver a response for a notification posted before
    // the process restarted, and entries past the cap are pruned. Rust brings
    // the application forward even then; there is no window named here to
    // bring forward, so nothing happens.
    if (!found) {
        return;
    }
    Window* win = found->win;
    EntityId list = found->list;
    AppActivate(win);
    if (!list.IsValid()) {
        // Nothing to dispatch to: the list never rendered, so its handle was
        // never stamped. The window is still brought forward.
        return;
    }
    // Off the platform's own event and onto the next turn of the loop, which
    // is where an entity may be touched. The post is dropped if the window or
    // the list has gone by then.
    Entity<NotificationListState> e;
    e.id = list;
    WindowPost(win, ListenTo(e, &NotificationListState::OnSystemResponse,
                             (intptr_t)id));
}

void NotificationListState::OnSystemResponse(NotificationListState* self,
                                             Ctx* cx, const ClickEvent*,
                                             intptr_t idArg) {
    int id = (int)idArg;
    Listener onClick = {};
    NotificationSystemState* state = SysState(cx->app);
    if (state) {
        for (int i = 0; i < state->entries.len; i++) {
            if (state->entries[i].id == id &&
                state->entries[i].win == cx->win) {
                onClick = state->entries[i].onClick;
                SysEntryRemoveAt(state, i);
                break;
            }
        }
    }
    // A no-op when the card already closed or was never created, which is
    // what system-only delivery leaves.
    NotificationDismiss(self, cx, id);
    if (onClick.IsValid()) {
        ClickEvent ev = {};
        ListenerCall(cx->app, cx->win, onClick, &ev);
    }
    Notify(cx);
}

int NotificationIndexOf(const NotificationListState* s, int id) {
    for (int i = 0; i < s->items.len; i++) {
        if (s->items[i].id == id) {
            return i;
        }
    }
    return -1;
}

bool NotificationIdentitySame(const NotificationItem& a,
                              const NotificationItem& b) {
    if (a.identityType || b.identityType) {
        return a.identityType != 0 && a.identityType == b.identityType &&
               a.identityHasKey == b.identityHasKey &&
               (!a.identityHasKey || a.identityKey == b.identityKey);
    }
    return a.id != 0 && a.id == b.id;
}

static int NotificationIndexOfIdentity(const NotificationListState* s,
                                       const NotificationItem& item) {
    for (int i = 0; i < s->items.len; i++) {
        if (NotificationIdentitySame(s->items[i], item)) {
            return i;
        }
    }
    return -1;
}

static void NotificationRemoveAt(NotificationListState* s, int ix) {
    ToastRemove(&s->stack, s->items[ix].id);
    for (int i = ix; i < s->items.len - 1; i++) {
        s->items[i] = s->items[i + 1];
    }
    s->items.len--;
}

// push_system: what the OS notification center is given, and the registry
// entry its response comes back through.
static void NotificationPushSystem(NotificationListState* s, Ctx* cx,
                                   const NotificationItem& item) {
    Str title = item.title;
    Str body = item.message;
    if (title.len == 0) {
        // A message with no title becomes the system notification's title,
        // and one with neither — a content-only notification — has nothing
        // textual to show.
        if (body.len == 0) {
            return;
        }
        title = body;
        body = Str{};
    }
    NotificationInitSystem(cx->app);
    char buf[64];
    Str tag = NotificationSystemTag(buf, (int)sizeof(buf), item.id);
    // Rust registers and then posts; the order is the other way here because
    // a platform that cannot post — every one but Windows so far — would
    // otherwise leave an entry waiting for a response that cannot come.
    if (!SysNotifyShow(tag, title, body)) {
        return;
    }
    NotificationSystemEntry e;
    e.id = item.id;
    e.identityType = item.identityType;
    e.identityKey = item.identityKey;
    e.identityHasKey = item.identityHasKey;
    e.list = s->self;
    e.win = cx->win;
    e.onClick = item.onClick;
    NotificationSystemInsert(e);
}

int NotificationPush(NotificationListState* s, Ctx* cx, NotificationItem item,
                     int timeoutMs) {
    // A push with an id already in the list replaces that one: Rust keys its
    // notifications by NotificationId, so the same one never stacks twice.
    int at = NotificationIndexOfIdentity(s, item);
    if (at >= 0) {
        if (item.id == 0) {
            item.id = s->items[at].id;
        }
        NotificationRemoveAt(s, at);
    }
    NotificationDelivery delivery =
        item.hasDelivery ? item.delivery : s->delivery;
    if (item.id == 0 && cx && NotificationDeliveryIncludesSystem(delivery)) {
        item.id = NotificationSystemIdentityId(item, cx->win);
    }
    if (item.id == 0) {
        item.id = s->nextId++;
    }
    // The id has to be settled before this: it is what the system tag names,
    // and what makes a second push with the same id replace the first there
    // as well.
    if (cx && NotificationDeliveryIncludesSystem(delivery)) {
        NotificationPushSystem(s, cx, item);
    }
    if (!NotificationDeliveryIncludesInApp(delivery)) {
        return item.id;
    }
    // ToastManager keeps every mounted toast. `max_items` is applied by
    // visible() while rendering; ending entries remain mounted and visible
    // until their exit completes.
    if (!s->items.Append(item)) {
        return item.id;
    }
    if (!ToastPush(&s->stack, item.id, timeoutMs)) {
        s->items.len--;
    }
    return item.id;
}

void NotificationDismiss(NotificationListState* s, Ctx* cx, int id) {
    // Unconditional, as Rust's close() is: a system-only notification has no
    // card to animate out and must still be retracted.
    if (cx) {
        NotificationSystemDismiss(id, cx->win);
    }
    for (int i = 0; i < s->stack.entries.len; i++) {
        if (s->stack.entries[i].id == id) {
            // The card animates out first; advance drops it when it is done.
            s->stack.entries[i].status = ToastStatus::Ending;
            s->stack.entries[i].elapsedMs = 0;
            return;
        }
    }
}

static void NotificationDismissIdentity(NotificationListState* s, Ctx* cx,
                                        NotificationTypeId type, bool hasKey,
                                        uint32_t key) {
    if (!s || !type) {
        return;
    }
    if (cx) {
        if (hasKey) {
            NotificationSystemDismissByTypeKey(type, key, cx->win);
        } else {
            NotificationSystemDismissByType(type, cx->win);
        }
    }
    for (int i = 0; i < s->items.len; i++) {
        const NotificationItem& item = s->items[i];
        bool matches = item.identityType == type &&
                       (!hasKey || (item.identityHasKey &&
                                    item.identityKey == key));
        if (!matches) {
            continue;
        }
        for (int j = 0; j < s->stack.entries.len; j++) {
            if (s->stack.entries[j].id == item.id) {
                s->stack.entries[j].status = ToastStatus::Ending;
                s->stack.entries[j].elapsedMs = 0;
                break;
            }
        }
    }
}

void NotificationDismissByType(NotificationListState* s, Ctx* cx,
                               NotificationTypeId type) {
    NotificationDismissIdentity(s, cx, type, false, 0);
}

void NotificationDismissByTypeKey(NotificationListState* s, Ctx* cx,
                                  NotificationTypeId type, uint32_t key) {
    NotificationDismissIdentity(s, cx, type, true, key);
}

void NotificationClear(NotificationListState* s, Ctx* cx) {
    if (cx) {
        NotificationSystemDismissAll(cx->win);
    }
    for (int i = 0; i < s->items.len; i++) {
        NotificationDismiss(s, cx, s->items[i].id);
    }
}

bool NotificationAdvance(NotificationListState* s, int deltaMs) {
    bool changed = ToastAdvance(&s->stack, deltaMs, s->stack.IsExpanded());
    if (!changed) {
        return false;
    }
    // Whatever the stack dropped goes from the list with it.
    for (int i = s->items.len - 1; i >= 0; i--) {
        bool alive = false;
        for (int j = 0; j < s->stack.entries.len; j++) {
            if (s->stack.entries[j].id == s->items[i].id) {
                alive = true;
                break;
            }
        }
        if (!alive) {
            for (int k = i; k < s->items.len - 1; k++) {
                s->items[k] = s->items[k + 1];
            }
            s->items.len--;
        }
    }
    return true;
}

void NotificationListState::OnCloseClick(NotificationListState* self, Ctx* cx,
                                         const ClickEvent*, intptr_t id) {
    NotificationDismiss(self, cx, (int)id);
    Notify(cx);
}

void NotificationListState::OnItemClick(NotificationListState* self, Ctx* cx,
                                        const ClickEvent* ev, intptr_t id) {
    int at = NotificationIndexOf(self, (int)id);
    if (at >= 0 && self->items[at].onClick.IsValid()) {
        ListenerCall(cx->app, cx->win, self->items[at].onClick, ev);
    }
    NotificationDismiss(self, cx, (int)id);
    Notify(cx);
}

void NotificationListState::OnHover(NotificationListState* self, Ctx* cx,
                                    const HoverEvent* ev) {
    // is_expanded: the pointer over the stack opens it out, and holds every
    // timeout while it is there.
    if (self->stack.hovered == ev->hovered) {
        return;
    }
    self->stack.hovered = ev->hovered;
    Notify(cx);
}

void NotificationListState::OnTick(NotificationListState* self, Ctx* cx,
                                   const TickEvent*) {
    if (NotificationAdvance(self, kNotificationTickMs)) {
        Notify(cx);
    }
}

NotificationList* NotificationList::New(Ctx* cx,
                                        Entity<NotificationListState> state) {
    Arena* a = cx->a;
    NotificationList* l = ArenaNew<NotificationList>(a);
    l->a = a;
    l->cx = cx;
    l->state = state;
    return l;
}

El* NotificationList::IntoEl() {
    NotificationListState* s = state.Get(cx);
    if (s) {
        // cx.entity(): what a system notification's response is dispatched
        // back to.
        s->self = state.id;
    }
    if (!s || s->items.len == 0) {
        return Div(a);
    }
    WinSize win = WindowSize(cx->win);
    bool bottom = s->placement == NotificationAnchor::BottomLeft ||
                  s->placement == NotificationAnchor::BottomCenter ||
                  s->placement == NotificationAnchor::BottomRight;
    bool expanded = s->stack.IsExpanded();

    // ToastManager::visible(max_items): newest active entries plus every
    // ending entry, kept in display order.
    int activeCount = 0;
    for (int i = 0; i < s->stack.entries.len; i++) {
        if (s->stack.entries[i].status != ToastStatus::Ending) {
            activeCount++;
        }
    }
    int maxItems = s->maxItems > 0 ? s->maxItems : 0;
    int firstActive = activeCount > maxItems ? activeCount - maxItems : 0;
    int activeIx = 0;
    ArenaVec<int> shown;
    for (int i = 0; i < s->stack.entries.len; i++) {
        bool ending = s->stack.entries[i].status == ToastStatus::Ending;
        bool visible = ending || activeIx >= firstActive;
        if (!ending) {
            activeIx++;
        }
        if (visible) {
            shown.Append(a, i);
        }
    }
    if (shown.len == 0) {
        return Div(a);
    }

    float* heights = (float*)Alloc(a, (int)sizeof(float) * shown.len);
    float* collapsedOff =
        (float*)Alloc(a, (int)sizeof(float) * shown.len);
    float* expandedOff =
        (float*)Alloc(a, (int)sizeof(float) * shown.len);
    for (int i = 0; i < shown.len; i++) {
        heights[i] = s->itemH;
    }
    float expandedH = 0;
    float collapsedH = ToastStackGeometry(
        heights, shown.len, kToastCollapsedPeek, kToastExpandedGap, bottom,
        collapsedOff, expandedOff, &expandedH);
    // toast.rs: the geometry is sprung rather than transitioned — a pointer
    // arriving and leaving retargets every offset while they are still
    // moving, and a spring turns them around from where they are. A pixel's
    // tenth is arrived; the fade keeps the finer default, since it runs over
    // 0..1.
    Spring geometry = SpringNew((float)kToastTransitionMs);
    geometry.epsilon = 0.1f;
    Spring fade = SpringNew((float)kToastTransitionMs);
    // The stack's own height, which is what opens the space the cards move
    // into rather than snapping the whole layer taller.
    float stackH =
        SpringValue(cx, MotionId(StrL("notification-stack"), StrL("height")),
                    expanded ? expandedH : collapsedH, geometry);

    // The stack floats over the window in the corner its placement names.
    El* layer = Div(a)->Absolute()->Fixed()->W(s->width)->H(stackH)->OnHover(
        ListenTo(state, &NotificationListState::OnHover));
    float margin = kNotificationMargin;
    bool right = s->placement == NotificationAnchor::TopRight ||
                 s->placement == NotificationAnchor::RightCenter ||
                 s->placement == NotificationAnchor::BottomRight;
    bool center = s->placement == NotificationAnchor::TopCenter ||
                  s->placement == NotificationAnchor::BottomCenter;
    float left = right ? win.dipW - s->width - margin
                       : (center ? (win.dipW - s->width) * 0.5f : margin);
    // The top margin clears the title bar, which is what Rust's default
    // margins do.
    float top = bottom ? win.dipH - stackH - margin : kTitleBarHeight + margin;
    if (s->placement == NotificationAnchor::LeftCenter ||
        s->placement == NotificationAnchor::RightCenter) {
        top = (win.dipH - stackH) * 0.5f;
    }
    layer->Left(left)->Top(top);
    // The stack needs a hit box of its own for the hover to reach it.
    layer->Id(StrL("notification-stack"))
        ->Click(HashClickId(StrL("notification-stack")));

    for (int i = 0; i < shown.len; i++) {
        const NotificationItem& it = s->items[shown[i]];
        int rank = shown.len - 1 - i;
        // "visibility": collapsed_visible is how many of them show at all
        // when the stack is closed, and the ones past it fade rather than
        // vanishing. A card that has finished fading is left out — Rust keeps
        // it in the tree at zero opacity, where it would still be in the way
        // of the pointer.
        Str key = StrDup(a, fmt("%d", it.id));
        float visible = SpringValue(
            cx, MotionId(StrL("toast-visibility"), key),
            (expanded || rank < kToastCollapsedVisible) ? 1.f : 0.f, fade);
        if (visible <= 0.01f) {
            continue;
        }
        // "offset" and "inset": where this card sits, and how much narrower
        // it is than the front one. Both are the card's own transitions, keyed
        // on its id, so a stack that opens moves each of them from wherever it
        // had got to.
        float off =
            SpringValue(cx, MotionId(StrL("toast-offset"), key),
                        expanded ? expandedOff[i] : collapsedOff[i], geometry);
        float shrink = SpringValue(
            cx, MotionId(StrL("toast-inset"), key),
            expanded ? 0.f
                     : s->width * kToastCollapsedScaleStep *
                           (float)(rank < kToastCollapsedVisible
                                       ? rank
                                       : kToastCollapsedVisible - 1),
            geometry);
        El* card =
            Notification::New(cx, it.title, it.message)
                ->Kind(it.kind)
                ->Content(it.content)
                ->OnClose(ListenTo(state, &NotificationListState::OnCloseClick,
                                   (intptr_t)it.id))
                ->OnClick(ListenTo(state, &NotificationListState::OnItemClick,
                                   (intptr_t)it.id))
                ->IntoEl();
        layer->Child(Div(a)
                         ->Absolute()
                         ->Top(off)
                         ->Left(shrink * 0.5f)
                         ->W(s->width - shrink)
                         ->Opacity(visible)
                         ->Child(card));
    }
    return layer;
}

El* Notification::IntoEl() {
    const Theme& th = ThemeNow(cx->app);
    // Notification's own box, not an Alert: border_1 in theme.border on the
    // popover surface, radius_lg, shadow_md, py_3p5 px_4 gap_3. Nothing here
    // is tinted by the kind — only the icon is.
    El* card = gpui::Toast::New(cx, StrL("notification"))
                    ->FlexRow()
                   ->Group()
                   ->W(kFill)
                   ->Gap(12)
                   ->PadY(14)
                   ->PadX(16)
                   ->Border(1, th.border)
                   ->Bg(th.tokens.popover)
                   // shadow_md: there is no box shadow in paint.h, so the
                   // border is what separates the card from the page.
                   ->Radius(th.radiusLg);
    IconName iconName = IconName::None;
    Rgba iconFg = th.foreground;
    if (kind == NotificationKind::Info) {
        iconName = IconName::Info;
        iconFg = th.info;
    } else if (kind == NotificationKind::Success) {
        iconName = IconName::CircleCheck;
        iconFg = th.success;
    } else if (kind == NotificationKind::Warning) {
        iconName = IconName::TriangleAlert;
        iconFg = th.warning;
    } else if (kind == NotificationKind::Error) {
        iconName = IconName::CircleX;
        iconFg = th.danger;
    }
    bool hasIcon = iconName != IconName::None;
    if (hasIcon) {
        // div().absolute().top(px(18.)).left_4(): out of the row, so the
        // body's own pl_6 is what keeps the text clear of it.
        card->Child(Div(a)->Absolute()->Top(18)->Left(16)->Child(
            IconEl(a, iconName, 16)->Fg(iconFg)));
    }
    El* body = Div(a)->FlexCol()->Flex1()->ClipX()->ClipY()->Gap(4);
    if (hasIcon) {
        body->PadL(24);
    }
    if (title.s && title.len > 0) {
        body->Child(TextEl(a, title)
                        ->Font(14)
                        ->Semibold()
                        ->Fg(th.foreground)
                        ->Wrap()
                        ->W(kFill));
    }
    if (message.s && message.len > 0) {
        body->Child(
            TextEl(a, message)->Font(14)->Fg(th.foreground)->Wrap()->W(kFill));
    }
    if (content) {
        body->Child(content);
    }
    card->Child(body);
    if (action) {
        card->Child(action);
    }
    // The x sits in the corner and is invisible until the pointer is on the
    // card — group_hover, which is why the card is the group.
    card->Child(
        Div(a)->Absolute()->Top(4)->Right(4)->GroupHoverVisible()->Child(
            component::Button::New(cx, StrL("close"))
                ->Ghost()
                ->WithSize(UiSize::XSmall)
                ->Icon(IconName::X)
                ->OnClick(onClose)
                ->IntoEl()));
    if (anchor == NotificationAnchor::None) {
        return card;
    }
    // Rust hands the notification list to Root, which floats it in a corner
    // of the window. A fixed layer with the matching alignment does the same.
    card->W(width);
    El* layer = Div(a)
                    ->Fixed()
                    ->Top(0)
                    ->Left(0)
                    ->W(kFill)
                    ->H(kFill)
                    ->FlexCol()
                    ->Pad(16)
                    ->Child(card);
    switch (anchor) {
        case NotificationAnchor::TopLeft:
            layer->JustifyStart()->ItemsStart();
            break;
        case NotificationAnchor::TopCenter:
            layer->JustifyStart()->ItemsCenter();
            break;
        case NotificationAnchor::TopRight:
            layer->JustifyStart()->ItemsEnd();
            break;
        case NotificationAnchor::LeftCenter:
            layer->JustifyCenter()->ItemsStart();
            break;
        case NotificationAnchor::RightCenter:
            layer->JustifyCenter()->ItemsEnd();
            break;
        case NotificationAnchor::BottomLeft:
            layer->JustifyEnd()->ItemsStart();
            break;
        case NotificationAnchor::BottomCenter:
            layer->JustifyEnd()->ItemsCenter();
            break;
        default:
            layer->JustifyEnd()->ItemsEnd();
            break;
    }
    return layer;
}

} // namespace component
} // namespace gpui

#ifndef GPUI_SHELL_RETAINED_H_
#define GPUI_SHELL_RETAINED_H_

#include "base/input.h"
#include "base/otp_input.h"
#include "base/slider.h"
#include "base/virtual_list.h"
#include "shell/spec.h"

namespace gpui::shell {

using EntityHandle = uint64_t;

const int kMaxLiveEntities = 10000;

enum class RetainedKind : uint8_t {
    Input,
    Textarea,
    Slider,
    Otp,
    Focus,
    VirtualScroll,
};

enum class RetainedEvent : uint8_t {
    InputChange,
    InputSubmit,
    InputFocus,
    InputBlur,
    SliderChange,
    SliderRelease,
    OtpChange,
    OtpComplete,
    OtpFocus,
    OtpBlur,
};

struct RetainedCallback {
    RetainedEvent event = RetainedEvent::InputChange;
    CallbackId callback = 0;
};

struct NumberInputConfig {
    bool hasStep = false;
    double step = 1;
    bool hasMin = false;
    double min = 0;
    bool hasMax = false;
    double max = 0;
};

struct RetainedEntry {
    uint32_t id = 0;
    RetainedKind kind = RetainedKind::Input;
    EntityId owner = {};
    void* application = nullptr;
    App* app = nullptr;
    InputState* input = nullptr;
    SliderState* slider = nullptr;
    Entity<OtpState> otp = {};
    FocusHandle focus = {};
    VirtualListScrollHandle scroll = {};
    NumberInputConfig number = {};
    Vec<RetainedCallback> callbacks;
};

class RetainedStore {
  public:
    RetainedStore();
    RetainedStore(const RetainedStore&) = delete;
    RetainedStore& operator=(const RetainedStore&) = delete;
    ~RetainedStore();

    int Len() const { return entries.len; }
    uint32_t Checkpoint() const { return nextId; }

    EntityHandle CreateInput(bool textarea, Str placeholder, Str value,
                             int rows, App* app, EntityId owner,
                             void* application);
    EntityHandle CreateSlider(float min, float max, float step,
                              SliderScale scale, SliderValue value, App* app,
                              EntityId owner, void* application);
    EntityHandle CreateOtp(int length, Str value, bool masked, App* app,
                           EntityId owner, void* application);
    EntityHandle CreateFocus(App* app, EntityId owner, void* application);
    EntityHandle CreateVirtualScroll(App* app, EntityId owner,
                                     void* application);

    RetainedEntry* Find(EntityHandle handle) const;
    RetainedEntry* FindLocal(uint32_t id) const;
    bool AddCallback(EntityHandle handle, RetainedEvent event,
                     CallbackId callback, bool replace,
                     CallbackId* replaced = nullptr);
    bool Release(EntityHandle handle, Vec<CallbackId>* callbacks = nullptr);
    void ReleaseOwner(EntityId owner, Vec<CallbackId>* callbacks = nullptr);
    void Rollback(uint32_t checkpoint, Vec<CallbackId>* callbacks = nullptr);
    void Clear(Vec<CallbackId>* callbacks = nullptr);

  private:
    uint32_t storeId = 0;
    uint32_t nextId = 1;
    Vec<RetainedEntry*> entries;

    EntityHandle Push(RetainedEntry* entry);
    bool Belongs(EntityHandle handle, uint32_t* id) const;
    static void Destroy(RetainedEntry* entry, Vec<CallbackId>* callbacks);
};

} // namespace gpui::shell
#endif // GPUI_SHELL_RETAINED_H_

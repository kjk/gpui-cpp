#include "gpui/keymap.h"

namespace gpui {

// FNV-1a, the same one element ids are hashed with. An action is a name and
// nothing else, so two spellings of it are two actions — which is Rust's rule
// too, where two action types never compare equal.
static uint32_t HashName(Str s) {
    uint32_t h = 2166136261u;
    if (s.s) {
        for (int i = 0; i < s.len; i++) {
            h ^= (uint8_t)s.s[i];
            h *= 16777619u;
        }
    }
    // 0 is "no action", so a name that hashes there gets nudged off it.
    return h ? h : 1u;
}

uint32_t ActionOf(Str name) {
    return HashName(name);
}

uint32_t KeyContextOf(Str name) {
    return HashName(name);
}

// --- parsing --------------------------------------------------------------

struct NamedKey {
    const char* name;
    int vk;
};

// The names GPUI spells its keys with. The port's key codes are the Windows
// virtual keys, which is what a KeyEvent carries on all three platforms.
static const NamedKey kNamedKeys[] = {
    {"backspace", KeyBack},
    {"tab", KeyTab},
    {"enter", KeyReturn},
    {"return", KeyReturn},
    {"escape", KeyEscape},
    {"space", KeySpace},
    {"pageup", KeyPageUp},
    {"pagedown", KeyPageDown},
    {"end", KeyEnd},
    {"home", KeyHome},
    {"left", KeyLeft},
    {"up", KeyUp},
    {"right", KeyRight},
    {"down", KeyDown},
    {"delete", KeyDelete},
    // The OEM keys, which only Windows reports; a binding on one is simply
    // never matched on the other two, the way a binding on a key the keyboard
    // does not have is never matched anywhere.
    {"-", 189},
    {"=", 187},
    {"[", 219},
    {"]", 221},
    {"\\", 220},
    {";", 186},
    {"'", 222},
    {",", 188},
    {".", 190},
    {"/", 191},
    {"`", 192},
};

static bool NameEq(Str s, const char* lit) {
    int n = 0;
    while (lit[n]) {
        n++;
    }
    if (s.len != n) {
        return false;
    }
    for (int i = 0; i < n; i++) {
        char c = s.s[i];
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        if (c != lit[i]) {
            return false;
        }
    }
    return true;
}

static int VkForName(Str name) {
    if (name.len == 0) {
        return 0;
    }
    for (int i = 0; i < (int)(sizeof(kNamedKeys) / sizeof(kNamedKeys[0]));
         i++) {
        if (NameEq(name, kNamedKeys[i].name)) {
            return kNamedKeys[i].vk;
        }
    }
    // f1..f12, which are VK_F1 (112) upwards.
    if ((name.s[0] == 'f' || name.s[0] == 'F') && name.len >= 2 &&
        name.len <= 3) {
        int n = 0;
        for (int i = 1; i < name.len; i++) {
            if (name.s[i] < '0' || name.s[i] > '9') {
                return 0;
            }
            n = n * 10 + (name.s[i] - '0');
        }
        if (n >= 1 && n <= 12) {
            return 111 + n;
        }
        return 0;
    }
    if (name.len != 1) {
        return 0;
    }
    char c = name.s[0];
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 'A';
    }
    if (c >= 'A' && c <= 'Z') {
        return c;
    }
    if (c >= '0' && c <= '9') {
        return c;
    }
    return 0;
}

bool KeyChordParse(Str spec, KeyChord* out) {
    if (!out || spec.len == 0) {
        return false;
    }
    KeyChord c = {};
    int i = 0;
    // The modifiers are dash-separated prefixes; the last field is the key,
    // which may itself be "-" — so a dash is only a separator when something
    // follows it.
    while (i < spec.len) {
        int dash = -1;
        for (int j = i; j < spec.len - 1; j++) {
            if (spec.s[j] == '-') {
                dash = j;
                break;
            }
        }
        if (dash < 0) {
            break;
        }
        Str part = Str(spec.s + i, dash - i);
        if (NameEq(part, "ctrl") || NameEq(part, "cmd") ||
            NameEq(part, "secondary") || NameEq(part, "super") ||
            NameEq(part, "win")) {
            // The platform's shortcut key and Control are one modifier here:
            // macOS folds Command onto ctrl on the way in, so a keymap that
            // named them apart could never match the second one.
            c.ctrl = true;
        } else if (NameEq(part, "alt") || NameEq(part, "option")) {
            c.alt = true;
        } else if (NameEq(part, "shift")) {
            c.shift = true;
        } else {
            return false;
        }
        i = dash + 1;
    }
    c.vk = VkForName(Str(spec.s + i, spec.len - i));
    if (!c.vk) {
        return false;
    }
    *out = c;
    return true;
}

bool KeyChordEq(const KeyChord& a, const KeyChord& b) {
    return a.vk == b.vk && a.shift == b.shift && a.ctrl == b.ctrl &&
           a.alt == b.alt;
}

// --- the keymap -----------------------------------------------------------

struct BoundKey {
    KeyChord chord = {};
    uint32_t action = 0;
    uint32_t context = 0; // 0: anywhere
};

// More than any application binds. Rust grows a Vec; this is a fixed table so
// the keymap costs nothing until something is bound.
static const int kMaxBindings = 256;
static BoundKey gBindings[kMaxBindings];
static int gNBindings = 0;

void KeymapClear() {
    gNBindings = 0;
}

void KeymapBind(const KeyBinding* bindings, int n) {
    for (int i = 0; i < n && gNBindings < kMaxBindings; i++) {
        KeyChord chord = {};
        if (!bindings[i].stroke || !bindings[i].action ||
            !KeyChordParse(Str(bindings[i].stroke), &chord)) {
            continue;
        }
        BoundKey b;
        b.chord = chord;
        b.action = bindings[i].action;
        b.context =
            bindings[i].context ? KeyContextOf(Str(bindings[i].context)) : 0;
        gBindings[gNBindings++] = b;
    }
}

// The last binding for a chord wins, so the search runs backwards.
static uint32_t MatchIn(const KeyChord& chord, uint32_t context) {
    for (int i = gNBindings - 1; i >= 0; i--) {
        if (gBindings[i].context == context &&
            KeyChordEq(gBindings[i].chord, chord)) {
            return gBindings[i].action;
        }
    }
    return 0;
}

uint32_t KeymapMatch(const KeyChord& chord, const uint32_t* contexts,
                     int nContexts) {
    // Innermost context first: a component's own binding beats the one the
    // application put on the window.
    for (int i = 0; i < nContexts; i++) {
        uint32_t action = MatchIn(chord, contexts[i]);
        if (action) {
            return action;
        }
    }
    return MatchIn(chord, 0);
}

} // namespace gpui

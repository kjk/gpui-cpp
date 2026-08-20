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

static bool IsSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// What a name is made of, in a context or in a predicate: "Editor",
// "single_line", "popup-menu", "gpui.Editor".
static bool IsNameChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
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
    // The OEM keys. Only the brackets are reported on all three — the X11 and
    // Cocoa windows map those two, because a field binds them; a binding on
    // any of the rest is simply never matched off Windows, the way a binding
    // on a key the keyboard does not have is never matched anywhere.
    {"-", 189},
    {"=", 187},
    {"[", KeyLeftBracket},
    {"]", KeyRightBracket},
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

int KeyChordsParse(Str spec, KeyChord* out, int maxChords) {
    if (!out || maxChords <= 0 || spec.len == 0) {
        return 0;
    }
    int n = 0;
    int i = 0;
    while (i < spec.len) {
        while (i < spec.len && IsSpace(spec.s[i])) {
            i++;
        }
        int start = i;
        while (i < spec.len && !IsSpace(spec.s[i])) {
            i++;
        }
        if (i == start) {
            break;
        }
        // A sequence longer than the matcher can hold is a spec it cannot
        // read, the same as a key it does not know.
        if (n >= maxChords) {
            return 0;
        }
        if (!KeyChordParse(Str(spec.s + start, i - start), &out[n])) {
            return 0;
        }
        n++;
    }
    return n;
}

// --- key contexts ---------------------------------------------------------

// More than an element declares: "Editor mode=full vim_mode=normal" is
// already past what anything in the Rust tree writes.
static const int kMaxIdents = 4;
static const int kMaxPairs = 4;

struct ParsedContext {
    uint32_t id = 0;
    uint32_t idents[kMaxIdents] = {};
    int nIdents = 0;
    uint32_t keys[kMaxPairs] = {};
    uint32_t vals[kMaxPairs] = {};
    int nPairs = 0;
};

// Every distinct context spelling in the process. An element rebuilds its own
// out of the same literal every frame, so this fills once and then only ever
// answers.
static const int kMaxContexts = 128;
static ParsedContext gContexts[kMaxContexts];
static int gNContexts = 0;

static void ParseContextInto(Str spec, ParsedContext* c) {
    int i = 0;
    while (i < spec.len) {
        while (i < spec.len && IsSpace(spec.s[i])) {
            i++;
        }
        int s0 = i;
        while (i < spec.len && IsNameChar(spec.s[i])) {
            i++;
        }
        if (i == s0) {
            // Nothing a context is made of. Step over it rather than stop:
            // the id is the whole spelling either way.
            i++;
            continue;
        }
        uint32_t name = HashName(Str(spec.s + s0, i - s0));
        int j = i;
        while (j < spec.len && IsSpace(spec.s[j])) {
            j++;
        }
        if (j < spec.len && spec.s[j] == '=') {
            j++;
            while (j < spec.len && IsSpace(spec.s[j])) {
                j++;
            }
            int v0 = j;
            while (j < spec.len && IsNameChar(spec.s[j])) {
                j++;
            }
            if (j > v0 && c->nPairs < kMaxPairs) {
                c->keys[c->nPairs] = name;
                c->vals[c->nPairs] = HashName(Str(spec.s + v0, j - v0));
                c->nPairs++;
            }
            i = j;
            continue;
        }
        if (c->nIdents < kMaxIdents) {
            c->idents[c->nIdents++] = name;
        }
    }
}

uint32_t KeyContextOf(Str name) {
    uint32_t id = HashName(name);
    for (int i = 0; i < gNContexts; i++) {
        if (gContexts[i].id == id) {
            return id;
        }
    }
    if (gNContexts < kMaxContexts) {
        ParsedContext c;
        c.id = id;
        ParseContextInto(name, &c);
        gContexts[gNContexts++] = c;
    }
    return id;
}

static const ParsedContext* ContextFor(uint32_t id) {
    for (int i = 0; i < gNContexts; i++) {
        if (gContexts[i].id == id) {
            return &gContexts[i];
        }
    }
    return nullptr;
}

static bool ContextHasIdent(uint32_t id, uint32_t ident) {
    const ParsedContext* c = ContextFor(id);
    if (!c) {
        // The table filled. A context that is one plain name hashes to that
        // name, so the common one still resolves.
        return ident == id;
    }
    for (int i = 0; i < c->nIdents; i++) {
        if (c->idents[i] == ident) {
            return true;
        }
    }
    return false;
}

static bool ContextValue(uint32_t id, uint32_t key, uint32_t* val) {
    const ParsedContext* c = ContextFor(id);
    if (!c) {
        return false;
    }
    for (int i = 0; i < c->nPairs; i++) {
        if (c->keys[i] == key) {
            *val = c->vals[i];
            return true;
        }
    }
    return false;
}

// --- context predicates ---------------------------------------------------

// KeyBindingContextPredicate. Ident carries the name; Eq and Neq the key and
// the value; the rest their operands.
enum class PredOp : uint8_t {
    Ident,
    Eq,
    Neq,
    Not,
    And,
    Or,
    Child
};

struct PredNode {
    PredOp op = PredOp::Ident;
    uint32_t a = 0;
    uint32_t b = 0;
    int16_t l = -1;
    int16_t r = -1;
};

// Predicates live as long as the bindings that hold them, so the pool is
// emptied with the keymap.
static const int kMaxPreds = 256;
static PredNode gPreds[kMaxPreds];
static int gNPreds = 0;

struct PredParser {
    Str s;
    int i = 0;
    bool bad = false;
};

static void PredSkipWs(PredParser* p) {
    while (p->i < p->s.len && IsSpace(p->s.s[p->i])) {
        p->i++;
    }
}

static int PredAlloc(PredParser* p, PredOp op, uint32_t a, uint32_t b, int l,
                     int r) {
    if (gNPreds >= kMaxPreds) {
        p->bad = true;
        return -1;
    }
    PredNode& n = gPreds[gNPreds];
    n.op = op;
    n.a = a;
    n.b = b;
    n.l = (int16_t)l;
    n.r = (int16_t)r;
    return gNPreds++;
}

static int ParsePredExpr(PredParser* p, int minPrec);

static int ParsePredPrimary(PredParser* p) {
    PredSkipWs(p);
    if (p->i >= p->s.len) {
        p->bad = true;
        return -1;
    }
    char c = p->s.s[p->i];
    // `!` is negation unless it is the first half of `!=`, which is an
    // operator and belongs to the caller.
    if (c == '!' && !(p->i + 1 < p->s.len && p->s.s[p->i + 1] == '=')) {
        p->i++;
        int inner = ParsePredPrimary(p);
        if (p->bad) {
            return -1;
        }
        return PredAlloc(p, PredOp::Not, 0, 0, inner, -1);
    }
    if (c == '(') {
        p->i++;
        int e = ParsePredExpr(p, 1);
        PredSkipWs(p);
        if (p->bad || p->i >= p->s.len || p->s.s[p->i] != ')') {
            p->bad = true;
            return -1;
        }
        p->i++;
        return e;
    }
    int s0 = p->i;
    while (p->i < p->s.len && IsNameChar(p->s.s[p->i])) {
        p->i++;
    }
    if (p->i == s0) {
        p->bad = true;
        return -1;
    }
    return PredAlloc(p, PredOp::Ident, HashName(Str(p->s.s + s0, p->i - s0)), 0,
                     -1, -1);
}

struct PredOpTok {
    PredOp op;
    int prec;
    int len;
};

// Rust's precedence: the child operator binds loosest, so "Workspace > Editor
// && mode == full" is a Workspace over an Editor that has the mode, not a
// child of a conjunction.
static bool PeekPredOp(PredParser* p, PredOpTok* out) {
    PredSkipWs(p);
    int n = p->s.len - p->i;
    const char* s = p->s.s + p->i;
    if (n >= 2 && s[0] == '&' && s[1] == '&') {
        *out = {PredOp::And, 3, 2};
        return true;
    }
    if (n >= 2 && s[0] == '|' && s[1] == '|') {
        *out = {PredOp::Or, 2, 2};
        return true;
    }
    if (n >= 2 && s[0] == '=' && s[1] == '=') {
        *out = {PredOp::Eq, 4, 2};
        return true;
    }
    if (n >= 2 && s[0] == '!' && s[1] == '=') {
        *out = {PredOp::Neq, 4, 2};
        return true;
    }
    if (n >= 1 && s[0] == '>') {
        *out = {PredOp::Child, 1, 1};
        return true;
    }
    return false;
}

static int ParsePredExpr(PredParser* p, int minPrec) {
    int left = ParsePredPrimary(p);
    if (p->bad) {
        return -1;
    }
    for (;;) {
        PredOpTok t = {};
        if (!PeekPredOp(p, &t) || t.prec < minPrec) {
            break;
        }
        p->i += t.len;
        int right = ParsePredExpr(p, t.prec + 1);
        if (p->bad) {
            return -1;
        }
        if (t.op == PredOp::Eq || t.op == PredOp::Neq) {
            // Both sides of a comparison are plain names: `mode == full`.
            if (gPreds[left].op != PredOp::Ident ||
                gPreds[right].op != PredOp::Ident) {
                p->bad = true;
                return -1;
            }
            left = PredAlloc(p, t.op, gPreds[left].a, gPreds[right].a, -1, -1);
        } else {
            left = PredAlloc(p, t.op, 0, 0, left, right);
        }
        if (p->bad) {
            return -1;
        }
    }
    return left;
}

// The root of the parsed predicate, or -1 for one that cannot be read — which
// drops the binding, the same as a chord that cannot be read.
static int PredParse(Str spec, bool* ok) {
    PredParser p;
    p.s = spec;
    int root = ParsePredExpr(&p, 1);
    PredSkipWs(&p);
    if (p.bad || p.i != p.s.len || root < 0) {
        *ok = false;
        return -1;
    }
    *ok = true;
    return root;
}

// `contexts` is the ancestry from the level being tried outwards, innermost
// first; the innermost is the one an identifier or a comparison is read
// against, which is Rust's `contexts.last()`.
static bool PredEval(int ix, const uint32_t* contexts, int n) {
    if (ix < 0 || n <= 0) {
        // Rust reads nothing out of an empty stack, negation included.
        return false;
    }
    const PredNode& p = gPreds[ix];
    uint32_t val = 0;
    switch (p.op) {
        case PredOp::Ident:
            return ContextHasIdent(contexts[0], p.a);
        case PredOp::Eq:
            return ContextValue(contexts[0], p.a, &val) && val == p.b;
        case PredOp::Neq:
            // A key the context does not carry is not equal to the value,
            // which is `context.get(k) != Some(v)`.
            return !ContextValue(contexts[0], p.a, &val) || val != p.b;
        case PredOp::Not:
            return !PredEval(p.l, contexts, n);
        case PredOp::And:
            return PredEval(p.l, contexts, n) && PredEval(p.r, contexts, n);
        case PredOp::Or:
            return PredEval(p.l, contexts, n) || PredEval(p.r, contexts, n);
        case PredOp::Child:
            // The right half against this level, the left half against what
            // encloses it.
            return PredEval(p.r, contexts, n) &&
                   PredEval(p.l, contexts + 1, n - 1);
    }
    return false;
}

// --- the keymap -----------------------------------------------------------

struct BoundKey {
    KeyChord strokes[kMaxStrokes] = {};
    int nStrokes = 0;
    uint32_t action = 0;
    int pred = -1; // -1: anywhere
};

// More than any application binds. Rust grows a Vec; this is a fixed table so
// the keymap costs nothing until something is bound.
static const int kMaxBindings = 256;
static BoundKey gBindings[kMaxBindings];
static int gNBindings = 0;

// The chords of a sequence that has begun but not finished. Rust keeps the
// same on the window's matcher; the keymap is process-wide here, and so is
// this.
static KeyChord gPending[kMaxStrokes];
static int gNPending = 0;

void KeymapClearPending() {
    gNPending = 0;
}

bool KeymapPending() {
    return gNPending > 0;
}

void KeymapClear() {
    gNBindings = 0;
    gNPreds = 0;
    gNPending = 0;
}

void KeymapBind(const KeyBinding* bindings, int n) {
    for (int i = 0; i < n && gNBindings < kMaxBindings; i++) {
        BoundKey b;
        if (!bindings[i].stroke || !bindings[i].action) {
            continue;
        }
        b.nStrokes =
            KeyChordsParse(Str(bindings[i].stroke), b.strokes, kMaxStrokes);
        if (!b.nStrokes) {
            continue;
        }
        if (bindings[i].context) {
            bool ok = false;
            b.pred = PredParse(Str(bindings[i].context), &ok);
            if (!ok) {
                continue;
            }
        }
        b.action = bindings[i].action;
        gBindings[gNBindings++] = b;
    }
}

// One level of the stack, or — with nothing passed — the unscoped pass, which
// only bindings with no predicate take part in. The last binding for a chord
// wins, so the search runs backwards.
static uint32_t MatchIn(const uint32_t* contexts, int n, bool* pending) {
    for (int i = gNBindings - 1; i >= 0; i--) {
        const BoundKey& b = gBindings[i];
        bool applies = n > 0 ? (b.pred >= 0 && PredEval(b.pred, contexts, n))
                             : (b.pred < 0);
        if (!applies || b.nStrokes < gNPending) {
            continue;
        }
        bool begins = true;
        for (int k = 0; k < gNPending; k++) {
            if (!KeyChordEq(b.strokes[k], gPending[k])) {
                begins = false;
                break;
            }
        }
        if (!begins) {
            continue;
        }
        if (b.nStrokes == gNPending) {
            return b.action;
        }
        // Begun but not finished. Noted rather than returned: a binding that
        // is complete beats one that is only started, which is what makes
        // "ctrl-k" fire even with "ctrl-k ctrl-o" bound beside it.
        *pending = true;
    }
    return 0;
}

KeyMatch KeymapMatch(const KeyChord& chord, const uint32_t* contexts,
                     int nContexts) {
    KeyMatch m;
    if (gNPending >= kMaxStrokes) {
        gNPending = 0;
    }
    gPending[gNPending++] = chord;

    bool pending = false;
    for (int lvl = 0; lvl < nContexts; lvl++) {
        uint32_t action = MatchIn(contexts + lvl, nContexts - lvl, &pending);
        if (action) {
            gNPending = 0;
            m.action = action;
            return m;
        }
    }
    uint32_t action = MatchIn(nullptr, 0, &pending);
    if (action) {
        gNPending = 0;
        m.action = action;
        return m;
    }
    if (pending) {
        m.pending = true;
        return m;
    }
    // Neither finished nor continued anything: what was held is dropped,
    // which is Rust's matcher clearing its pending keystrokes.
    gNPending = 0;
    return m;
}

} // namespace gpui

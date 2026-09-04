#include "html5ever/html5ever.h"

namespace html5ever {

using namespace base;

// html5ever's generated atom sets become sequential strings here. They are
// read only at structural boundaries, where a linear walk is smaller than a
// pointer table and insignificant beside allocating the node.
static const char kVoidElements[] =
    "area\0base\0basefont\0bgsound\0br\0col\0embed\0frame\0hr\0img\0input\0"
    "keygen\0link\0meta\0param\0source\0track\0wbr\0";
static const char kRawElements[] =
    "iframe\0noembed\0noframes\0script\0style\0xmp\0";
static const char kRcdataElements[] = "textarea\0title\0";
static const char kFormattingElements[] =
    "a\0b\0big\0code\0em\0font\0i\0nobr\0s\0small\0strike\0strong\0tt\0u\0";
static const char kBlockElements[] =
    "address\0article\0aside\0blockquote\0center\0details\0dialog\0dir\0div\0"
    "dl\0fieldset\0figcaption\0figure\0footer\0form\0h1\0h2\0h3\0h4\0h5\0"
    "h6\0header\0hgroup\0hr\0main\0menu\0nav\0ol\0p\0pre\0search\0section\0"
    "summary\0table\0ul\0";
static const char kHeadElements[] =
    "base\0basefont\0bgsound\0link\0meta\0noframes\0script\0style\0template\0"
    "title\0";
static const char kTableParts[] =
    "caption\0col\0colgroup\0tbody\0td\0tfoot\0th\0thead\0tr\0";

static bool IsSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

static bool IsAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool IsDigit(char c) {
    return c >= '0' && c <= '9';
}

static bool IsNameChar(char c) {
    return IsAlpha(c) || IsDigit(c) || c == '-' || c == '_' || c == ':';
}

static bool In(SeqStrings set, Str value) {
    return SeqStrIndexIS(set, value) >= 0;
}

static Str LowerCopy(Arena* a, Str value) {
    Str result = StrDup(a, value);
    StrLowerAscii(result.s);
    return result;
}

// The frequently occurring named references. Numeric references cover the
// rest of the compact implementation; the full markdown build already owns
// the generated 2,125-name database used by TextView's HTML projection.
// Keeping this tokenizer's standalone table as SeqStrings avoids a pointer
// relocation per spelling and per value.
static const char kEntityNames[] =
    "AMP\0AElig\0COPY\0CounterClockwiseContourIntegral\0GT\0LT\0QUOT\0REG\0"
    "amp\0apos\0cent\0copy\0euro\0gt\0hellip\0laquo\0ldquo\0lsquo\0lt\0"
    "mdash\0middot\0nbsp\0ndash\0pound\0quot\0raquo\0rdquo\0reg\0rsquo\0"
    "trade\0yen\0";
static const char kEntityValues[] =
    "&\0Æ\0©\0∳\0>\0<\0\"\0®\0&\0'\0¢\0©\0€\0>\0…\0«\0“\0‘\0<\0—\0"
    "·\0 \0–\0£\0\"\0»\0”\0®\0’\0™\0¥\0";

static Str NamedEntity(Str name) {
    int ix = SeqStrIndex(kEntityNames, name);
    return ix < 0 ? Str{} : SeqStrByIndex(kEntityValues, ix);
}

static uint32_t NumericEntity(Str value, int radix) {
    uint32_t cp = 0;
    bool any = false;
    for (int i = 0; i < value.len; i++) {
        char c = value.s[i];
        uint32_t digit = 0;
        if (IsDigit(c)) {
            digit = (uint32_t)(c - '0');
        } else if (radix == 16 && c >= 'a' && c <= 'f') {
            digit = (uint32_t)(c - 'a' + 10);
        } else if (radix == 16 && c >= 'A' && c <= 'F') {
            digit = (uint32_t)(c - 'A' + 10);
        } else {
            break;
        }
        any = true;
        if (cp > 0x10ffffu / (uint32_t)radix) return 0xfffd;
        cp = cp * (uint32_t)radix + digit;
    }
    if (!any || cp == 0 || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) {
        return 0xfffd;
    }
    // The HTML numeric-character-reference replacement table.
    static const uint16_t controls[32] = {
        0x20ac, 0x0081, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021,
        0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008d, 0x017d, 0x008f,
        0x0090, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
        0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0x009d, 0x017e, 0x0178,
    };
    if (cp >= 0x80 && cp <= 0x9f) cp = controls[cp - 0x80];
    return cp;
}

static int EncodeUtf8(char* out, uint32_t cp) {
    if (cp <= 0x7f) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp <= 0x7ff) {
        out[0] = (char)(0xc0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3f));
        return 2;
    }
    if (cp <= 0xffff) {
        out[0] = (char)(0xe0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
        out[2] = (char)(0x80 | (cp & 0x3f));
        return 3;
    }
    out[0] = (char)(0xf0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3f));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3f));
    out[3] = (char)(0x80 | (cp & 0x3f));
    return 4;
}

static void AppendCp(Arena* a, StrBuilder& out, uint32_t cp) {
    char bytes[4];
    int n = EncodeUtf8(bytes, cp);
    StrBuilderAppend(a, out, Str(bytes, n));
}

static Str Decode(Arena* a, Str value, bool attribute) {
    StrBuilder out;
    StrBuilderReserve(a, out, value.len);
    for (int i = 0; i < value.len;) {
        if (value.s[i] != '&') {
            char c = value.s[i++] == '\r' ? '\n' : value.s[i - 1];
            if (c == '\n' && i < value.len && value.s[i] == '\n' &&
                value.s[i - 1] == '\r') {
                i++;
            }
            if (c == 0) {
                AppendCp(a, out, 0xfffd);
            } else {
                StrBuilderAppendChar(a, out, c);
            }
            continue;
        }
        int start = i++;
        if (i < value.len && value.s[i] == '#') {
            i++;
            int radix = 10;
            if (i < value.len && (value.s[i] == 'x' || value.s[i] == 'X')) {
                radix = 16;
                i++;
            }
            int digits = i;
            while (
                i < value.len &&
                (IsDigit(value.s[i]) ||
                 (radix == 16 && ((value.s[i] >= 'a' && value.s[i] <= 'f') ||
                                  (value.s[i] >= 'A' && value.s[i] <= 'F'))))) {
                i++;
            }
            if (digits == i) {
                StrBuilderAppendChar(a, out, '&');
                i = start + 1;
                continue;
            }
            uint32_t cp =
                NumericEntity(Str(value.s + digits, i - digits), radix);
            if (i < value.len && value.s[i] == ';') i++;
            AppendCp(a, out, cp);
            continue;
        }
        int end = i;
        while (end < value.len && IsAlpha(value.s[end]) && end - i < 31) end++;
        if (end < value.len && IsDigit(value.s[end])) end++;
        int matched = -1;
        Str decoded = {};
        for (int n = end - i; n > 0; n--) {
            decoded = NamedEntity(Str(value.s + i, n));
            if (decoded.s) {
                matched = n;
                break;
            }
        }
        bool semi = matched > 0 && i + matched < value.len &&
                    value.s[i + matched] == ';';
        if (matched < 0 ||
            (attribute && !semi && i + matched < value.len &&
             (IsDigit(value.s[i + matched]) || IsAlpha(value.s[i + matched]) ||
              value.s[i + matched] == '='))) {
            StrBuilderAppendChar(a, out, '&');
            i = start + 1;
            continue;
        }
        StrBuilderAppend(a, out, decoded);
        i += matched + (semi ? 1 : 0);
    }
    return StrBuilderTakeStr(a, out);
}

struct Scanner {
    Arena* a = nullptr;
    Str source = {};
    int at = 0;
    int line = 1;
    TokenSink sink = nullptr;
    void* user = nullptr;
    TokenizerOptions options = {};
    Str rawName = {};
    bool rcdata = false;
};

static void Emit(Scanner* s, const Token& token) {
    if (s->sink) s->sink(s->user, &token);
}

static void Error(Scanner* s, const char* message) {
    if (!s->options.exactErrors) return;
    Token token;
    token.kind = TokenKind::ParseError;
    token.data = Str((char*)message);
    token.line = s->line;
    Emit(s, token);
}

static void SkipSpace(Scanner* s) {
    while (s->at < s->source.len && IsSpace(s->source.s[s->at])) {
        if (s->source.s[s->at++] == '\n') s->line++;
    }
}

static Str ScanName(Scanner* s) {
    int start = s->at;
    while (s->at < s->source.len && IsNameChar(s->source.s[s->at])) s->at++;
    return LowerCopy(s->a, Str(s->source.s + start, s->at - start));
}

static Attribute* ScanAttrs(Scanner* s, bool* selfClosing) {
    Attribute* first = nullptr;
    Attribute* last = nullptr;
    *selfClosing = false;
    for (;;) {
        SkipSpace(s);
        if (s->at >= s->source.len) return first;
        char c = s->source.s[s->at];
        if (c == '>') {
            s->at++;
            return first;
        }
        if (c == '/' && s->at + 1 < s->source.len &&
            s->source.s[s->at + 1] == '>') {
            s->at += 2;
            *selfClosing = true;
            return first;
        }
        int nameStart = s->at;
        while (s->at < s->source.len && !IsSpace(s->source.s[s->at]) &&
               s->source.s[s->at] != '=' && s->source.s[s->at] != '>' &&
               s->source.s[s->at] != '/') {
            s->at++;
        }
        if (nameStart == s->at) {
            Error(s, "unexpected byte in tag");
            s->at++;
            continue;
        }
        Str name =
            LowerCopy(s->a, Str(s->source.s + nameStart, s->at - nameStart));
        SkipSpace(s);
        Str value = StrDup(s->a, StrL(""));
        if (s->at < s->source.len && s->source.s[s->at] == '=') {
            s->at++;
            SkipSpace(s);
            int start = s->at;
            if (s->at < s->source.len &&
                (s->source.s[s->at] == '\'' || s->source.s[s->at] == '"')) {
                char quote = s->source.s[s->at++];
                start = s->at;
                while (s->at < s->source.len && s->source.s[s->at] != quote) {
                    if (s->source.s[s->at++] == '\n') s->line++;
                }
                value =
                    Decode(s->a, Str(s->source.s + start, s->at - start), true);
                if (s->at < s->source.len) s->at++;
            } else {
                while (s->at < s->source.len && !IsSpace(s->source.s[s->at]) &&
                       s->source.s[s->at] != '>') {
                    s->at++;
                }
                value =
                    Decode(s->a, Str(s->source.s + start, s->at - start), true);
            }
        }
        bool duplicate = false;
        for (Attribute* at = first; at; at = at->next) {
            if (StrEq(at->name, name)) duplicate = true;
        }
        if (duplicate) {
            Error(s, "duplicate attribute");
            continue;
        }
        Attribute* attr = ArenaNew<Attribute>(s->a);
        attr->name = name;
        attr->value = value;
        if (last)
            last->next = attr;
        else
            first = attr;
        last = attr;
    }
}

static int FindRawClose(const Scanner* s) {
    for (int i = s->at; i + 2 + s->rawName.len <= s->source.len; i++) {
        if (s->source.s[i] != '<' || s->source.s[i + 1] != '/') continue;
        if (!StrEqI(Str(s->source.s + i + 2, s->rawName.len), s->rawName)) {
            continue;
        }
        int end = i + 2 + s->rawName.len;
        if (end >= s->source.len || IsSpace(s->source.s[end]) ||
            s->source.s[end] == '>') {
            return i;
        }
    }
    return s->source.len;
}

static void TokenizeRun(Scanner* s) {
    if (s->options.discardBom && s->source.len >= 3 &&
        (uint8_t)s->source.s[0] == 0xef && (uint8_t)s->source.s[1] == 0xbb &&
        (uint8_t)s->source.s[2] == 0xbf) {
        s->at = 3;
    }
    while (s->at < s->source.len) {
        if (s->rawName.s) {
            int end = FindRawClose(s);
            if (end > s->at) {
                Token text;
                text.kind = TokenKind::Character;
                Str raw(s->source.s + s->at, end - s->at);
                text.data =
                    s->rcdata ? Decode(s->a, raw, false) : StrDup(s->a, raw);
                text.line = s->line;
                for (int i = s->at; i < end; i++) {
                    if (s->source.s[i] == '\n') s->line++;
                }
                s->at = end;
                Emit(s, text);
                continue;
            }
            s->rawName = {};
            s->rcdata = false;
        }
        if (s->source.s[s->at] != '<') {
            int start = s->at;
            while (s->at < s->source.len && s->source.s[s->at] != '<') {
                if (s->source.s[s->at++] == '\n') s->line++;
            }
            Token text;
            text.kind = TokenKind::Character;
            text.data =
                Decode(s->a, Str(s->source.s + start, s->at - start), false);
            text.line = s->line;
            Emit(s, text);
            continue;
        }
        int tokenLine = s->line;
        if (s->at + 3 < s->source.len &&
            StrEq(Str(s->source.s + s->at, 4), StrL("<!--"))) {
            s->at += 4;
            int start = s->at;
            while (s->at + 2 < s->source.len &&
                   !(s->source.s[s->at] == '-' &&
                     s->source.s[s->at + 1] == '-' &&
                     s->source.s[s->at + 2] == '>')) {
                if (s->source.s[s->at++] == '\n') s->line++;
            }
            Token comment;
            comment.kind = TokenKind::Comment;
            comment
                .data = StrDup(s->a, Str(s->source.s + start, s->at - start));
            comment.line = tokenLine;
            if (s->at + 2 < s->source.len)
                s->at += 3;
            else
                Error(s, "eof in comment");
            Emit(s, comment);
            continue;
        }
        if (s->at + 2 < s->source.len && s->source.s[s->at + 1] == '!') {
            int start = s->at + 2;
            s->at = start;
            while (s->at < s->source.len && s->source.s[s->at] != '>') {
                s->at++;
            }
            Str body = StrTrimAscii(Str(s->source.s + start, s->at - start));
            if (s->at < s->source.len) s->at++;
            Token token;
            token.kind = TokenKind::Doctype;
            token.line = tokenLine;
            if (StrStartsWithI(body, "doctype")) {
                body = StrTrimAscii(Str(body.s + 7, body.len - 7));
                int n = 0;
                while (n < body.len && !IsSpace(body.s[n])) n++;
                token.name = LowerCopy(s->a, Str(body.s, n));
                token.forceQuirks = !StrEqI(token.name, "html");
            } else {
                token.kind = TokenKind::Comment;
                token.data = StrDup(s->a, body);
            }
            Emit(s, token);
            continue;
        }
        if (s->at + 1 < s->source.len && s->source.s[s->at + 1] == '/') {
            s->at += 2;
            SkipSpace(s);
            Token token;
            token.kind = TokenKind::EndTag;
            token.name = ScanName(s);
            token.line = tokenLine;
            while (s->at < s->source.len && s->source.s[s->at] != '>') s->at++;
            if (s->at < s->source.len) s->at++;
            Emit(s, token);
            continue;
        }
        if (s->at + 1 < s->source.len && IsAlpha(s->source.s[s->at + 1])) {
            s->at++;
            Token token;
            token.kind = TokenKind::StartTag;
            token.name = ScanName(s);
            token.line = tokenLine;
            token.attrs = ScanAttrs(s, &token.selfClosing);
            Emit(s, token);
            if (!token.selfClosing && (In(kRawElements, token.name) ||
                                       In(kRcdataElements, token.name))) {
                s->rawName = token.name;
                s->rcdata = In(kRcdataElements, token.name);
            }
            continue;
        }
        Token text;
        text.kind = TokenKind::Character;
        text.data = StrDup(s->a, Str(s->source.s + s->at, 1));
        text.line = tokenLine;
        s->at++;
        Emit(s, text);
    }
    Token eof;
    eof.kind = TokenKind::Eof;
    eof.line = s->line;
    Emit(s, eof);
}

void Tokenize(Arena* a, Str source, TokenSink sink, void* user,
              TokenizerOptions options) {
    if (!a || !sink) return;
    Scanner scanner;
    scanner.a = a;
    scanner.source = source;
    scanner.sink = sink;
    scanner.user = user;
    scanner.options = options;
    TokenizeRun(&scanner);
}

static Node* NewNode(Arena* a, NodeKind kind, Str name = {}) {
    Node* node = ArenaNew<Node>(a);
    node->kind = kind;
    node->name = name;
    return node;
}

static void Append(Node* parent, Node* child) {
    child->parent = parent;
    child->next = nullptr;
    if (parent->last)
        parent->last->next = child;
    else
        parent->first = child;
    parent->last = child;
}

static void InsertBefore(Node* before, Node* child) {
    Node* parent = before ? before->parent : nullptr;
    if (!parent) return;
    child->parent = parent;
    if (parent->first == before) {
        child->next = before;
        parent->first = child;
        return;
    }
    Node* prev = parent->first;
    while (prev && prev->next != before) prev = prev->next;
    if (!prev) return;
    prev->next = child;
    child->next = before;
}

static Attribute* CloneAttrs(Arena* a, const Attribute* attrs) {
    Attribute* first = nullptr;
    Attribute* last = nullptr;
    for (; attrs; attrs = attrs->next) {
        Attribute* copy = ArenaNew<Attribute>(a);
        *copy = *attrs;
        copy->next = nullptr;
        if (last)
            last->next = copy;
        else
            first = copy;
        last = copy;
    }
    return first;
}

struct Builder {
    Arena* a = nullptr;
    ParseOptions options = {};
    Node* doc = nullptr;
    Node* html = nullptr;
    Node* head = nullptr;
    Node* body = nullptr;
    ArenaVec<Node*> open{};
    bool fragment = false;
    Str context = {};
};

static Node* Current(Builder* b) {
    return b->open.len ? b->open[b->open.len - 1] : b->doc;
}

static int OpenIndex(Builder* b, Str name) {
    for (int i = b->open.len - 1; i >= 0; i--) {
        if (StrEqI(b->open[i]->name, name)) return i;
    }
    return -1;
}

static bool HasOpen(Builder* b, const char* name) {
    return OpenIndex(b, Str((char*)name)) >= 0;
}

static Node* Element(Builder* b, Str name, Attribute* attrs,
                     Namespace ns = Namespace::Html) {
    Node* node = NewNode(b->a, NodeKind::Element, StrDup(b->a, name));
    node->attrs = CloneAttrs(b->a, attrs);
    node->ns = ns;
    return node;
}

static Node* EnsureWrapper(Builder* b, Node** slot, const char* name,
                           Node* parent) {
    if (*slot) return *slot;
    *slot = Element(b, Str((char*)name), nullptr);
    (*slot)->implicit = true;
    Append(parent, *slot);
    return *slot;
}

static Node* Body(Builder* b) {
    if (b->fragment) return b->doc;
    EnsureWrapper(b, &b->html, "html", b->doc);
    EnsureWrapper(b, &b->head, "head", b->html);
    return EnsureWrapper(b, &b->body, "body", b->html);
}

static bool AllSpace(Str value) {
    for (int i = 0; i < value.len; i++) {
        if (!IsSpace(value.s[i])) return false;
    }
    return true;
}

static Node* TableInScope(Builder* b) {
    for (int i = b->open.len - 1; i >= 0; i--) {
        if (StrEqI(b->open[i]->name, "table")) return b->open[i];
    }
    return nullptr;
}

static bool TableAllows(Str parent, Str child) {
    if (StrEqI(parent, "table")) {
        return In(kTableParts, child) || StrEqI(child, "style") ||
               StrEqI(child, "script") || StrEqI(child, "template");
    }
    if (StrEqI(parent, "tbody") || StrEqI(parent, "thead") ||
        StrEqI(parent, "tfoot")) {
        return StrEqI(child, "tr");
    }
    if (StrEqI(parent, "tr")) {
        return StrEqI(child, "td") || StrEqI(child, "th");
    }
    return true;
}

static Node* InsertionParent(Builder* b, Str child, bool textIsSpace = false) {
    Node* current = Current(b);
    Node* table = TableInScope(b);
    if (table && !TableAllows(current->name, child) && !textIsSpace) {
        return table->parent ? table->parent : current;
    }
    return current == b->doc ? Body(b) : current;
}

static void AppendText(Builder* b, Str data) {
    if (data.len <= 0) return;
    Node* parent = InsertionParent(b, {}, AllSpace(data));
    Node* table = TableInScope(b);
    Node* text = NewNode(b->a, NodeKind::Text);
    text->data = StrDup(b->a, data);
    if (table && parent == table->parent && !AllSpace(data)) {
        InsertBefore(table, text);
    } else if (parent->last && parent->last->kind == NodeKind::Text) {
        ArenaStr joined = ArenaStrDup(b->a, parent->last->data);
        joined = ArenaStrAppend(b->a, joined, data);
        parent->last->data = ArenaStrGet(b->a, joined);
    } else {
        Append(parent, text);
    }
}

static bool ClosesP(Str name) {
    return In(kBlockElements, name) || StrEqI(name, "listing");
}

static void CloseNamed(Builder* b, const char* name) {
    int at = OpenIndex(b, Str((char*)name));
    if (at >= 0) b->open.Truncate(at);
}

static void CloseImplied(Builder* b, Str name) {
    if (ClosesP(name) && HasOpen(b, "p")) CloseNamed(b, "p");
    if (StrEqI(name, "li")) {
        int at = OpenIndex(b, StrL("li"));
        if (at >= 0) b->open.Truncate(at);
    }
    if (StrEqI(name, "dt") || StrEqI(name, "dd")) {
        int dt = OpenIndex(b, StrL("dt"));
        int dd = OpenIndex(b, StrL("dd"));
        int at = dt > dd ? dt : dd;
        if (at >= 0) b->open.Truncate(at);
    }
    if (StrEqI(name, "tr")) {
        int at = OpenIndex(b, StrL("tr"));
        if (at >= 0) b->open.Truncate(at);
    }
    if (StrEqI(name, "td") || StrEqI(name, "th")) {
        int td = OpenIndex(b, StrL("td"));
        int th = OpenIndex(b, StrL("th"));
        int at = td > th ? td : th;
        if (at >= 0) b->open.Truncate(at);
    }
    if (name.len == 2 && name.s[0] == 'h' && name.s[1] >= '1' &&
        name.s[1] <= '6') {
        for (int i = b->open.len - 1; i >= 0; i--) {
            Str n = b->open[i]->name;
            if (n.len == 2 && n.s[0] == 'h' && n.s[1] >= '1' && n.s[1] <= '6') {
                b->open.Truncate(i);
                break;
            }
        }
    }
}

static void MergeAttrs(Arena* a, Node* node, Attribute* attrs) {
    for (; attrs; attrs = attrs->next) {
        bool exists = false;
        for (Attribute* at = node->attrs; at; at = at->next) {
            if (StrEq(at->name, attrs->name)) exists = true;
        }
        if (exists) continue;
        Attribute* copy = ArenaNew<Attribute>(a);
        *copy = *attrs;
        copy->next = node->attrs;
        node->attrs = copy;
    }
}

static Node* PushElement(Builder* b, const Token* token,
                         Namespace ns = Namespace::Html) {
    Node* parent = InsertionParent(b, token->name);
    Node* node = Element(b, token->name, token->attrs, ns);
    Node* table = TableInScope(b);
    if (table && parent == table->parent &&
        !TableAllows(Current(b)->name, token->name)) {
        InsertBefore(table, node);
    } else {
        Append(parent, node);
    }
    if (!token->selfClosing && !In(kVoidElements, token->name)) {
        b->open.Append(b->a, node);
    }
    return node;
}

static void StartTag(Builder* b, const Token* token) {
    Str name = token->name;
    if (!b->fragment && StrEqI(name, "html")) {
        Node* html = EnsureWrapper(b, &b->html, "html", b->doc);
        html->implicit = false;
        MergeAttrs(b->a, html, token->attrs);
        if (b->open.len == 0) b->open.Append(b->a, html);
        return;
    }
    if (!b->fragment && StrEqI(name, "head")) {
        EnsureWrapper(b, &b->html, "html", b->doc);
        Node* head = EnsureWrapper(b, &b->head, "head", b->html);
        head->implicit = false;
        MergeAttrs(b->a, head, token->attrs);
        if (!HasOpen(b, "head")) b->open.Append(b->a, head);
        return;
    }
    if (!b->fragment && StrEqI(name, "body")) {
        Body(b)->implicit = false;
        MergeAttrs(b->a, b->body, token->attrs);
        while (b->open.len && b->open[b->open.len - 1] != b->html)
            b->open.Pop();
        if (!HasOpen(b, "html")) b->open.Append(b->a, b->html);
        b->open.Append(b->a, b->body);
        return;
    }
    if (!b->fragment && In(kHeadElements, name) && !b->body) {
        EnsureWrapper(b, &b->html, "html", b->doc);
        Node* head = EnsureWrapper(b, &b->head, "head", b->html);
        Node* node = Element(b, name, token->attrs);
        Append(head, node);
        if (!token->selfClosing && !In(kVoidElements, name)) {
            b->open.Append(b->a, node);
        }
        return;
    }
    Body(b);
    if (!b->fragment && b->open.len == 0) {
        b->open.Append(b->a, b->html);
        b->open.Append(b->a, b->body);
    } else if (!b->fragment && Current(b) == b->html) {
        b->open.Append(b->a, b->body);
    }

    CloseImplied(b, name);
    if (StrEqI(name, "tr") && StrEqI(Current(b)->name, "table")) {
        Node* tbody = Element(b, StrL("tbody"), nullptr);
        tbody->implicit = true;
        Append(Current(b), tbody);
        b->open.Append(b->a, tbody);
    } else if ((StrEqI(name, "td") || StrEqI(name, "th")) &&
               StrEqI(Current(b)->name, "table")) {
        Node* tbody = Element(b, StrL("tbody"), nullptr);
        tbody->implicit = true;
        Append(Current(b), tbody);
        b->open.Append(b->a, tbody);
        Node* tr = Element(b, StrL("tr"), nullptr);
        tr->implicit = true;
        Append(Current(b), tr);
        b->open.Append(b->a, tr);
    } else if ((StrEqI(name, "td") || StrEqI(name, "th")) &&
               (StrEqI(Current(b)->name, "tbody") ||
                StrEqI(Current(b)->name, "thead") ||
                StrEqI(Current(b)->name, "tfoot"))) {
        Node* tr = Element(b, StrL("tr"), nullptr);
        tr->implicit = true;
        Append(Current(b), tr);
        b->open.Append(b->a, tr);
    }
    Namespace ns = Current(b)->ns;
    if (StrEqI(name, "svg"))
        ns = Namespace::Svg;
    else if (StrEqI(name, "math"))
        ns = Namespace::MathMl;
    PushElement(b, token, ns);
}

static void EndFormatting(Builder* b, Str name) {
    int at = OpenIndex(b, name);
    if (at < 0) return;
    ArenaVec<Node*> reopen;
    for (int i = at + 1; i < b->open.len; i++) {
        if (In(kFormattingElements, b->open[i]->name)) {
            reopen.Append(b->a, b->open[i]);
        }
    }
    Node* parent = b->open[at]->parent;
    b->open.Truncate(at);
    for (int i = 0; i < reopen.len; i++) {
        Node* old = reopen[i];
        Node* node = Element(b, old->name, old->attrs, old->ns);
        Append(parent, node);
        b->open.Append(b->a, node);
        parent = node;
    }
}

static void EndTag(Builder* b, const Token* token) {
    Str name = token->name;
    if (StrEqI(name, "head")) {
        CloseNamed(b, "head");
        return;
    }
    if (StrEqI(name, "body") || StrEqI(name, "html")) {
        while (b->open.len && b->open[b->open.len - 1] != b->html)
            b->open.Pop();
        return;
    }
    if (In(kFormattingElements, name)) {
        EndFormatting(b, name);
        return;
    }
    int at = OpenIndex(b, name);
    if (at >= 0) b->open.Truncate(at);
}

static void BuildToken(void* user, const Token* token) {
    Builder* b = (Builder*)user;
    switch (token->kind) {
        case TokenKind::Character:
        case TokenKind::NullCharacter:
            AppendText(b, token->data);
            break;
        case TokenKind::Comment: {
            Node* comment = NewNode(b->a, NodeKind::Comment);
            comment->data = StrDup(b->a, token->data);
            Append(Current(b), comment);
            break;
        }
        case TokenKind::Doctype:
            if (!b->options.dropDoctype && !b->fragment) {
                Node* node =
                    NewNode(b->a, NodeKind::Doctype, StrDup(b->a, token->name));
                node->data = StrDup(b->a, token->data);
                node->systemId = StrDup(b->a, token->systemId);
                Append(b->doc, node);
            }
            break;
        case TokenKind::StartTag:
            StartTag(b, token);
            break;
        case TokenKind::EndTag:
            EndTag(b, token);
            break;
        default:
            break;
    }
}

static Node* Parse(Arena* a, Str source, Str context, ParseOptions options,
                   bool fragment) {
    if (!a) return nullptr;
    Builder builder;
    builder.a = a;
    builder.options = options;
    builder.fragment = fragment;
    builder.context = context;
    builder.doc = NewNode(a, NodeKind::Document);
    if (fragment) {
        builder.doc->name = context.s ? LowerCopy(a, context) : StrL("body");
        builder.doc->ns = StrEqI(context, "svg")    ? Namespace::Svg
                          : StrEqI(context, "math") ? Namespace::MathMl
                                                    : Namespace::Html;
    }
    TokenizerOptions tokenizer = options.tokenizer;
    tokenizer.exactErrors = tokenizer.exactErrors || options.exactErrors;
    if (fragment &&
        (In(kRawElements, context) || In(kRcdataElements, context))) {
        Scanner scanner;
        scanner.a = a;
        scanner.source = source;
        scanner.sink = BuildToken;
        scanner.user = &builder;
        scanner.options = tokenizer;
        scanner.rawName = context;
        scanner.rcdata = In(kRcdataElements, context);
        TokenizeRun(&scanner);
    } else {
        Tokenize(a, source, BuildToken, &builder, tokenizer);
    }
    if (!fragment) Body(&builder);
    return builder.doc;
}

Node* ParseDocument(Arena* a, Str source, ParseOptions options) {
    return Parse(a, source, {}, options, false);
}

Node* ParseFragment(Arena* a, Str source, Str context, ParseOptions options) {
    return Parse(a, source, context, options, true);
}

const Attribute* Attr(const Node* node, Str name) {
    if (!node) return nullptr;
    for (const Attribute* attr = node->attrs; attr; attr = attr->next) {
        if (StrEqI(attr->name, name)) return attr;
    }
    return nullptr;
}

Str AttrValue(const Node* node, Str name) {
    const Attribute* attr = Attr(node, name);
    return attr ? attr->value : Str{};
}

static void WriteEscaped(Arena* a, StrBuilder& out, Str value, bool attribute) {
    for (int i = 0; i < value.len; i++) {
        char c = value.s[i];
        if (c == '&')
            StrBuilderAppend(a, out, StrL("&amp;"));
        else if (c == '<')
            StrBuilderAppend(a, out, StrL("&lt;"));
        else if (c == '>' && !attribute)
            StrBuilderAppend(a, out, StrL("&gt;"));
        else if (c == '"' && attribute)
            StrBuilderAppend(a, out, StrL("&quot;"));
        else
            StrBuilderAppendChar(a, out, c);
    }
}

static void WriteNode(Arena* a, StrBuilder& out, const Node* node,
                      bool include) {
    if (!node) return;
    bool element = node->kind == NodeKind::Element;
    if (include) {
        if (node->kind == NodeKind::Text) {
            if (node->parent && In(kRawElements, node->parent->name))
                StrBuilderAppend(a, out, node->data);
            else
                WriteEscaped(a, out, node->data, false);
        } else if (node->kind == NodeKind::Comment) {
            StrBuilderAppend(a, out, StrL("<!--"));
            StrBuilderAppend(a, out, node->data);
            StrBuilderAppend(a, out, StrL("-->"));
        } else if (node->kind == NodeKind::Doctype) {
            StrBuilderAppend(a, out, StrL("<!DOCTYPE "));
            StrBuilderAppend(a, out, node->name);
            StrBuilderAppendChar(a, out, '>');
        } else if (element) {
            StrBuilderAppendChar(a, out, '<');
            StrBuilderAppend(a, out, node->name);
            for (const Attribute* attr = node->attrs; attr; attr = attr->next) {
                StrBuilderAppendChar(a, out, ' ');
                StrBuilderAppend(a, out, attr->name);
                StrBuilderAppend(a, out, StrL("=\""));
                WriteEscaped(a, out, attr->value, true);
                StrBuilderAppendChar(a, out, '"');
            }
            StrBuilderAppendChar(a, out, '>');
        }
    }
    if (node->kind != NodeKind::Text && node->kind != NodeKind::Comment &&
        node->kind != NodeKind::Doctype) {
        for (const Node* child = node->first; child; child = child->next) {
            WriteNode(a, out, child, true);
        }
    }
    if (include && element && !In(kVoidElements, node->name)) {
        StrBuilderAppend(a, out, StrL("</"));
        StrBuilderAppend(a, out, node->name);
        StrBuilderAppendChar(a, out, '>');
    }
}

Str Serialize(Arena* a, const Node* node, SerializeOptions options) {
    if (!a || !node) return {};
    StrBuilder out;
    WriteNode(a, out, node, options.includeNode);
    return StrBuilderTakeStr(a, out);
}

} // namespace html5ever

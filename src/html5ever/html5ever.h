#ifndef GPUI_HTML5EVER_HTML5EVER_H_
#define GPUI_HTML5EVER_HTML5EVER_H_
/* A C++ port of html5ever 0.27's public parsing model.

   The Rust crate is generic over a TreeSink. This port keeps the useful half
   of that seam: Tokenize sends POD tokens to a callback, while ParseDocument
   and ParseFragment build an arena-owned DOM. Strings and nodes have the
   arena's lifetime. */

#include "base.h"

namespace html5ever {

using base::Arena;
using base::ArenaVec;
using base::Str;

enum class Namespace : uint8_t {
    Html,
    MathMl,
    Svg,
};

enum class NodeKind : uint8_t {
    Document,
    Doctype,
    Text,
    Comment,
    Element,
};

struct Attribute {
    Str name = {};
    Str value = {};
    Namespace ns = Namespace::Html;
    Attribute* next = nullptr;
};

struct Node {
    NodeKind kind = NodeKind::Document;
    Namespace ns = Namespace::Html;
    Str name = {};
    // Text/comment contents, or a doctype's public identifier.
    Str data = {};
    // A doctype's system identifier.
    Str systemId = {};
    Attribute* attrs = nullptr;
    Node* parent = nullptr;
    Node* first = nullptr;
    Node* last = nullptr;
    Node* next = nullptr;
    // Parser-created html/head/body/tbody nodes are distinguishable from
    // source elements. Consumers which project rather than serialize a DOM
    // can omit those wrappers without losing an explicit source wrapper.
    bool implicit = false;
};

enum class TokenKind : uint8_t {
    Eof,
    ParseError,
    Character,
    NullCharacter,
    Comment,
    Doctype,
    StartTag,
    EndTag,
};

struct Token {
    TokenKind kind = TokenKind::Eof;
    Str name = {};
    Str data = {};
    Str systemId = {};
    Attribute* attrs = nullptr;
    bool selfClosing = false;
    bool forceQuirks = false;
    int line = 1;
};

using TokenSink = void (*)(void* user, const Token* token);

struct TokenizerOptions {
    bool exactErrors = false;
    bool discardBom = true;
};

struct ParseOptions {
    TokenizerOptions tokenizer = {};
    bool exactErrors = false;
    bool scriptingEnabled = true;
    bool iframeSrcdoc = false;
    bool dropDoctype = false;
};

struct SerializeOptions {
    bool includeNode = false;
    bool createMissingParent = true;
};

void Tokenize(Arena* a, Str source, TokenSink sink, void* user = nullptr,
              TokenizerOptions options = {});
Node* ParseDocument(Arena* a, Str source, ParseOptions options = {});
Node* ParseFragment(Arena* a, Str source, Str context = Str{},
                    ParseOptions options = {});
Str Serialize(Arena* a, const Node* node, SerializeOptions options = {});

const Attribute* Attr(const Node* node, Str name);
Str AttrValue(const Node* node, Str name);

} // namespace html5ever
#endif // GPUI_HTML5EVER_HTML5EVER_H_

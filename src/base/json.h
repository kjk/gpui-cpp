#ifndef GPUI_BASE_JSON_H_
#define GPUI_BASE_JSON_H_
/* A small JSON reader and writer.

   Rust reaches for serde_json to persist a dock layout; this is the part of
   that the port needs — a document parsed into arena nodes, and a builder
   that writes one back out. Numbers are doubles, strings keep their escapes
   undone, and nothing here allocates outside the arena it was given. */

#include "base.h"

namespace gpui {

enum class JsonKind : uint8_t {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object
};

struct JsonValue;

// One member of an object, or one element of an array — the same node type,
// with `key` set only for a member. They are a linked list rather than an
// array so a document parses in one pass without counting anything twice.
struct JsonValue {
    JsonKind kind = JsonKind::Null;
    bool b = false;
    double num = 0;
    Str str = {};
    Str key = {};
    // The first child of an object or array, and the next sibling of either.
    JsonValue* first = nullptr;
    JsonValue* next = nullptr;
};

// The document, or null when the text is not JSON. Everything it points at
// lives in `a`, including the strings — the source text is not kept.
JsonValue* JsonParse(Arena* a, Str text);

// The member of an object by name, or null. A value that is not an object has
// no members.
const JsonValue* JsonGet(const JsonValue* v, const char* key);
// The element of an array by index, or null.
const JsonValue* JsonAt(const JsonValue* v, int index);
// How many children an object or an array has.
int JsonLen(const JsonValue* v);

// The reads a caller does on a value it expects: each answers `fallback` when
// the value is missing or is of another kind.
double JsonNumber(const JsonValue* v, double fallback = 0);
bool JsonBool(const JsonValue* v, bool fallback = false);
Str JsonString(const JsonValue* v, Str fallback = {});

// The writer. It tracks whether a comma is due, so a caller writes members and
// elements without keeping count.
struct JsonWriter {
    StrBuilder* out = nullptr;
    // Whether something has already been written at this depth.
    bool wrote[32] = {};
    int depth = 0;

    void BeginObject(const char* key = nullptr);
    void EndObject();
    void BeginArray(const char* key = nullptr);
    void EndArray();
    void Number(const char* key, double v);
    void Bool(const char* key, bool v);
    void String(const char* key, Str v);
    void Null(const char* key);
    // A caller that already validated a complete JSON value may preserve it
    // without turning it into a quoted string. Value writes a parsed tree in
    // canonical compact form and is the safe way to produce such text.
    void Raw(const char* key, Str json);
    void Value(const char* key, const JsonValue* value);
};

} // namespace gpui
#endif // GPUI_BASE_JSON_H_

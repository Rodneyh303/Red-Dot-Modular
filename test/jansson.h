// ── test/jansson.h — TEST-ONLY jansson shim for the standalone unit-test harness ────────────────
// The pure .dmcurve module (src/dsp/MonotoneCurve.hpp) serialises with jansson's json_t API,
// exactly like TuningPreset (.dmtune). The plugin tree links Rack's bundled libjansson, but the
// standalone harness (test/run_all.sh) has no jansson library to link — which is why
// test_TuningRoundTrip cannot cover TuningPreset's JSON at all. MonotoneCurve deliberately
// includes <jansson.h> DIRECTLY (not via rack.hpp), so with -Itest first on the include path
// the test build resolves that include to THIS shim (the test/rack.hpp precedent) while the
// plugin build keeps using the real thing — the .dmcurve schema logic runs against the real
// API surface either way.
//
// Implements the subset the house JSON style uses (TuningPreset / MicroTuning dataToJson):
// builders, object/array accessors, value accessors, type checks, refcount ownership
// (json_object_set_new / json_array_append_new STEAL the reference; the _get accessors
// BORROW), plus json_dumps / json_loads so tests can round-trip through actual JSON TEXT
// (proving the emitted schema is parseable). Simplifications: object key lookup is a linear
// scan (our objects are tiny), json_dumps honours JSON_INDENT/JSON_COMPACT and always prints
// reals with %.17g (round-trip exact), json_loads accepts the common escapes but not \uXXXX
// (none of the plugin's data uses them).

#pragma once

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

typedef long long json_int_t;   // matches jansson_config.h (JSON_INTEGER_IS_LONG_LONG 1)

#define JSON_ERROR_TEXT_LENGTH   160
#define JSON_ERROR_SOURCE_LENGTH 80

typedef struct json_error_t {
    int  line, column, position;
    char source[JSON_ERROR_SOURCE_LENGTH];
    char text[JSON_ERROR_TEXT_LENGTH];
} json_error_t;

// Encoding flags (subset; only JSON_INDENT / JSON_COMPACT affect dumps output).
#define JSON_COMPACT          0x20
#define JSON_MAX_INDENT       0x1F
#define JSON_INDENT(n)        ((n) & JSON_MAX_INDENT)
#define JSON_REAL_PRECISION(n) (((n) & 0x1F) << 11)

typedef enum json_type {
    JSON_OBJECT, JSON_ARRAY, JSON_STRING, JSON_INTEGER,
    JSON_REAL, JSON_TRUE, JSON_FALSE, JSON_NULL
} json_type_t;

typedef struct json_t {
    size_t                 refcount;
    json_type              type;
    std::string            str;      // JSON_STRING
    json_int_t             integer;  // JSON_INTEGER
    double                 real;     // JSON_REAL
    std::vector<json_t*>   items;    // JSON_ARRAY elements / JSON_OBJECT values
    std::vector<std::string> keys;   // JSON_OBJECT keys (parallel to items)
} json_t;

static inline json_type json_typeof(const json_t* json) {
    return json ? json->type : (json_type)-1;
}

#define json_is_object(json)  ((json) && json_typeof(json) == JSON_OBJECT)
#define json_is_array(json)   ((json) && json_typeof(json) == JSON_ARRAY)
#define json_is_string(json)  ((json) && json_typeof(json) == JSON_STRING)
#define json_is_integer(json) ((json) && json_typeof(json) == JSON_INTEGER)
#define json_is_real(json)    ((json) && json_typeof(json) == JSON_REAL)
#define json_is_number(json)  (json_is_integer(json) || json_is_real(json))
#define json_is_true(json)    ((json) && json_typeof(json) == JSON_TRUE)
#define json_is_false(json)   ((json) && json_typeof(json) == JSON_FALSE)
#define json_is_boolean(json) (json_is_true(json) || json_is_false(json))
#define json_is_null(json)    ((json) && json_typeof(json) == JSON_NULL)
#define json_boolean_value    json_is_true
#define json_boolean(val)     ((val) ? json_true() : json_false())

// ── construction / refcount ────────────────────────────────────────────────────────────────────
static inline json_t* jansson_new(json_type t) {
    json_t* j = new json_t();
    j->refcount = 1;
    j->type = t;
    return j;
}
static inline json_t* json_object(void) { return jansson_new(JSON_OBJECT); }
static inline json_t* json_array(void)  { return jansson_new(JSON_ARRAY); }
static inline json_t* json_string(const char* value) {
    json_t* j = jansson_new(JSON_STRING);
    j->str = value ? value : "";
    return j;
}
static inline json_t* json_integer(json_int_t value) { json_t* j = jansson_new(JSON_INTEGER); j->integer = value; return j; }
static inline json_t* json_real(double value)        { json_t* j = jansson_new(JSON_REAL);    j->real = value;    return j; }
static inline json_t* json_true(void)  { return jansson_new(JSON_TRUE); }
static inline json_t* json_false(void) { return jansson_new(JSON_FALSE); }
static inline json_t* json_null(void)  { return jansson_new(JSON_NULL); }

static inline json_t* json_incref(json_t* json) {
    if (json) json->refcount++;
    return json;
}

static inline void json_decref(json_t* json) {
    if (!json || --json->refcount > 0) return;
    for (size_t i = 0; i < json->items.size(); ++i) json_decref(json->items[i]);
    delete json;
}

// ── object ─────────────────────────────────────────────────────────────────────────────────────
static inline json_t* json_object_get(const json_t* object, const char* key) {
    if (!json_is_object(object) || !key) return NULL;
    for (size_t i = 0; i < object->keys.size(); ++i)
        if (object->keys[i] == key) return object->items[i];   // borrowed reference
    return NULL;
}
static inline int json_object_set_new(json_t* object, const char* key, json_t* value) {
    if (!json_is_object(object) || !key || !value) { json_decref(value); return -1; }
    for (size_t i = 0; i < object->keys.size(); ++i) {
        if (object->keys[i] == key) {
            json_decref(object->items[i]);
            object->items[i] = value;   // steals the caller's reference
            return 0;
        }
    }
    object->keys.push_back(key);
    object->items.push_back(value);     // steals the caller's reference
    return 0;
}

// ── array ──────────────────────────────────────────────────────────────────────────────────────
static inline size_t json_array_size(const json_t* array) {
    return json_is_array(array) ? array->items.size() : 0;
}
static inline json_t* json_array_get(const json_t* array, size_t index) {
    if (!json_is_array(array) || index >= array->items.size()) return NULL;   // borrowed
    return array->items[index];
}
static inline int json_array_append_new(json_t* array, json_t* value) {
    if (!json_is_array(array) || !value) { json_decref(value); return -1; }
    array->items.push_back(value);       // steals the caller's reference
    return 0;
}

// ── value access ───────────────────────────────────────────────────────────────────────────────
static inline const char* json_string_value(const json_t* string) {
    return json_is_string(string) ? string->str.c_str() : NULL;
}
static inline json_int_t json_integer_value(const json_t* integer) {
    return json_is_integer(integer) ? integer->integer : (json_int_t)0;
}
static inline double json_real_value(const json_t* real) {
    return json_is_real(real) ? real->real : 0.0;
}
static inline double json_number_value(const json_t* json) {
    if (json_is_real(json))    return json->real;
    if (json_is_integer(json)) return (double)json->integer;
    return 0.0;
}

// ── text output: json_dumps (malloc'd C string; caller free()s) ─────────────────────────────────
static void jansson_dump_into(const json_t* j, size_t flags, int indent, std::string& out) {
    const bool compact = (flags & JSON_COMPACT) != 0;
    const int  step    = (int)(flags & JSON_MAX_INDENT);
    const auto pad = [&](int level) { if (!compact && step) out.append((size_t)(level * step), ' '); };
    const auto nl  = [&]() { if (!compact && step) out.push_back('\n'); };
    switch (j ? j->type : JSON_NULL) {
        case JSON_OBJECT: {
            if (!j || j->keys.empty()) { out += "{}"; break; }
            out += '{'; nl();
            for (size_t i = 0; i < j->keys.size(); ++i) {
                pad(indent + 1);
                out += '"'; out += j->keys[i]; out += '"';
                out += compact ? ":" : ": ";
                jansson_dump_into(j->items[i], flags, indent + 1, out);
                if (i + 1 < j->keys.size()) out += ',';
                nl();
            }
            pad(indent);
            out += '}';
            break;
        }
        case JSON_ARRAY: {
            if (!j || j->items.empty()) { out += "[]"; break; }
            out += '['; nl();
            for (size_t i = 0; i < j->items.size(); ++i) {
                pad(indent + 1);
                jansson_dump_into(j->items[i], flags, indent + 1, out);
                if (i + 1 < j->items.size()) out += ',';
                nl();
            }
            pad(indent);
            out += ']';
            break;
        }
        case JSON_STRING: {
            out += '"';
            for (size_t k = 0; j && k < j->str.size(); ++k) {
                const char c = j->str[k];
                switch (c) {
                    case '"':  out += "\\\""; break;
                    case '\\': out += "\\\\"; break;
                    case '\n': out += "\\n";  break;
                    case '\t': out += "\\t";  break;
                    case '\r': out += "\\r";  break;
                    default:   out.push_back(c); break;
                }
            }
            out += '"';
            break;
        }
        case JSON_INTEGER: {
            char b[40];
            snprintf(b, sizeof b, "%lld", j ? j->integer : (json_int_t)0);
            out += b;
            break;
        }
        case JSON_REAL: {
            char b[64];
            const unsigned prec = (unsigned)((flags >> 11) & 0x1F);
            snprintf(b, sizeof b, "%.*g", prec ? (int)prec : 17, j ? j->real : 0.0);
            out += b;
            break;
        }
        case JSON_TRUE:  out += "true";  break;
        case JSON_FALSE: out += "false"; break;
        case JSON_NULL: default: out += "null"; break;
    }
}

static inline char* json_dumps(json_t* json, size_t flags) {
    std::string out;
    jansson_dump_into(json, flags, 0, out);
    char* s = (char*)malloc(out.size() + 1);
    if (!s) return NULL;
    memcpy(s, out.data(), out.size());
    s[out.size()] = '\0';
    return s;
}

// ── text input: json_loads (recursive descent) ──────────────────────────────────────────────────
struct JanssonParser {
    const char*   s;
    size_t        pos, len;
    json_error_t* err;

    void fail(const char* text) {
        if (err && !err->text[0]) snprintf(err->text, JSON_ERROR_TEXT_LENGTH, "%s", text);
    }
    void skipWs() {
        while (pos < len && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r')) ++pos;
    }
};

static json_t* jansson_parse_value(JanssonParser& p, int depth);

static json_t* jansson_parse_string(JanssonParser& p) {
    ++p.pos;   // opening quote
    std::string out;
    while (p.pos < p.len) {
        const char c = p.s[p.pos++];
        if (c == '"') return json_string(out.c_str());
        if (c == '\\') {
            if (p.pos >= p.len) { p.fail("premature end of string"); return NULL; }
            const char e = p.s[p.pos++];
            switch (e) {
                case '"':  out.push_back('"');  break;
                case '\\': out.push_back('\\'); break;
                case '/':  out.push_back('/');  break;
                case 'b':  out.push_back('\b'); break;
                case 'f':  out.push_back('\f'); break;
                case 'n':  out.push_back('\n'); break;
                case 'r':  out.push_back('\r'); break;
                case 't':  out.push_back('\t'); break;
                default:   p.fail("unsupported escape in string"); return NULL;
            }
            continue;
        }
        if ((unsigned char)c < 0x20) { p.fail("control character in string"); return NULL; }
        out.push_back(c);
    }
    p.fail("unterminated string");
    return NULL;
}

static json_t* jansson_parse_number(JanssonParser& p) {
    const size_t start = p.pos;
    if (p.pos < p.len && (p.s[p.pos] == '-' || p.s[p.pos] == '+')) ++p.pos;
    bool isReal = false;
    while (p.pos < p.len) {
        const char c = p.s[p.pos];
        if (c >= '0' && c <= '9') { ++p.pos; continue; }
        if (c == '.' || c == 'e' || c == 'E') {
            isReal = true;
            ++p.pos;
            if (p.pos < p.len && (p.s[p.pos] == '+' || p.s[p.pos] == '-')) ++p.pos;   // exponent sign
            continue;
        }
        break;
    }
    if (p.pos == start) { p.fail("invalid number"); return NULL; }
    const std::string tok(p.s + start, p.pos - start);
    if (isReal) return json_real(strtod(tok.c_str(), NULL));
    return json_integer((json_int_t)strtoll(tok.c_str(), NULL, 10));
}

static json_t* jansson_parse_value(JanssonParser& p, int depth) {
    if (depth > 64) { p.fail("nesting too deep"); return NULL; }
    p.skipWs();
    if (p.pos >= p.len) { p.fail("premature end of input"); return NULL; }
    const char c = p.s[p.pos];
    if (c == '{') {
        ++p.pos;
        json_t* obj = json_object();
        p.skipWs();
        if (p.pos < p.len && p.s[p.pos] == '}') { ++p.pos; return obj; }
        for (;;) {
            p.skipWs();
            if (p.pos >= p.len || p.s[p.pos] != '"') { p.fail("expected object key string"); json_decref(obj); return NULL; }
            json_t* key = jansson_parse_string(p);
            if (!key) { json_decref(obj); return NULL; }
            p.skipWs();
            if (p.pos >= p.len || p.s[p.pos] != ':') { p.fail("expected ':'"); json_decref(key); json_decref(obj); return NULL; }
            ++p.pos;
            json_t* val = jansson_parse_value(p, depth + 1);
            if (!val) { json_decref(key); json_decref(obj); return NULL; }
            json_object_set_new(obj, json_string_value(key), val);   // steals val
            json_decref(key);
            p.skipWs();
            if (p.pos < p.len && p.s[p.pos] == ',') { ++p.pos; continue; }
            if (p.pos < p.len && p.s[p.pos] == '}') { ++p.pos; return obj; }
            p.fail("expected ',' or '}'");
            json_decref(obj);
            return NULL;
        }
    }
    if (c == '[') {
        ++p.pos;
        json_t* arr = json_array();
        p.skipWs();
        if (p.pos < p.len && p.s[p.pos] == ']') { ++p.pos; return arr; }
        for (;;) {
            json_t* val = jansson_parse_value(p, depth + 1);
            if (!val) { json_decref(arr); return NULL; }
            json_array_append_new(arr, val);   // steals val
            p.skipWs();
            if (p.pos < p.len && p.s[p.pos] == ',') { ++p.pos; continue; }
            if (p.pos < p.len && p.s[p.pos] == ']') { ++p.pos; return arr; }
            p.fail("expected ',' or ']'");
            json_decref(arr);
            return NULL;
        }
    }
    if (c == '"') return jansson_parse_string(p);
    if (c == 't' && p.len - p.pos >= 4 && strncmp(p.s + p.pos, "true", 4) == 0)  { p.pos += 4; return json_true(); }
    if (c == 'f' && p.len - p.pos >= 5 && strncmp(p.s + p.pos, "false", 5) == 0) { p.pos += 5; return json_false(); }
    if (c == 'n' && p.len - p.pos >= 4 && strncmp(p.s + p.pos, "null", 4) == 0)  { p.pos += 4; return json_null(); }
    if (c == '-' || c == '+' || (c >= '0' && c <= '9')) return jansson_parse_number(p);
    p.fail("unexpected character");
    return NULL;
}

static inline json_t* json_loads(const char* input, size_t flags, json_error_t* error) {
    (void)flags;
    if (error) { error->text[0] = 0; error->line = error->column = error->position = 0; }
    JanssonParser p;
    p.s   = input ? input : "";
    p.len = strlen(p.s);
    p.pos = 0;
    p.err = error;
    json_t* v = jansson_parse_value(p, 0);
    if (!v) return NULL;
    p.skipWs();
    if (p.pos != p.len) { p.fail("trailing characters after JSON value"); json_decref(v); return NULL; }
    return v;
}

#pragma once

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// Minimal, dependency-free JSON for the engine⇄orchestrator protocol (see
// docs/protocol_v1.md). We fully control the wire format, so the writer is a
// tiny object builder; the parser is just enough to read replies (flat objects
// with int/bool/string/null/array values, standard string escapes incl. \uXXXX).
// Not a general-purpose JSON library — scoped to this protocol.
namespace ww::jsonu {

// ---------- Writer ----------

inline std::string escape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 2);
    for (unsigned char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            case '\b': o += "\\b"; break;
            case '\f': o += "\\f"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    o += buf;
                } else {
                    o += static_cast<char>(c);  // pass UTF-8 bytes through verbatim
                }
        }
    }
    return o;
}

inline std::string quote(const std::string& s) { return "\"" + escape(s) + "\""; }

// Ordered key/value object builder. `raw` injects a pre-built JSON fragment
// (e.g. a nested array/object the caller already serialized).
class Obj {
public:
    Obj& str(const std::string& k, const std::string& v) { key(k); buf_ += quote(v); return *this; }
    Obj& num(const std::string& k, long long v) { key(k); buf_ += std::to_string(v); return *this; }
    Obj& boolean(const std::string& k, bool v) { key(k); buf_ += v ? "true" : "false"; return *this; }
    Obj& null(const std::string& k) { key(k); buf_ += "null"; return *this; }
    Obj& raw(const std::string& k, const std::string& rawJson) { key(k); buf_ += rawJson; return *this; }
    std::string dump() const { return "{" + buf_ + "}"; }

private:
    void key(const std::string& k) {
        if (!buf_.empty()) buf_ += ",";
        buf_ += quote(k) + ":";
    }
    std::string buf_;
};

// ---------- Parser ----------

struct Value {
    enum class T { Null, Bool, Int, Double, Str, Arr, Obj } t = T::Null;
    bool b = false;
    long long i = 0;
    double d = 0;
    std::string s;
    std::vector<Value> arr;
    std::vector<std::pair<std::string, Value>> members;  // objects (vector: incomplete-type safe)

    bool isNull() const { return t == T::Null; }
    bool isBool() const { return t == T::Bool; }
    bool isInt() const { return t == T::Int; }
    bool isStr() const { return t == T::Str; }

    const Value* get(const std::string& k) const {
        if (t != T::Obj) return nullptr;
        for (const auto& [kk, vv] : members) {
            if (kk == k) return &vv;
        }
        return nullptr;
    }
};

namespace detail {

struct P {
    const std::string& s;
    std::size_t i = 0;
    bool ok = true;
    int depth = 0;  // guards against stack overflow on adversarial deep nesting
    static constexpr int kMaxDepth = 200;
    explicit P(const std::string& str) : s(str) {}

    void ws() {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i;
    }
    char peek() const { return i < s.size() ? s[i] : '\0'; }

    static std::string utf8(unsigned cp) {
        std::string o;
        if (cp <= 0x7F) {
            o += static_cast<char>(cp);
        } else if (cp <= 0x7FF) {
            o += static_cast<char>(0xC0 | (cp >> 6));
            o += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp <= 0xFFFF) {
            o += static_cast<char>(0xE0 | (cp >> 12));
            o += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            o += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            o += static_cast<char>(0xF0 | (cp >> 18));
            o += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            o += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            o += static_cast<char>(0x80 | (cp & 0x3F));
        }
        return o;
    }
    unsigned hex4() {
        unsigned v = 0;
        for (int k = 0; k < 4 && i < s.size(); ++k) {
            char c = s[i++];
            v <<= 4;
            if (c >= '0' && c <= '9') v += static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') v += static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v += static_cast<unsigned>(c - 'A' + 10);
            else ok = false;
        }
        return v;
    }
    std::string unicode() {
        unsigned cp = hex4();
        if (cp >= 0xD800 && cp <= 0xDBFF) {  // high surrogate -> expect low surrogate
            if (i + 1 < s.size() && s[i] == '\\' && s[i + 1] == 'u') {
                i += 2;
                unsigned lo = hex4();
                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
            }
        }
        return utf8(cp);
    }
    std::string string() {
        std::string out;
        ++i;  // opening quote
        while (i < s.size()) {
            char c = s[i++];
            if (c == '"') return out;
            if (c == '\\') {
                if (i >= s.size()) break;
                char e = s[i++];
                switch (e) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'u': out += unicode(); break;
                    default: ok = false; return out;
                }
            } else {
                out += c;
            }
        }
        ok = false;
        return out;
    }
    Value number() {
        std::size_t start = i;
        bool dbl = false;
        if (peek() == '-') ++i;
        while (i < s.size()) {
            char c = s[i];
            if (c >= '0' && c <= '9') { ++i; }
            else if (c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') { dbl = true; ++i; }
            else break;
        }
        Value v;
        std::string tok = s.substr(start, i - start);
        if (tok.empty()) { ok = false; return v; }
        if (dbl) { v.t = Value::T::Double; v.d = std::strtod(tok.c_str(), nullptr); }
        else { v.t = Value::T::Int; v.i = std::strtoll(tok.c_str(), nullptr, 10); }
        return v;
    }
    Value keyword() {
        Value v;
        if (s.compare(i, 4, "true") == 0) { v.t = Value::T::Bool; v.b = true; i += 4; }
        else if (s.compare(i, 5, "false") == 0) { v.t = Value::T::Bool; v.b = false; i += 5; }
        else if (s.compare(i, 4, "null") == 0) { v.t = Value::T::Null; i += 4; }
        else ok = false;
        return v;
    }
    Value array() {
        Value v; v.t = Value::T::Arr; ++i; ws();
        if (peek() == ']') { ++i; return v; }
        while (ok) {
            v.arr.push_back(value());
            ws();
            char c = peek();
            if (c == ',') { ++i; continue; }
            if (c == ']') { ++i; break; }
            ok = false; break;
        }
        return v;
    }
    Value object() {
        Value v; v.t = Value::T::Obj; ++i; ws();
        if (peek() == '}') { ++i; return v; }
        while (ok) {
            ws();
            if (peek() != '"') { ok = false; break; }
            std::string k = string();
            ws();
            if (peek() != ':') { ok = false; break; }
            ++i;
            v.members.emplace_back(std::move(k), value());
            ws();
            char c = peek();
            if (c == ',') { ++i; continue; }
            if (c == '}') { ++i; break; }
            ok = false; break;
        }
        return v;
    }
    Value value() {
        ws();
        if (i >= s.size() || depth > kMaxDepth) { ok = false; return {}; }
        char c = s[i];
        if (c == '{') { ++depth; Value v = object(); --depth; return v; }
        if (c == '[') { ++depth; Value v = array(); --depth; return v; }
        if (c == '"') { Value v; v.t = Value::T::Str; v.s = string(); return v; }
        if (c == 't' || c == 'f' || c == 'n') return keyword();
        return number();
    }
};

}  // namespace detail

inline std::optional<Value> parse(const std::string& s) {
    detail::P p(s);
    Value v = p.value();
    if (!p.ok) return std::nullopt;
    return v;
}

}  // namespace ww::jsonu

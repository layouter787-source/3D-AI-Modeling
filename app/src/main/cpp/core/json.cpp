#include "core/json.h"

#include <cstdio>
#include <cstdlib>

namespace core {

namespace {

void appendUtf8(std::string& out, unsigned cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

class Parser {
public:
    explicit Parser(const std::string& text) : t_(text) {}

    Json document() {
        Json v = value();
        ws();
        if (i_ != t_.size()) fail("trailing characters");
        return v;
    }

private:
    const std::string& t_;
    size_t i_ = 0;
    int depth_ = 0;

    [[noreturn]] void fail(const std::string& what) const {
        throw std::runtime_error("invalid JSON: " + what + " at position " + std::to_string(i_));
    }

    void ws() {
        while (i_ < t_.size() && static_cast<unsigned char>(t_[i_]) <= ' ') ++i_;
    }

    Json value() {
        ws();
        if (i_ >= t_.size()) fail("unexpected end");
        const char c = t_[i_];
        if (c == '{') return object();
        if (c == '[') return array();
        if (c == 34) {
            Json j;
            j.type = Json::Type::String;
            j.s = str();
            return j;
        }
        if (c == 't' || c == 'f' || c == 'n') return literal();
        return number();
    }

    Json literal() {
        Json j;
        if (t_.compare(i_, 4, "true") == 0) {
            j.type = Json::Type::Bool;
            j.b = true;
            i_ += 4;
        } else if (t_.compare(i_, 5, "false") == 0) {
            j.type = Json::Type::Bool;
            j.b = false;
            i_ += 5;
        } else if (t_.compare(i_, 4, "null") == 0) {
            i_ += 4;
        } else {
            fail("bad literal");
        }
        return j;
    }

    Json number() {
        const char c = t_[i_];
        if (!(c == '-' || (c >= '0' && c <= '9'))) fail("unexpected character");
        const char* start = t_.c_str() + i_;
        char* end = nullptr;
        const double v = std::strtod(start, &end);
        if (end == start) fail("bad number");
        i_ += static_cast<size_t>(end - start);
        Json j;
        j.type = Json::Type::Number;
        j.n = v;
        return j;
    }

    std::string str() {
        ++i_;  // aspas de abertura
        std::string out;
        while (true) {
            if (i_ >= t_.size()) fail("unterminated string");
            const char c = t_[i_++];
            if (c == 34) break;
            if (c != 92) {
                out += c;
                continue;
            }
            if (i_ >= t_.size()) fail("bad escape");
            const char e = t_[i_++];
            switch (e) {
                case 34: out += static_cast<char>(34); break;
                case 92: out += static_cast<char>(92); break;
                case 47: out += '/'; break;
                case 'b': out += static_cast<char>(8); break;
                case 'f': out += static_cast<char>(12); break;
                case 'n': out += static_cast<char>(10); break;
                case 'r': out += static_cast<char>(13); break;
                case 't': out += static_cast<char>(9); break;
                case 'u': {
                    if (i_ + 4 > t_.size()) fail("bad unicode escape");
                    unsigned cp = static_cast<unsigned>(std::strtoul(t_.substr(i_, 4).c_str(), nullptr, 16));
                    i_ += 4;
                    if (cp >= 0xD800 && cp <= 0xDBFF && i_ + 6 <= t_.size() && t_[i_] == 92 && t_[i_ + 1] == 'u') {
                        const unsigned lo =
                            static_cast<unsigned>(std::strtoul(t_.substr(i_ + 2, 4).c_str(), nullptr, 16));
                        if (lo >= 0xDC00 && lo <= 0xDFFF) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            i_ += 6;
                        }
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default: fail("bad escape");
            }
        }
        return out;
    }

    Json array() {
        if (++depth_ > 64) fail("nesting too deep");
        Json j;
        j.type = Json::Type::Array;
        ++i_;
        ws();
        if (i_ < t_.size() && t_[i_] == ']') {
            ++i_;
            --depth_;
            return j;
        }
        while (true) {
            j.a.push_back(value());
            ws();
            if (i_ >= t_.size()) fail("unterminated array");
            if (t_[i_] == ',') {
                ++i_;
                continue;
            }
            if (t_[i_] == ']') {
                ++i_;
                break;
            }
            fail("expected ',' or ']'");
        }
        --depth_;
        return j;
    }

    Json object() {
        if (++depth_ > 64) fail("nesting too deep");
        Json j;
        j.type = Json::Type::Object;
        ++i_;
        ws();
        if (i_ < t_.size() && t_[i_] == '}') {
            ++i_;
            --depth_;
            return j;
        }
        while (true) {
            ws();
            if (i_ >= t_.size() || t_[i_] != 34) fail("expected a key");
            std::string key = str();
            ws();
            if (i_ >= t_.size() || t_[i_] != ':') fail("expected ':'");
            ++i_;
            Json v = value();
            j.o.emplace_back(std::move(key), std::move(v));
            ws();
            if (i_ >= t_.size()) fail("unterminated object");
            if (t_[i_] == ',') {
                ++i_;
                continue;
            }
            if (t_[i_] == '}') {
                ++i_;
                break;
            }
            fail("expected ',' or '}'");
        }
        --depth_;
        return j;
    }
};

}  // namespace

Json Json::parse(const std::string& text) {
    Parser parser(text);
    return parser.document();
}

const Json* Json::get(const std::string& key) const {
    if (type != Type::Object) return nullptr;
    for (const auto& kv : o) {
        if (kv.first == key) return &kv.second;
    }
    return nullptr;
}

std::string jsonQuote(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out += static_cast<char>(34);
    for (unsigned char c : s) {
        switch (c) {
            case 34: out += static_cast<char>(92); out += static_cast<char>(34); break;
            case 92: out += static_cast<char>(92); out += static_cast<char>(92); break;
            case 10: out += static_cast<char>(92); out += 'n'; break;
            case 13: out += static_cast<char>(92); out += 'r'; break;
            case 9: out += static_cast<char>(92); out += 't'; break;
            default:
                if (c < 32) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "%04x", static_cast<unsigned>(c));
                    out += static_cast<char>(92);
                    out += 'u';
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += static_cast<char>(34);
    return out;
}

}  // namespace core

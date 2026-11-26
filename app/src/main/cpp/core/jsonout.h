#pragma once

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "core/json.h"
#include "core/math.h"

namespace core {

// Auxiliares para escrever JSON à mão (valores "raw" já são JSON válido).

inline std::string jnum(double v) {
    if (!std::isfinite(v)) return "0";
    char b[48];
    std::snprintf(b, sizeof(b), "%.5g", v);
    return b;
}

inline std::string jvec(Vec3 v) { return "[" + jnum(v.x) + "," + jnum(v.y) + "," + jnum(v.z) + "]"; }

inline std::string jbool(bool b) { return b ? "true" : "false"; }

inline std::string jlist(const std::vector<std::string>& raw) {
    std::string s = "[";
    for (size_t i = 0; i < raw.size(); ++i) {
        if (i > 0) s += ",";
        s += raw[i];
    }
    return s + "]";
}

class JObj {
public:
    JObj& add(const std::string& key, const std::string& raw) {
        if (!body_.empty()) body_ += ",";
        body_ += jsonQuote(key) + ":" + raw;
        return *this;
    }
    std::string str() const { return "{" + body_ + "}"; }

private:
    std::string body_;
};

}  // namespace core

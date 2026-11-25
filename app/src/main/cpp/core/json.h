#pragma once

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace core {

// JSON mínimo (suficiente para os argumentos das ferramentas).
struct Json {
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool b = false;
    double n = 0.0;
    std::string s;
    std::vector<Json> a;
    std::vector<std::pair<std::string, Json>> o;

    // Lança std::runtime_error se o texto for inválido.
    static Json parse(const std::string& text);

    const Json* get(const std::string& key) const;

    bool isNumber() const { return type == Type::Number; }
    bool isString() const { return type == Type::String; }
    bool isBool() const { return type == Type::Bool; }
    bool isArray() const { return type == Type::Array; }
    bool isObject() const { return type == Type::Object; }
};

// Devolve o texto entre aspas, com escapes JSON.
std::string jsonQuote(const std::string& s);

}  // namespace core

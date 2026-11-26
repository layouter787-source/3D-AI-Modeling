#pragma once

#include <string>

#include "core/editor.h"

namespace core {

// Definições das ferramentas em JSON: [{"name","description","parameters":{JSON Schema}}, ...].
std::string toolDefinitionsJson();

// Executa uma ferramenta. argsJson é um objeto JSON. Devolve {"ok":bool,"message":"...","data":{...}}.
// Ferramentas que alteram a cena são atómicas (se falharem, nada muda) e entram no histórico.
std::string callTool(Editor& editor, const std::string& name, const std::string& argsJson);

}  // namespace core

#pragma once
#include "model.hpp"

namespace rc {
// Gera somente o CSS; não acessa áudio nem altera configurações.
std::string gerar_css_tema(const Config& configuracao, bool composicao);
}

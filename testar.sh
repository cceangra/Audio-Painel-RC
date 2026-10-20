#!/usr/bin/env bash
# Testes locais sem comandar áudio. Uma pasta temporária de remoção é criada
# e apagada em testes/ para testar o desinstalador sem tocar nesta instalação.
set -euo pipefail
APP_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
for RC_FILE in "$APP_DIR"/*.sh; do bash -n "$RC_FILE"; done
exec "$APP_DIR/audio-painel-rc" --testes

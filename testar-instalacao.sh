#!/usr/bin/env bash
# Todos os arquivos e atalhos deste teste ficam em uma pasta privada em testes/.
set -euo pipefail
APP_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
exec "$APP_DIR/audio-painel-rc" --testes-instalacao

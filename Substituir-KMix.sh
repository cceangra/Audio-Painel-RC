#!/usr/bin/env bash
# Somente este comando explícito solicita a substituição do KMix.
# Não desinstala o KMix. Registra o valor anterior para Restaurar-KMix.sh.
set -euo pipefail
APP_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
exec "$APP_DIR/audio-painel-rc" --substituir-kmix

#!/usr/bin/env bash
# Somente leitura. Inclui pw-dump e, durante o teste, os dois pw-cat.
set -euo pipefail
APP_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
exec "$APP_DIR/audio-painel-rc" --ram "$@"

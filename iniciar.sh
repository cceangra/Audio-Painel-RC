#!/usr/bin/env bash
set -euo pipefail
APP_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
if [[ ! -x "$APP_DIR/audio-painel-rc" ]]; then
    printf '%s\n' 'Execute: bash compilar.sh, dentro desta pasta.' >&2
    exit 1
fi
exec "$APP_DIR/audio-painel-rc" --iniciar "$@"

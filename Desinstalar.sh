#!/usr/bin/env bash
# Não remove ZIPs, KMix, filtro de ruído ou automação HDMI.
set -euo pipefail
APP_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
if [[ ! -x "$APP_DIR/audio-painel-rc" ]]; then
    printf '%s\n' 'Executável ausente. Recompile com compilar.sh para usar a remoção verificada.' >&2
    exit 1
fi
if [[ $# -eq 0 && ( -n "${DISPLAY:-}" || -n "${WAYLAND_DISPLAY:-}" ) ]]; then
    exec "$APP_DIR/audio-painel-rc" --desinstalador
fi
exec "$APP_DIR/audio-painel-rc" --desinstalar "$@"

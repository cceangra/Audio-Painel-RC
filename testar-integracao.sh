#!/usr/bin/env bash
# Simula PipeWire e PCM. Não captura voz, não toca áudio, não altera o sistema.
set -euo pipefail
APP_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
RC_WORK="$(mktemp -d "$APP_DIR/testes/io-XXXXXX")"
cleanup() {
    local RC_FILE
    shopt -s nullglob dotglob
    for RC_FILE in "$RC_WORK"/*; do
        if [[ -f "$RC_FILE" || -L "$RC_FILE" ]]; then unlink -- "$RC_FILE"; fi
    done
    rmdir -- "$RC_WORK" 2>/dev/null || true
}
trap cleanup EXIT
RC_CXX="${CXX:-g++}"
RC_COMPILER_ROOT="${RC_GXX_ROOT:-$HOME/Apps/G++/dependencias/rootfs}"
if [[ -z "${CXX:-}" && -x "$RC_COMPILER_ROOT/usr/bin/g++-14" ]]; then
    RC_CXX="$RC_COMPILER_ROOT/usr/bin/g++-14"
    export PATH="$RC_COMPILER_ROOT/usr/bin:$PATH"
fi
TMPDIR="$RC_WORK" "$RC_CXX" -std=c++17 -O2 -pipe "$APP_DIR/testes/falso-pipewire.cpp" -o "$RC_WORK/falso-pipewire"
for RC_TOOL in pw-dump wpctl pw-metadata pw-cat; do ln -s falso-pipewire "$RC_WORK/$RC_TOOL"; done
export RC_AUDIO_TEST_DIR="$RC_WORK"
export PATH="$RC_WORK:$PATH"
"$APP_DIR/audio-painel-rc" --testes-io

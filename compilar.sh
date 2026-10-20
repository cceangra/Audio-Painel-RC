#!/usr/bin/env bash
# Compila somente na pasta do app. Não instala pacotes ou altera o desktop.
set -euo pipefail
APP_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
RC_COMPILER_ROOT="${RC_GXX_ROOT:-$HOME/Apps/G++/dependencias/rootfs}"
for RC_PROC in /proc/[0-9]*/exe; do
    RC_EXE="$(readlink -- "$RC_PROC" 2>/dev/null || true)"
    if [[ "$RC_EXE" == "$APP_DIR/audio-painel-rc" || "$RC_EXE" == "$APP_DIR/audio-painel-rc (deleted)" ]]; then
        printf '%s\n' 'Feche o painel, a central e o medidor desta pasta antes de recompilar.' >&2
        exit 1
    fi
done
if [[ -n "${CXX:-}" ]]; then
    RC_CXX="$CXX"
elif [[ -x "$RC_COMPILER_ROOT/usr/bin/g++-14" ]]; then
    RC_CXX="$RC_COMPILER_ROOT/usr/bin/g++-14"
    export PATH="$RC_COMPILER_ROOT/usr/bin:$PATH"
elif [[ -x "$HOME/Apps/G++/bin/g++" ]]; then
    RC_CXX="$HOME/Apps/G++/bin/g++"
elif command -v g++ >/dev/null 2>&1; then
    RC_CXX="$(command -v g++)"
else
    printf '%s\n' 'G++ não encontrado. Nenhum pacote foi instalado.' >&2
    exit 1
fi
RC_BUILD_DIR="$(mktemp -d "$APP_DIR/.compilacao-XXXXXX")"
RC_TEMP="$RC_BUILD_DIR/audio-painel-rc"
cleanup() {
    local RC_BUILD_FILE
    shopt -s nullglob dotglob
    for RC_BUILD_FILE in "$RC_BUILD_DIR"/*; do
        if [[ -f "$RC_BUILD_FILE" || -L "$RC_BUILD_FILE" ]]; then unlink -- "$RC_BUILD_FILE" 2>/dev/null || true; fi
    done
    rmdir -- "$RC_BUILD_DIR" 2>/dev/null || true
}
trap cleanup EXIT
printf 'Compilador: %s\n' "$RC_CXX"
mapfile -d '' RC_SOURCES < <(find "$APP_DIR/src" -type f -name '*.cpp' -print0 | sort -z)
if ((${#RC_SOURCES[@]} == 0)); then
    printf '%s\n' 'Nenhum arquivo C++ encontrado.' >&2
    exit 1
fi
RC_OTIMIZACAO="${RC_OTIMIZACAO:--O2}"
case "$RC_OTIMIZACAO" in -O0|-O1|-O2|-O3|-Os|-Oz) ;; *) printf "%s\n" "Otimização inválida: $RC_OTIMIZACAO" >&2; exit 1;; esac
TMPDIR="$RC_BUILD_DIR" "$RC_CXX" -std=c++17 "$RC_OTIMIZACAO" -pipe -DNDEBUG -Wall -Wextra -Wno-misleading-indentation \
    -fno-ident -ffunction-sections -fdata-sections \
     -I"$APP_DIR/src/incluir" "${RC_SOURCES[@]}" -Wl,--gc-sections -Wl,-z,relro,-z,now -o "$RC_TEMP" -ldl -pthread
TMPDIR="$RC_BUILD_DIR" "$RC_CXX" -x c -Os -pipe "$APP_DIR/ferramentas/launcher.c" -Wl,-z,relro,-z,now -o "$RC_BUILD_DIR/Iniciar"
chmod 755 "$RC_TEMP"
if command -v strip >/dev/null 2>&1; then strip --strip-unneeded "$RC_TEMP"; fi
mv -f -- "$RC_TEMP" "$APP_DIR/audio-painel-rc"
chmod 755 "$RC_BUILD_DIR/Iniciar"
if command -v strip >/dev/null 2>&1; then strip --strip-unneeded "$RC_BUILD_DIR/Iniciar"; fi
mv -f -- "$RC_BUILD_DIR/Iniciar" "$APP_DIR/Iniciar"
printf '%s\n' 'Compilação concluída. GTK3 e GLib são carregados do sistema; não precisa dos headers -dev.'

# Atualiza a lista segura de arquivos que o instalador pode copiar ou remover.
python3 "$APP_DIR/scripts/gerar-manifesto.py"

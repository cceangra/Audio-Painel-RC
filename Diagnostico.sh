#!/usr/bin/env bash
# Somente leitura; não grava relatório, não captura som e não troca volumes.
set -euo pipefail
APP_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
bash "$APP_DIR/iniciar.sh" --verificar
printf '\n%s\n' 'TESTE DE VOZ — DISPONIBILIDADE DO PW-CAT (SOMENTE LEITURA)'
if command -v pw-cat >/dev/null 2>&1; then
    pw-cat --help
else
    printf '%s\n' 'pw-cat não foi encontrado. O teste de retorno precisa dessa ferramenta do PipeWire.'
fi

#!/usr/bin/env python3
"""Gera listas de conteúdo do pacote sem incluir dados pessoais do usuário."""
import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ROOT_FILES = (
    "IDENTIDADE-RC", "Iniciar", "audio-painel-rc", "README.md", "LEIA-ME.md",
    "CHANGELOG.md", "LICENSE", "preferencias.json", "compilar.sh",
    "iniciar.sh", "Instalar.sh", "Desinstalar.sh", "Configuracoes.sh",
    "Diagnostico.sh", "Restaurar-KMix.sh", "Substituir-KMix.sh",
    "Teclado.sh", "testar.sh", "testar-instalacao.sh",
    "testar-integracao.sh", "medir-ram.sh",
)
DIRECTORIES = ("src", "ferramentas", "icones", "previas", "docs", "scripts", "testes")
MANIFEST = "ARQUIVOS.txt"
CHECKSUMS = "SHA256SUMS.txt"


def arquivos():
    resultado = set(ROOT_FILES)
    for directory in DIRECTORIES:
        folder = ROOT / directory
        if not folder.is_dir():
            raise SystemExit(f"Diretório ausente: {directory}")
        for file in folder.rglob('*'):
            if file.is_symlink():
                raise SystemExit(f"Link simbólico inesperado: {file}")
            if not file.is_file():
                continue
            relative = file.relative_to(ROOT)
            if any(part.startswith(('.', 'io-', 'instalador-', 'remocao-', 'protecao-'))
                   for part in relative.parts):
                continue
            if '__pycache__' in relative.parts or file.suffix in ('.pyc', '.o'):
                continue
            resultado.add(relative.as_posix())
    resultado.update((MANIFEST, CHECKSUMS))
    for item in resultado:
        p = ROOT / item
        if item not in (MANIFEST, CHECKSUMS) and (not p.is_file() or p.is_symlink()):
            raise SystemExit(f"Arquivo do pacote ausente: {item}")
    return sorted(resultado)


def conteudos():
    nomes = arquivos()
    manifest = '\n'.join(nomes) + '\n'
    checksums = []
    for nome in nomes:
        if nome == CHECKSUMS:
            continue
        data = manifest.encode() if nome == MANIFEST else (ROOT / nome).read_bytes()
        checksums.append(f"{hashlib.sha256(data).hexdigest()}  {nome}")
    return manifest, '\n'.join(checksums) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--verificar', action='store_true', help='Verifica o manifesto existente')
    args = parser.parse_args()
    manifest, sums = conteudos()
    if args.verificar:
        if ((ROOT / MANIFEST).read_text() != manifest or
                (ROOT / CHECKSUMS).read_text() != sums):
            raise SystemExit('Manifesto ou SHA256SUMS divergente. Compile novamente.')
        print(f'Manifesto e hashes verificados ({len(manifest.splitlines())} arquivos).')
    else:
        (ROOT / MANIFEST).write_text(manifest)
        (ROOT / CHECKSUMS).write_text(sums)
        print(f'Manifesto e hashes atualizados ({len(manifest.splitlines())} arquivos).')


if __name__ == '__main__':
    main()

# Guia de GitHub — Áudio Painel RC

## Onde estão os arquivos?

Abra a aba **Code** do repositório e navegue pelas pastas. O `README.md` explica o projeto; `docs/` documenta os módulos; `src/` é a pasta reservada ao código-fonte.

## O que significam os commits?

Cada commit registra um conjunto de alterações, com descrição. Abra **Commits** para consultar o histórico e recuperar uma versão anterior.

## Como manter versões seguras?

- **main**: versão considerada estável.
- **melhoria-interface**: ajustes visuais.
- **melhoria-audio**: desenvolvimento de funções de áudio.
- **melhoria-instalador**: alterações na instalação e remoção.

Use **Pull requests** para comparar uma melhoria com `main` antes de mesclar. Para uma versão publicada, use **Releases** e tags como `v0.3.0`.

## Como atualizar com o ChatGPT?

Peça explicitamente a alteração desejada e o repositório `cceangra/Audio-Painel-RC`. Prefira mudanças isoladas, com testes, documentação e um commit descritivo. O código C++ deve ser importado integralmente antes de realizar alterações funcionais.

## Importante

O código-fonte no GitHub **não instala nenhum programa automaticamente** no computador. Ter o projeto aqui significa manter o código versionado, não executar o aplicativo.

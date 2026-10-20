# Arquitetura do Áudio Painel RC

## Separação de responsabilidades

- **src/aplicacao/** — ponto de entrada, inicialização e coordenação do programa.
- **src/interface/** — tema visual, componentes, janelas, controles e preferências gráficas.
- **src/audio/** — dispositivos, volumes, fluxos e teste do microfone com retorno atrasado.
- **src/sistema/** — integração ao Linux, teclado, ambiente e plataforma.
- **src/instalador/** — assistente de instalação, atualização e desinstalação.
- **src/ferramentas/** — diagnósticos e ferramentas auxiliares.
- **src/incluir/** — cabeçalhos e contratos entre módulos.
- **src/testes/** — testes automatizados da lógica.
- **icones/** — recursos SVG da aplicação.
- **previas/** — capturas de tela para documentação.
- **docs/** — manuais e decisões técnicas.

## Como editar

Para alterar **apenas cores e estilo**, comece por `src/interface/tema.cpp` e seu cabeçalho. Para modificar controles, trabalhe em `src/interface/`. Para alterar rotas e regras de áudio, trabalhe em `src/audio/`.

## Regras

1. A interface chama funções de áudio; ela não deve duplicar regras de áudio.
2. Alterações no instalador exigem revisão da lista de arquivos instalados e removidos.
3. Evite criar arquivos externos sem informar e registrar seus caminhos.
4. Preserve suporte ao Q4OS/Debian e não assuma que todos usam Trinity.
5. Atualizações devem passar pelos testes antes de entrar na versão estável.

**Status:** documentação e organização inicial. A importação completa das fontes será registrada separadamente.

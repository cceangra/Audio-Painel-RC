# Arquitetura do Áudio Painel RC

Esta organização foi preparada a partir da versão **0.3.0**, preservando a lógica
original. Todas as pastas e instruções de manutenção são documentadas em português.

## Onde alterar cada coisa

| Local | Responsabilidade | Exemplos |
|---|---|---|
| `src/interface/tema.cpp` | Aparência: CSS, tema escuro/claro, cores, bordas, opacidade e fontes | Ajustar visual sem mexer no PipeWire |
| `src/interface/componentes.cpp` | Componentes GTK comuns | Botão, rótulo, seção e caixa |
| `src/interface/popup.cpp` | Janela flutuante de volume | Posição, abertura, fechamento |
| `src/interface/settings.cpp` | Telas de configurações | Controles, opções e teste de voz na interface |
| `src/interface/controls.cpp` | Widgets de volume e aplicativos | Slider, ícone de mudo |
| `src/interface/gtk_api.cpp` | Carregamento da API GTK3 | Chamadas gráficas |
| `src/audio/model.cpp` | Modelo do áudio PipeWire | Dispositivos, rotas, estado e planos |
| `src/audio/process.cpp` | Monitoramento e comandos de áudio | Execução do backend |
| `src/audio/voice_test.cpp` | Teste de microfone com atraso de 1 segundo | Buffer PCM e processo de teste |
| `src/sistema/` | Serviços do Linux e integrações | Teclas, desktops, autostart, KMix |
| `src/instalador/installation.cpp` | Regras de instalação, cópia segura e remoção | Arquivos e rastreamento |
| `src/instalador/setup.cpp` | Assistente gráfico do instalador | Telas e diálogos da instalação |
| `src/aplicacao/` | Coordenação e ponto de entrada | `main`, ciclo de vida e comunicação |
| `src/testes/` | Testes automáticos da aplicação | Casos de regressão |
| `src/incluir/` | Interfaces internas C++ (`.hpp`) | Contratos entre módulos |

## Separação de layout e funcionamento

1. Para cores, tema, CSS: **`src/interface/tema.cpp`**.
2. Para botões reutilizáveis: **`src/interface/componentes.cpp`**.
3. Para o desenho de cada tela: outros arquivos em **`src/interface/`**.
4. Para comportamento de áudio: **`src/audio/`**.
5. Para instalação segura: **`src/instalador/`**.

**Limite atual:** alguns callbacks de comportamento continuam nas classes de interface,
principalmente `settings.cpp` e `controls.cpp`. Os diretórios já separam as áreas
e o CSS saiu de `app.cpp`, mas esta atualização não faz uma reescrita completa para
uma arquitetura MVC/MVP. Essa etapa exige testes de integração antes de prosseguir.

## Compilação, manifesto e segurança

- `bash compilar.sh` reúne os `.cpp` das subpastas, compila o aplicativo e o iniciador.
- `scripts/gerar-manifesto.py` gera `ARQUIVOS.txt` e `SHA256SUMS.txt` para o instalador.
- O manifesto lista **somente** arquivos permitidos do produto e não inclui dados pessoais,
  pastas `.git`, históricos de instalação nem alterações do desktop.
- O repositório versiona código-fonte; binários e manifestos gerados são ignorados pelo Git.
- Os binários de distribuição devem ser publicados em uma **Release** com o ZIP do instalador.
- A alteração do tema não deve executar comandos PipeWire, e alterações de áudio não devem
  depender das cores ou do layout.

## Comandos de verificação

```bash
bash compilar.sh
bash testar.sh
bash testar-instalacao.sh
bash testar-integracao.sh
```

Os testes usam ambientes simulados, mas ainda é necessário validar a interface gráfica
na sessão real com GTK3, PipeWire e Trinity.

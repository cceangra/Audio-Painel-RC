# Manual do painel de áudio

## Interface e personalização

- Layout 4 padrão: master e microfone lado a lado; mixer no centro; saída e
  entrada na faixa inferior. A lista de aplicativos tem rolagem.
- Layout 1 compacto, com mixer recolhível; layout 3 com abas Geral,
  Aplicativos e Microfone. Todos mantêm acesso aos controles.
- Temas escuro, claro e acompanhamento do GTK do sistema; seis cores de
  destaque e seletor de cor personalizada.
- Largura, tamanho do texto, densidade, cantos, opacidade do fundo, quantidade
  de linhas, distância da barra, margem e espera para esconder ajustáveis.
- Transparência somente no fundo, quando há composição ativa no Trinity.
  Texto, sliders e botões permanecem opacos. Sem composição, o fundo é opaco.
- O painel é posicionado pela geometria do ícone e limitado à área de trabalho.
  A posição exata do ícone continua sob controle da bandeja do Trinity.
- Ao sair da área com o mouse, o painel esconde após 350 ms por padrão, com
  margem de 12 px. Menus, seletores abertos e arrastes protegem a interação.
  A região também permite atravessar do ícone até o painel.
- Animação de abertura de 120 ms, opcional. Depois de 20 s oculto, os widgets
  do painel são descartados; a próxima abertura os recria. Ajuste esse tempo
  para 0 se preferir mantê-los prontos. Não há animação ou timer de mouse em
  execução enquanto o painel está oculto.

A opção Sistema usa o modo claro/escuro detectado no GTK ao iniciar. Depois de
uma mudança externa de tema, reinicie o aplicativo para atualizar essa detecção.

Preferências ficam em `preferencias.json`, na própria pasta. Mudanças são
aplicadas entre os processos por eventos do sistema de arquivos, sem polling.

## Áudio

O volume vai até 100%. Ajustar volume não desativa o mudo. O botão de som ou
microfone silencia/ativa o controle correspondente. A roda sobre o ícone
ajusta o master em passos de 5%.

O mixer agrupa fluxos pelo nome do aplicativo e PID/cliente. O slider exibe o
maior volume do grupo e aplica o ajuste a todos os fluxos. Mudo também atua
no grupo. O menu `⋮` permite selecionar uma saída ou acompanhar o padrão.
As configurações oferecem os mesmos recursos, com seletores sempre visíveis.

P2 e alto-falantes são portas do mesmo dispositivo analógico. O painel lê
rotas e perfis reais, sinaliza portas desconectadas e não inventa dispositivos.
HDMI e outras saídas já expostas pelo PipeWire aparecem no seletor.

A automação P2 → HDMI → alto-falantes existente no notebook é preservada.
Ela pode reverter uma seleção manual em cerca de 2 segundos. Este programa
não a pausa, não a reescreve e não cria uma segunda automação.

## Microfone, filtro e teste da própria voz

O controle principal atua na fonte padrão. Quando ela é o filtro de ruído,
o seletor do painel troca somente a porta física associada, mantendo o filtro.
O programa identifica a relação pelo fluxo de captura e seu destino/ligação.
Se não identificar a origem, informa isso e não escolhe uma entrada por conta
própria. O arquivo e o plugin RNNoise existentes não são modificados.

A página **Microfone** da central permite escolher explicitamente a fonte
padrão, selecionar a porta física e ajustar o ganho antes do filtro. Este
último controle fica na seção recolhível ao final da página. O mudo físico é
separado do mudo do filtro. Se o Capture Switch estiver desligado, aparece
“Ativar entrada física”; nenhuma captura é ligada automaticamente.

Para o teste parecido com o do PS4:

1. Conecte o fone e abra **Configurações → Microfone**.
2. Confira a fonte e a saída. Ative a entrada/fonte pelo controle se estiver
   silenciada; comece com volume baixo.
3. Clique **Iniciar teste** e fale. Sua voz retorna com **1 segundo de buffer**,
   acrescido da latência dos dispositivos e do PipeWire.
4. Use **Parar**. O teste também para ao sair da página, minimizar/fechar a
   central, mudar de fonte, saída ou porta física, ou completar 30 segundos.

Use fones: o retorno em alto-falantes pode causar apito. O teste não faz uma
avaliação automática de defeitos; permite ouvir o sinal e observar seu nível.

São usados PCM mono de 48 kHz/16 bits e um buffer circular de 96.000 bytes,
com fila de saída limitada. O teste usa o `pw-cat` do PipeWire, por pipes, e
**não salva gravações**. Dois processos temporários existem somente durante
o teste e são encerrados ao parar. Não cria loopback persistente, módulo,
serviço, configuração de áudio ou arquivo de som. Em `--demo` não há captura.

## RAM e processos

| Versão | PSS informado pelo usuário |
|---|---:|
| Python completa | 40,47 MiB |
| C++ experimental 0.1 | 13,75 MiB |
| C++ 0.2 completa (referência mais recente do relatório) | 17,30 MiB, incluindo 1,60 MiB do pw-dump |
| C++ 0.3 com instalador e teclado opcional | Ainda precisa medir no notebook |

Não há garantia de manter os 13,75 MiB ao acrescentar todos os recursos.
O cenário exato das duas medições anteriores não foi informado neste pedido.

A bandeja mantém um `pw-dump -m -N`, por eventos. A central tem seu próprio
monitor enquanto está aberta, mantendo independência e recuperação de falhas.
Fechá-la encerra seu monitor. Não há daemon próprio, thread de polling ou
servidor adicional. Comandos temporários são assíncronos e serializados; sliders
têm debounce e comandos pendentes do mesmo volume são condensados.

Somente dados de áudio necessários, rotas, padrões e links enxutos permanecem
no cache. O JSON é processado objeto por objeto e descartado. Nós removidos
saem do cache; estado idêntico não provoca atualização da interface.
Não são usados coleta forçada de memória, `malloc_trim` ou descarregamento
de bibliotecas para melhorar artificialmente uma medida.

Para medir, com o programa aberto:

```bash
bash medir-ram.sh --cenario "bandeja" --espera 5
bash medir-ram.sh --cenario "painel aberto" --espera 8
```

O medidor não abre o painel. Após iniciar a coleta, mova o mouse até o ícone,
abra o painel e deixe-o dentro da área para medir o cenário aberto.
Compare também antes do primeiro clique, após 20 s oculto, com central
aberta/minimizada e com teste de voz ativo. Mantenha os mesmos aplicativos e
dispositivos. `--amostras 5 --intervalo 1` são os padrões.

O medidor lê `/proc` e informa PSS/RSS/privada e CPU. Inclui os processos desta
pasta e seus auxiliares, inclusive `pw-dump` e `pw-cat`. Não inclui o PipeWire,
KMix ou automação HDMI preexistentes. Ele próprio é excluído. Não grava dados.
Compare principalmente a soma de PSS; RSS conta páginas compartilhadas de novo.


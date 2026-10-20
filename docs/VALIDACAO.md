# Validação · Áudio Painel RC C++ 0.3.0

## Resultado e escopo

Executável Linux x86-64, código completo, lançador Iniciar e assistentes
incluídos. A instalação usa cópia verificada, registro de transação,
recuperação e recolhimento da origem. O modo normal instala de verdade;
`--demo` é a prévia usada na revisão visual e não aplica a instalação/remoção.

| Verificação | Resultado |
|---|---|
| G++ 13.3, C++17, compilação sem headers GTK/X11 de desenvolvimento | Concluída; executáveis incluídos |
| Sintaxe Bash | Aprovada |
| Modelo de áudio, JSON, rotas, mudo físico/filtro, IDs, geometria e PCM | 44 testes aprovados |
| Instalação, remoção, atualização, cópia, arquivos externos e recuperação | 23 testes aprovados |
| I/O PipeWire simulado: monitor, volume/mudo, destino, ID reciclado, PCM e hotplug | 7 verificações aprovadas |
| Atalhos X11 em Xvfb: conflito BadAccess, eventos/passo/Caps Lock e liberação | 3 verificações aprovadas |
| AddressSanitizer + UndefinedBehaviorSanitizer | Núcleo, instalação e I/O aprovados em cópia local de teste; sem erros de acesso/UB detectados; detecção de vazamentos desativada |
| Interface nativa em X11 virtual, 1366 × 768 | Telas azul-escuras, caminho, sistema, teste parcial de teclas, integração, revisão e cancelamento conferidos |
| ZIP desta compilação ao lado da extração | Reconhecido automaticamente na tela; seletor de pasta aberto/cancelado |
| Remoção em pasta de 11 caracteres no sistema local | Concluída; arquivos próprios e pasta removidos |
| Proteção da demonstração | Instalação/remoção da prévia bloqueadas; comandos persistentes com --demo recusados |
| Preservação de funcionalidades | Layouts 1/3/4, mixer, filtro, microfone, rotas, teste de voz e central mantidos |

Os testes de instalação usam HOME fictício e pastas privadas. Cobrem caminhos
com espaços e acentos, ZIP recolhido/preservado, arquivo pessoal, origem mantida,
três atalhos opcionais, edição posterior, atalho de outro programa, rollback,
cancelamento, atualização preservando preferências e teclado, caminhos que
saem da pasta, links, reparação após mover, recuperação com arquivo editado,
interrupção antes da identidade e temporário de integração preparado.

Sob sanitizadores, as verificações de remoção foram executadas em cópia num
sistema de arquivos temporário local. O armazenamento replicado do executor
apresentou inconsistências na reaparição de diretórios removidos. Os
executáveis de análise e seus temporários não entram no ZIP.

## Limites do teste gráfico

O executor de desenvolvimento disponibiliza somente UID 0. A aplicação
recusa a interface normal como root; por isso a revisão visual usou --demo,
e o motor real de instalação/remoção foi validado separadamente pelos testes
com HOME fictício. A sessão não possui Trinity, PipeWire/WirePlumber real ou
os dispositivos do notebook. A criação de sockets AF_UNIX também é bloqueada;
a prévia do painel usa --demo --sem-ipc.

Isso não equivale a validar o duplo clique, o fluxo completo de instalação
por um usuário normal, a bandeja do Trinity ou o áudio real no notebook.
O instalador, o desinstalador e suas transações estão implementados; essas
integrações finais ainda precisam ser conferidas no equipamento.

## RAM · referências separadas

O relatório do usuário registra a versão C++ anterior completa com
**15,70 MiB PSS** no processo principal + **1,60 MiB** do pw-dump =
**17,30 MiB PSS** no notebook. Python (40,47 MiB) e protótipo C++ (13,75 MiB)
são referências anteriores com recursos/cenários diferentes.

Comparação de desenvolvimento: mesma sessão Xvfb, tema/layout padrão,
mesmos dados simulados, processos encerrados entre cenários, mediana de
5 amostras, intervalo de 0,5 s após 2 s de espera:

| Cenário de demonstração | 0.2.0 PSS | 0.3.0 PSS | Diferença |
|---|---:|---:|---:|
| Somente bandeja | 38,29 MiB | 38,39 MiB | +0,10 MiB |
| Painel aberto | 41,03 MiB | 41,14 MiB | +0,11 MiB |

**É uma simulação sem PipeWire real e sem controle direto de teclas.** Os
valores absolutos não são comparáveis ao notebook e não garantem a mesma
variação nele. O instalador encerra ao concluir, e o teclado direto usa o
processo principal; não foi acrescentado serviço próprio permanente.

Use medir-ram.sh no notebook nas mesmas condições da versão anterior:
bandeja, painel, central aberta/minimizada e teste de voz. O medidor soma
os auxiliares desta instalação, incluindo pw-dump e pw-cat quando ativos.
Não há novo número real de PSS do notebook nesta entrega.

## Compatibilidade do binário

- Compilação: Ubuntu 24.04, G++ 13.3.0, Linux x86-64.
- Requer GLIBC_2.38, GLIBCXX_3.4.29 e CXXABI_1.3.13, ou versões superiores.
- GTK3, GLib, GObject, Cairo e X11 são bibliotecas do sistema. Não há cópias
  dessas bibliotecas, Python ou navegador dentro do aplicativo.
- O G++ 14 portátil é contemplado pelo script, mas não foi executado aqui.
- Outras arquiteturas, desktops/compositores e versões antigas da glibc não
  foram validados. Wayland é parcial; PulseAudio independente não possui backend.
- Atalhos existentes do desktop não são transferidos automaticamente.

## Pendências no equipamento

1. Duplo clique/permissões do gerenciador de arquivos e instalação gráfica
   completa como usuário normal, seguida de abertura e desinstalação.
2. Sessão real Trinity/Kicker, bandeja e comunicação de instância única.
3. Volume, mudo, microfone físico, filtro, P2, HDMI, mixer e automação existente.
4. Qualidade/latência do retorno de voz; o buffer PCM de 1 segundo foi testado,
   mas o hardware e o servidor acrescentam sua latência.
5. Teclas físicas Fn, conflitos do desktop real e controle direto, se solicitado.
6. Login com autostart opcional; integração explícita/restauração do KMix.
7. Medição de RAM com ferramentas e dispositivos reais.

## Bases preservadas

| ZIP analisado na versão 0.2 | SHA-256 |
|---|---|
| Audio-Painel-RC-CPP-Teste-v0.1(1).zip | 520d3dd88089f56881b63157ce5f8556fb8ce6c1466a7b958e4098ca87b2c524 |
| Audio-Painel-RC-Otimizado-v0.1.1(2).zip | 6d403ff74b7be0cdea6fb6b114f0c0e9d593f1a51c21bb6ecf57e9e11b882fa5 |

O pacote 0.3 continua essa reimplementação, mantendo a comparação de recursos
em COMPARACAO.md. SHA256SUMS.txt confere os arquivos distribuídos antes de
editar preferências ou recompilar. Não inclui a si próprio na soma.

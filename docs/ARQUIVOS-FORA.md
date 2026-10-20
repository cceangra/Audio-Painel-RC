# Arquivos fora da pasta do aplicativo

## Padrão da instalação

Nenhum arquivo próprio externo é obrigatório. Os três atalhos do assistente
começam desmarcados. Executável, instalador, lançador `Iniciar`, desinstalador,
código, preferências, registros, backups próprios e temporários de cópia ficam
na pasta escolhida. A tela de revisão informa os caminhos antes de aplicar.

| Arquivo externo | Quando | Por que precisa ficar fora | Remoção |
|---|---|---|---|
| `$TDEHOME/Autostart/Audio-Painel-RC-CPP.desktop` (normalmente `~/.trinity/Autostart/`) | Autostart solicitado no Trinity | O desktop lê essa pasta ao entrar na sessão | Identidade/pasta e SHA-256 registrados; remover somente o arquivo próprio |
| `$XDG_CONFIG_HOME/autostart/Audio-Painel-RC-CPP.desktop` (normalmente `~/.config/autostart/`) | Autostart solicitado fora do Trinity | Diretório de login do padrão XDG | Mesmo registro; não sobrescrever outra instalação |
| `$XDG_DATA_HOME/applications/Audio-Painel-RC-CPP.desktop` (normalmente `~/.local/share/applications/`) | Opção adicionar ao menu | O menu de aplicativos consulta esse diretório | Remover se o conteúdo ainda corresponde ao criado |
| `$XDG_DESKTOP_DIR/Audio-Painel-RC-CPP.desktop` | Atalho na área de trabalho solicitado | O gerenciador da área de trabalho exibe arquivos desse diretório | Remover se corresponder ao registro; o desktop pode exigir confiança manual |
| `$TDEHOME/share/config/kmixrc` | Somente no comando separado **Substituir KMix**, depois de validar o novo ícone | Configuração compartilhada lida pelo KMix | Restaurar apenas a chave raiz Autostart, respeitando mudanças posteriores; não apagar opções alheias |
| `.Audio-Painel-RC-<identificador>.tmp`, nos mesmos diretórios dos atalhos ou do kmixrc | Brevemente, enquanto uma integração autorizada é gravada | A publicação atômica precisa ocorrer no mesmo sistema de arquivos do destino | Nome exato registrado antes da gravação; normalmente desaparece na confirmação; temporários interrompidos são rastreados para limpeza |

`HOME`, `TDEHOME` e os diretórios XDG são os da sessão do usuário; não há um
caminho fixo para `/home/rc`. O diretório da área de trabalho é lido do formato
XDG sem executar o conteúdo do arquivo. Se não estiver definido, a sugestão é
`~/Desktop`; o caminho real aparece no assistente.

Diretórios pais criados para os atalhos também são registrados e só são
removidos se estiverem vazios. O mesmo vale para pais da pasta de instalação
que precisaram ser criados. Não há arquivo de recuperação externo obrigatório.

## Registros que o desinstalador consulta

Todos ficam dentro do aplicativo:

- `instalacao.json`: pasta, versão, arquivos próprios, ZIPs, arquivos pessoais
  recolhidos, diretórios pais criados e eventuais restos na origem.
- `integracoes.json`: caminhos absolutos, tipo, estado, conteúdo esperado por
  SHA-256, temporários registrados e diretórios de cada atalho.
- `preferencias.json`: preferências e registro da troca opcional do KMix.
- `integracao-kmix/kmixrc.antes`: backup local quando o KMix foi alterado.
- `RELATORIO-INSTALACAO.txt`: resumo legível e lista final de arquivos externos.
- `.instalacao-pendente-*/transacao.json`: registro temporário para recuperar
  uma cópia/atualização interrompida; removido ao concluir.

Um atalho editado depois da instalação interrompe a remoção, preservando o
aplicativo e os registros para revisão. Isso evita apagar uma alteração feita
por você. Se um registro ou backup estiver incompleto ou tiver sido editado,
a restauração pode exigir revisão manual. Não há promessa de restauração
perfeita após apagar ou alterar os próprios registros.

## Recolhimento da origem e do ZIP

O assistente pode recolher o ZIP selecionado para `instalador/`. Também pode
recolher os arquivos da pasta original, **somente após verificar a cópia**.
Arquivos pessoais adicionais vão para `instalador/arquivos-preservados/` e
continuam preservados na desinstalação. ZIPs nunca são apagados por ela.

Se você optar por manter a origem, se um arquivo mudar durante a cópia ou se
houver um link simbólico adicional, o caminho e o motivo aparecem no relatório
e no registro. O programa não segue o link para mover arquivos de outro local.
Arquivos de origem deliberadamente preservados e arquivos pessoais não são
apagados pelo desinstalador. Uma falha ao recolher um arquivo próprio é
registrada com hash, permitindo conferir sua propriedade antes de removê-lo.

O instalador reconhece automaticamente apenas um ZIP correspondente à mesma
compilação ao lado da extração. Havendo vários candidatos, pede seleção pelo
campo ZIP. Ele não procura nem move arquivos arbitrários em todo o computador.

## Arquivos e estados pertencentes a outros componentes

| Local | Motivo para permanecer |
|---|---|
| `~/.local/state/wireplumber/` ou diretório de estado do servidor | O servidor pode salvar volumes, rotas e destinos escolhidos. É estado compartilhado com outros controles; o app não o apaga |
| Caches de fontes, GTK e desktop | Geridos pelos componentes do sistema; não são arquivos próprios do painel |
| Configurações de atalhos alteradas manualmente na ferramenta do desktop | A versão 0.3 não as modifica. Qualquer mudança manual é administrada por essa ferramenta |
| `/etc/pipewire/pipewire.conf.d/99-filtro-ruido-noise.conf` e `/opt/Apps/Filtro ruído noise/App/librnnoise_ladspa.so` no notebook diagnosticado | Configuração e plugin RNNoise já existentes. Não foram criados por este pacote e não são movidos ou apagados |
| `~/Documentos/Scripts-Automacao/HDMI-Automatico/` no notebook | Automação independente já existente; preservada |
| `~/Apps/G++/` | Compilador portátil já existente, usado apenas para recompilar; preservado |

A interface usa GSettings em memória e desativa os itens recentes do GTK
somente neste processo, para manter as próprias preferências na pasta.
Não cria regra udev, serviço persistente, arquivo de gravação, PID ou socket
no disco. O IPC é um socket abstrato liberado pelo kernel. O controle direto
no X11 usa reservas temporárias no processo principal, liberadas ao sair.

# Comparação das bases e cobertura do C++ 0.2

Foram lidos o `main.cpp`, os três scripts e o LEIA-ME do protótipo C++ enviado.
O executável anexado foi inspecionado como ELF; a entrega usa uma nova
compilação do código-fonte deste pacote.

A referência Python recuperada foi `Audio-Painel-RC-Otimizado-v0.1.1(2).zip`:
`painel.py`, `audio.py`, `integracao.py`, scripts, preferências, testes e
documentação. Não foi inferida completude a partir do LEIA-ME do protótipo.
As somas SHA-256 dos ZIPs de entrada aparecem em `VALIDACAO.md`.

| Recurso | C++ experimental 0.1 | C++ 0.2 |
|---|---|---|
| Master, volume e mudo | Comandos síncronos, no abrir | Assíncronos, eventos, limite de 100%, sem desmutar ao ajustar |
| Microfone padrão | Volume e mudo da fonte | Preservado, com fonte e porta física distintas |
| HDMI / P2 | Lista textual de sinks | Nós, portas, perfil ativo e disponibilidade reais |
| Fone e alto-falante no mesmo sink | Sem distinção de porta | Rotas do dispositivo analógico |
| Microfone físico / Capture Switch | Ausente | Mudo e ganho antes do filtro; ativação explícita |
| Filtro de ruído existente | Atuação na fonte padrão | Relação filtro–captura–entrada identificada; seleção de porta preserva filtro |
| Mixer por aplicativo | Ausente | Agrupado por nome e processo; volume e mudo de todos os fluxos do grupo |
| Saída por aplicativo | Ausente | Menu ⋮ e seletor na central; opção acompanhar padrão |
| Troca de saída global e fluxos | Apenas set-default | Seleção explícita também solicita movimento dos fluxos atuais |
| Monitor PipeWire / hotplug | Ausente | pw-dump por eventos, reconexão com espera progressiva |
| Segurança de IDs | Validação textual numérica | Referência id+serial+nome+classe; snapshot recente antes dos comandos |
| Layouts 1, 3 e 4 | Estruturas de teste | Compacto, abas e desenho horizontal pedido pelo usuário |
| Tema | Dependente do sistema | Escuro padrão, claro, sistema, cor e densidade configuráveis |
| Fundo transparente | Ausente | RGBA com composição; controles opacos; fallback sem composição |
| Ancoragem no ícone | Ausente | Geometria do tray e limites do monitor |
| Esconder ao sair com o mouse | Ausente | Área com margem/corredor, atraso configurável e proteção de interações |
| Roda no ícone | Ausente | Passos de 5% no master |
| Central independente | Só seleção de layout | Páginas de dispositivos, microfone, apps, aparência, diagnósticos e sobre |
| Construção sob demanda | Popup no clique | Popup no clique, páginas ao acessar e descarte do popup oculto |
| Preferências | Um layout em arquivo texto | JSON atômico local, bloqueio e aviso de mudanças por eventos |
| Instância única | Ausente | Socket abstrato por usuário, pasta e função; peer UID verificado |
| Substituir/restaurar KMix | Ausente | Mantido como recurso estritamente opcional e explícito |
| Inicializar com Trinity | Ausente | Opção desligada por padrão, atalho próprio rastreado |
| Diagnóstico wpctl | --version incompatível | dpkg-query + wpctl --help |
| Teste da própria voz | Ausente nas duas bases | Retorno com buffer de 1 s, indicador de sinal e parada automática |
| Medidor RAM | Só binário principal | PSS/RSS/privada/CPU, central e auxiliares incluídos |
| Desinstalação | Apagar pasta manualmente | Script com lista própria, proteção de ZIP/arquivos pessoais e restauração opcional |

A troca automática do KMix não foi portada como comportamento de início.
Ela depende do comando explícito do usuário. A automação HDMI e o RNNoise
continuam com seus arquivos/serviços atuais, sem uma segunda implementação.

O novo código usa GTK3 porque esse runtime já existe no notebook e foi o
toolkit do teste de RAM em C++. Não há uma conversão adicional para TQt3 neste
ZIP, nem promessa de igualar o PSS do KMix. O C++ retira Python/GI do processo
da aplicação; o custo do GTK e dos monitores ainda precisa ser medido no Q4OS.

## Módulos

| Fonte | Responsabilidade |
|---|---|
| json.hpp | JSON estrito e enquadramento incremental de objetos pw-dump |
| model.hpp / model.cpp | Grafo enxuto, volumes, rotas, filtros, grupos e planos de comando |
| platform.hpp / platform.cpp | Preferências, gravação atômica, processos, IPC, autostart e diagnóstico |
| process.hpp / process.cpp | Pipes/watchers GLib, filas assíncronas, monitor e recuperação |
| gtk_api.hpp / gtk_api.cpp | Assinaturas ABI e carregamento GTK3/GLib, sem GTK-dev |
| app.cpp | Ciclo da bandeja, tema, eventos, IPC e recarga |
| controls.cpp | Sliders, seletores e mixer reutilizados entre painel e central |
| popup.cpp | Três layouts, posicionamento, animação e área do mouse |
| settings.cpp | Páginas independentes, aparência, teste de voz e diagnósticos |
| voice_test.hpp / voice_test.cpp | Buffer PCM limitado e captura/reprodução temporárias |
| integration.hpp / integration.cpp | Troca explícita e restauração da chave raiz do KMix |
| tools.cpp | Medição em /proc e desinstalação por lista de arquivos |
| tests.cpp / tests_io.cpp | Regressões do modelo/remoção e I/O com ferramentas simuladas |
| main.cpp | Opções de execução e inicialização |


## Evolução 0.3.0

A cobertura de áudio da tabela acima foi mantida. Esta versão acrescenta
instalador e desinstalador gráficos, lançador Iniciar, cópia verificada e
recolhimento da extração/ZIP, registro de integrações e restos, autostart XDG,
atalhos opcionais de menu/área de trabalho, recuperação de instalações
interrompidas e reparação de caminhos de atalhos após mover a pasta.

O assistente reconhece sistema/sessão/backend e oferece configuração real de
teclas no X11: eventos confirmados, combinações alternativas, prova de reserva,
conflitos e controle direto integrado à bandeja. Compatibilidade é o padrão.
A página Teclado e Fn permite reconfigurar sem reinstalar. Preferências do
destino de uma atualização são preservadas; uma alteração de teclado só
substitui a anterior quando o usuário realmente a solicita no assistente.

Wayland permanece parcial: o portal GlobalShortcuts foi investigado, mas seu
registro não foi implementado. Não há backend PulseAudio independente nem
transferência automática de atalhos do desktop. Isso aparece na interface e
na documentação; não é anunciado suporte universal.

Novos módulos: `environment.*` (capacidades), `installation.*` (transações,
registros, hashes e integração de arquivos), `keyboard.*` (atalhos X11),
`setup.*` (assistentes), `tests_installation.cpp` e `ferramentas/launcher.c`.

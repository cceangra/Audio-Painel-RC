# Histórico

## 0.3.0

- Instalador e desinstalador gráficos em C++, com fundo azul-escuro.
- Lançador ELF `Iniciar`, assistente na primeira abertura e execução portátil.
- Pasta escolhida, cópia verificada com SHA-256, recolhimento da origem e ZIP.
- Registros locais de instalação, integrações externas, restos e recuperação.
- Atalhos opcionais para login, menu e área de trabalho; caminhos pré-visualizados.
- Detecção de distribuição, arquitetura, sessão, desktop e backend de áudio.
- Teclas de volume em compatibilidade e controle direto opcional no X11;
  confirmação de eventos, combinações personalizadas e teste real de conflitos.
- Página Teclado e Fn; exportação local do diagnóstico; passo ajustável.
- Fallback para janela em desktops sem bandeja compatível.
- Preservação dos layouts, mixer, filtro, teste de voz e central independente.
- Autostart XDG fora do Trinity; integração KMix restrita ao Trinity.
- Documentação pública, licença e testes de instalação/remoção/recuperação.

Limites: registro de atalhos via portal Wayland, backend PulseAudio independente
e transferência automática de atalhos dos desktops não foram implementados.

## 0.2.0

Reimplementação completa do painel em C++/GTK3, com layouts 1/3/4, mixer por
aplicativo, rotas reais, integração com filtro, teste de voz com buffer de 1 s,
central independente, personalização e medidor que inclui auxiliares.

## 0.1 experimental

Protótipo C++ original fornecido pelo usuário, anterior ao mixer completo e
ao monitor de áudio por eventos.

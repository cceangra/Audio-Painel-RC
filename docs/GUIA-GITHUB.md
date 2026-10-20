# Guia de GitHub — Áudio Painel RC

## 1. Criar o repositório

1. Acesse `https://github.com/new` com sua conta do GitHub.
2. Nome: `Audio-Painel-RC` (sem acentos, para facilitar comandos).
3. Selecione **Privado** para revisar o código antes da publicação, ou **Público**
   se quiser disponibilizá-lo desde o início.
4. **Não** marque README, `.gitignore` ou licença adicionais: estes arquivos já existem.
5. Clique **Create repository** e copie a URL HTTPS exibida.

## 2. Publicar os fontes organizados (Q4OS)

Instale `git` apenas se ainda não estiver disponível. Extraia o ZIP de **fontes**
recebido desta organização em uma pasta de trabalho. Abra o terminal dentro dela.
Os comandos a seguir não usam `sudo`.

```bash
cd /CAMINHO/DA/PASTA/Audio-Painel-RC

git init -b main
git config user.name "Seu Nome ou Apelido"
git config user.email "seu-email-de-commits@example.com"
git add .
git commit -m "Organizar código-fonte do Áudio Painel RC 0.3.0"
git remote add origin https://github.com/SEU_USUARIO/Audio-Painel-RC.git
git push -u origin main
```

Substitua os exemplos pelo caminho, usuário e e-mail desejados. Se o Git pedir
autenticação, use o método de autenticação recomendado pelo GitHub (gerenciador
de credenciais ou SSH). **Nunca coloque senhas ou tokens dentro dos arquivos do projeto.**

## 3. Atualizar o projeto depois

Antes de editar:

```bash
git pull --ff-only
```

Crie um branch para cada mudança:

```bash
git switch -c melhoria/tema-escuro
# Edite os arquivos, compile e teste.
bash compilar.sh
bash testar.sh
bash testar-instalacao.sh
bash testar-integracao.sh
git status --short
git add src/ docs/
git commit -m "Melhorar aparência do tema escuro"
git push -u origin melhoria/tema-escuro
```

Abra um **Pull Request** no GitHub para comparar o branch com `main` antes de
incorporar a mudança. Não use `git add -f` em executáveis, arquivos de instalação
ou dados locais ignorados.

## 4. Publicar um ZIP para usuários finais

No seu computador Linux, na pasta do projeto:

```bash
bash compilar.sh
```

O processo compila e gera o manifesto do instalador. Confira os testes e monte
um ZIP da pasta **sem `.git` nem dados pessoais** para anexar em
**GitHub → Releases → Draft a new release**. O ZIP enviado nesta conversa já
inclui um pacote pronto, criado a partir desta versão organizada.

## 5. O que significam os termos

- **Repositório:** pasta do projeto com histórico na nuvem.
- **Commit:** registro de uma alteração.
- **Branch:** linha de desenvolvimento independente.
- **Pull Request:** comparação/revisão antes de integrar o código.
- **Release:** versão de distribuição para as pessoas baixarem.

## 6. Cuidados

- Código-fonte vai no repositório. Os arquivos `audio-painel-rc` e `Iniciar`
  são gerados na compilação, por isso ficam fora do histórico.
- Não publique preferências pessoais, logs, gravações nem backups de instalação.
- `src/interface/tema.cpp` é o ponto inicial recomendado para ajustes visuais.
- Não execute `git reset --hard`, `git push --force` ou exclusões em massa como
  maneira de organizar o repositório.

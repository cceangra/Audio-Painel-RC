#include "ui.hpp"
#include "integration.hpp"
#include "setup.hpp"
#include "installation.hpp"
#include <iostream>
#include <thread>
#include <signal.h>
#include <sys/prctl.h>
#include <unistd.h>
namespace rc {
static void help(){std::cout<<"Áudio Painel RC C++ "<<VERSION<<"\n\n"
    "  --janela                  Abrir o painel imediatamente\n"
    "  --configuracoes           Central em processo separado\n"
    "  --instalar                Abrir o instalador gráfico\n"
    "  --iniciar                 Assistente na primeira execução; painel depois\n"
    "  --teclado                 Reconfigurar teclas de volume / Fn\n"
    "  --desinstalador            Remoção com interface gráfica\n"
    "  --demo                    Interface com áudio simulado\n"
    "  --verificar               Diagnóstico somente leitura, sem janela\n"
    "  --testes                  Testes internos sem alterar o áudio\n"
    "  --ram                     Medir este app e auxiliares em /proc\n"
    "     --cenario TEXTO --amostras 5 --intervalo 1 --espera 5\n"
    "  --status                  Consultar instâncias desta pasta\n"
    "  --encerrar                Encerrar instâncias desta pasta\n"
    "  --autostart-ativar         Criar somente seu atalho no desktop detectado\n"
    "  --autostart-desativar      Remover somente seu atalho\n"
    "  --substituir-kmix          Troca explícita, depois de validar a bandeja\n"
    "  --restaurar-kmix           Desfazer somente a troca registrada por este app\n"
    "  --desinstalar [--sim]      Remover arquivos próprios; preservar ZIP\n"
    "  --versao                  Versão\n\n"
    "O início normal não altera o KMix, o filtro, a automação HDMI ou o autostart.\n";}
}
int main(int argc,char** argv){
    using namespace rc;
    // Mesma PID do comando externo: sem shell ou processo intermediário
    // permanente. Impede monitores/testes órfãos após uma falha do painel.
    if(argc>=4&&std::string(argv[1])=="--_filho"){
        if(prctl(PR_SET_PDEATHSIG,SIGTERM)<0||getppid()!=std::atoi(argv[2]))return 126;
        execvp(argv[3],argv+3);std::cerr<<"Ferramenta não encontrada: "<<argv[3]<<'\n';return 127;
    }
    signal(SIGPIPE,SIG_IGN);setenv("LC_NUMERIC","C",1);
    // O seletor GTK de cores mantém somente estado transitório. A preferência
    // durável do painel é o JSON local; outros programas não são afetados.
    setenv("GSETTINGS_BACKEND","memory",1);
    bool settings=false,show=false,demo=false,standalone=false,ram=false,yes=false,setup=false,keys=false,remove_gui=false,bypass_setup=false;
    std::string mode,scenario="bandeja";int samples=5,interval=1000,wait=5000;
    try{
        for(int i=1;i<argc;++i){std::string arg=argv[i];
            auto next=[&](){if(++i>=argc)throw std::runtime_error("Falta valor de "+arg);return std::string(argv[i]);};
            if(arg=="--configuracoes"||arg=="--settings")settings=true;
            else if(arg=="--instalar")setup=true;else if(arg=="--teclado")keys=true;else if(arg=="--desinstalador")remove_gui=true;
            else if(arg=="--iniciar"){}else if(arg=="--painel")bypass_setup=true;
            else if(arg=="--janela")show=true;else if(arg=="--demo")demo=true;else if(arg=="--sem-ipc")standalone=true;else if(arg=="--sim")yes=true;
            else if(arg=="--ram")ram=true;else if(arg=="--amostras")samples=std::stoi(next());
            else if(arg=="--intervalo"||arg=="--espera"){double n=std::stod(next());if(!std::isfinite(n))throw std::runtime_error("Tempo inválido");int value=int(std::clamp(n,0.0,60.0)*1000);if(arg=="--intervalo")interval=value;else wait=value;}
            else if(arg=="--cenario")scenario=next();
            else if(arg=="--ajuda"||arg=="--help"){help();return 0;}
            else if(arg=="--versao"){std::cout<<VERSION<<'\n';return 0;}
            else if(arg=="--testes"||arg=="--self-test")mode="tests";
            else if(arg=="--testes-io")mode="tests-io";
            else if(arg=="--testes-instalacao")mode="tests-install";
            else if(arg=="--testes-teclado-x11")mode="tests-keyboard";
            else if(arg=="--verificar")mode="diagnostics";
            else if(arg=="--encerrar"||arg=="--status"||arg=="--autostart-ativar"||arg=="--autostart-desativar"||arg=="--desinstalar"||arg=="--substituir-kmix"||arg=="--restaurar-kmix")mode=arg;
            else if(arg=="--analisar-dump"){
                auto g=read_graph(read_file(next()));auto s=g.state();Json::Array outputs,inputs,apps;
                for(const auto& c:s.outputs)outputs.emplace_back(Json::Object{{"label",c.label},{"name",c.node.name},{"route",c.route},{"available",c.available},{"selected",c.selected}});
                for(const auto& c:s.inputs)inputs.emplace_back(Json::Object{{"label",c.label},{"name",c.node.name},{"route",c.route},{"available",c.available},{"selected",c.selected}});
                for(const auto& a:s.apps())apps.emplace_back(Json::Object{{"name",a.label},{"count",int(a.nodes.size())},{"output",a.nodes[0].output}});
                std::cout<<Json(Json::Object{{"sink",s.sink?s.sink->ref.name:""},{"source",s.source?s.source->ref.name:""},{"volume",s.sink?s.sink->volume:0},{"filtered",s.filtered},{"physical_muted",bool(s.physical&&s.physical->muted)},{"outputs",outputs},{"inputs",inputs},{"apps",apps}}).dump()<<'\n';return 0;
            }else throw std::runtime_error("Opção desconhecida: "+arg);
        }
        if(demo&&(mode=="--desinstalar"||mode=="--autostart-ativar"||mode=="--autostart-desativar"||mode=="--substituir-kmix"||mode=="--restaurar-kmix"))throw std::runtime_error("A demonstração não executa remoção ou alterações persistentes de integração. Retire --demo somente se desejar aplicar a ação.");
        if(ram)return memory_meter(samples,interval,wait,scenario);
        if(mode=="tests")return tests();if(mode=="tests-io")return tests_io();if(mode=="tests-install")return installation_tests();if(mode=="tests-keyboard")return keyboard_x11_tests();if(mode=="diagnostics")return diagnostics();
        if(mode=="--desinstalar")return uninstall(yes);
        if(mode=="--autostart-ativar"||mode=="--autostart-desativar"){autostart(mode=="--autostart-ativar");std::cout<<"Atalho próprio atualizado. KMix preservado.\n";return 0;}
        if(mode=="--substituir-kmix"){
            if(geteuid()==0)throw std::runtime_error("Execute sem sudo");bool started=false,ready=false;
            for(int n=0;n<80;++n){try{auto state=ipc("tray","status");if(state["embedded"].boolean()&&state["sink"].boolean()&&state["error"].text().empty()){ready=true;break;}}
                catch(...){if(!started){spawn_tray();started=true;}}std::this_thread::sleep_for(std::chrono::milliseconds(100));}
            if(!ready)throw std::runtime_error("O novo ícone ou o áudio não ficaram disponíveis. KMix preservado.");
            integrate_kmix();std::cout<<"Troca explícita concluída. KMix continua instalado e pode ser restaurado.\n";return 0;
        }
        if(mode=="--restaurar-kmix"){
            if(geteuid()==0)throw std::runtime_error("Execute sem sudo");try{ipc("tray","quit");}catch(...){}restore_kmix(true);std::cout<<"KMix restaurado. O atalho da substituição foi removido.\n";return 0;
        }
        if(mode=="--encerrar"||mode=="--status"){
            bool found=false;for(const char* role:{"tray","settings","demo-tray","demo-settings"})try{Json r=ipc(role,mode=="--encerrar"?"quit":"status");std::cout<<role<<": "<<r.dump()<<'\n';found=true;}catch(const std::exception&){}
            if(!found)std::cout<<"Nenhuma instância acessível desta pasta.\n";return 0;
        }
        if(setup||keys||remove_gui||(!settings&&!demo&&!bypass_setup&&!installed_here())){
            if(geteuid()==0&&!demo)throw std::runtime_error("Abra o instalador como usuário normal, sem sudo.");
            GtkApi ui;if(!ui.gtk_init_check(&argc,&argv))throw std::runtime_error("Não foi possível abrir a sessão gráfica. Abra Iniciar dentro do desktop ou execute iniciar.sh no terminal da sessão.");
            ui.g_object_set(ui.gtk_settings_get_default(),"gtk-recent-files-enabled",0,nullptr);
            if(remove_gui)return uninstall_window(ui,demo);return setup_window(ui,keys,demo);
        }
        if(standalone&&!demo)throw std::runtime_error("--sem-ipc é exclusivo de --demo, para testes gráficos isolados.");
        if(geteuid()==0&&!demo)throw std::runtime_error("Abra o painel como usuário normal, sem sudo.");
        int server=-1;
        if(!standalone){std::string role=(demo?"demo-":"")+std::string(settings?"settings":"tray");server=claim(role);if(server<0){ipc(role,settings||show?"show":"toggle");return 0;}}
        GtkApi ui;
        if(!ui.gtk_init_check(&argc,&argv)){if(server>=0)close(server);throw std::runtime_error("Não foi possível abrir a sessão gráfica. Execute no terminal da sua sessão de desktop.");}
        ui.g_object_set(ui.gtk_settings_get_default(),"gtk-recent-files-enabled",0,nullptr);
        App app(ui,settings,demo,standalone,server);return app.run(show);
    }catch(const std::exception& e){std::cerr<<"Áudio RC: "<<e.what()<<'\n';return 1;}
}

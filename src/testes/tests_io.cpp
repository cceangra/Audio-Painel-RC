#include "process.hpp"
#include "voice_test.hpp"
#include <iostream>
#include <sstream>
namespace rc {
int tests_io(){
    const char* dir=getenv("RC_AUDIO_TEST_DIR");if(!dir)throw std::runtime_error("Use testar-integracao.sh; ferramentas simuladas obrigatórias.");
    fs::path root=fs::canonical(dir);if(root.parent_path()!=folder()/"testes"||root.filename().string().find("io-")!=0)throw std::runtime_error("Pasta de simulação inválida");
    const char* path=getenv("PATH");std::string first=path?std::string(path).substr(0,std::string(path).find(':')):"";if(first!=root.string())throw std::runtime_error("PATH do teste não está isolado");
    for(const char* tool:{"pw-dump","wpctl","pw-metadata","pw-cat"})if(!fs::is_symlink(root/tool)||fs::canonical(root/tool)!=root/"falso-pipewire")throw std::runtime_error("Ferramenta simulada não validada");
    std::string fixture=read_file(folder()/"testes/sessao.json");atomic_write(root/"snapshot.json",fixture);atomic_write(root/"evento.json","[]");
    LoopApi loop;void* main=loop.g_main_loop_new(nullptr,0);std::unique_ptr<Backend> backend;std::unique_ptr<VoiceTest> voice;
    unsigned timeout=0,voice_check=0;int stage=0,passed=0;bool failed=false,ending=false;
    State initial=read_graph(fixture).state();Ref appref=initial.apps()[0].nodes[0].ref;
    auto fail=[&](const std::string& text){if(ending)return;failed=true;ending=true;std::cerr<<"ERRO integração: "<<text<<'\n';loop.g_main_loop_quit(main);};
    auto ok=[&](const std::string& text){++passed;std::cout<<"OK  "<<text<<'\n';};
    backend=std::make_unique<Backend>(loop,false,[&](){
        if(ending)return;
        try{
            if(stage==0&&backend->state.sink){
                if(!backend->state.filtered||!backend->state.physical||!backend->state.physical->muted)throw std::runtime_error("estado inicial divergente");
                ok("Monitor assíncrono conectado e estado derivado correto");stage=1;Action a;a.kind=Action::Volume;a.refs={appref};a.value=65;backend->submit(a);
            }else if(stage==6&&!backend->state.sink){ok("Hotplug recebido pelo monitor sem consultas periódicas");stage=7;ending=true;loop.g_main_loop_quit(main);}
        }catch(const std::exception& e){fail(e.what());}
    },[&](const std::string& error){
        if(ending)return;
        try{
            std::string commands=read_file(root/"comandos.txt");
            if(stage==1){if(!error.empty()||commands.find("wpctl set-volume 111 65.00% --limit 1.0")==std::string::npos)throw std::runtime_error("comando de volume não completou: "+error);ok("Comando de volume assíncrono, validado e limitado");stage=2;Action a;a.kind=Action::Mute;a.refs={appref};a.muted=true;backend->submit(a);}
            else if(stage==2){if(!error.empty()||commands.find("wpctl set-mute 111 1")==std::string::npos)throw std::runtime_error("mudo não completou: "+error);ok("Fila de comandos e mudo individual");stage=3;Action a;a.kind=Action::Choose;a.refs={appref};a.choice.node=initial.sink->ref;backend->submit(a);}
            else if(stage==3){if(!error.empty()||commands.find("pw-metadata -n default 111 target.object "+initial.sink->ref.name+" Spa:String")==std::string::npos)throw std::runtime_error("destino não completou: "+error);ok("Destino de aplicativo por metadados, sem shell");
                Json modified=Json::parse(fixture);for(auto& e:std::get<Json::Array>(modified.v))if(e["id"].integer()==111)e["info"]["props"]["object.serial"]=99999;
                atomic_write(root/"snapshot.json",modified.dump());stage=4;Action a;a.refs={appref};a.value=22;backend->submit(a);
            }else if(stage==4){if(error.find("mudou")==std::string::npos||commands.find("wpctl set-volume 111 22.00%")!=std::string::npos)throw std::runtime_error("ID reciclado recebeu comando");ok("Snapshot recente impede comando para ID reciclado");
                stage=5;voice=std::make_unique<VoiceTest>(loop,[&](const std::string& e){fail(e);});voice->start(initial.source->ref,initial.sink->ref);
                voice_check=loop.later([&](){voice_check=0;try{
                    if(!voice->running||voice->peak()<=0)throw std::runtime_error("PCM não circulou");voice->stop();
                    std::istringstream numbers(read_file(root/"pcm-contagem.txt"));long total=0,first=-1;numbers>>total>>first;
                    if(total<=96000||first<96000||first>96010)throw std::runtime_error("atraso PCM fora de um segundo: "+std::to_string(first));
                    ok("Captura/reprodução simuladas com atraso de 96000 bytes e parada limpa");stage=6;atomic_write(root/"evento.json","[{\"id\":62,\"info\":null}]");
                }catch(const std::exception& e){fail(e.what());}},1600);
            }
        }catch(const std::exception& e){fail(e.what());}
    });
    timeout=loop.later([&](){timeout=0;fail("tempo limite da suíte de I/O");},8000);
    backend->start();loop.g_main_loop_run(main);loop.cancel(timeout);loop.cancel(voice_check);voice.reset();backend.reset();loop.g_main_loop_unref(main);
    if(stage!=7)failed=true;std::cout<<"\n"<<passed<<" verificações de I/O passaram; "<<(failed?1:0)<<" falha(s). Só ferramentas simuladas foram chamadas.\n";return failed?1:0;
}
}

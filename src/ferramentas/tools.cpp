#include "platform.hpp"
#include "integration.hpp"
#include "installation.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <signal.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
namespace rc {
struct Proc {int pid=0,parent=0;unsigned long long ticks=0,start=0;std::vector<std::string> args;std::string exe;};
static std::map<int,Proc> processes(){
    std::map<int,Proc> out;std::error_code error;
    for(const auto& p:fs::directory_iterator("/proc",error)){
        auto name=p.path().filename().string();if(name.empty()||!std::all_of(name.begin(),name.end(),[](unsigned char c){return std::isdigit(c);}))continue;
        try{
            struct stat st{};if(stat(p.path().c_str(),&st)||st.st_uid!=getuid())continue;
            std::string text=read_file(p.path()/"stat",16384);size_t end=text.rfind(')');if(end==std::string::npos)continue;
            std::istringstream fields(text.substr(end+2));std::vector<std::string> f;std::string item;while(fields>>item)f.push_back(item);if(f.size()<20)continue;
            Proc v;v.pid=std::stoi(name);v.parent=std::stoi(f[1]);v.ticks=std::stoull(f[11])+std::stoull(f[12]);v.start=std::stoull(f[19]);
            text=read_file(p.path()/"cmdline",65536);size_t b=0;for(size_t k=0;k<=text.size();++k)if(k==text.size()||text[k]=='\0'){if(k>b)v.args.push_back(text.substr(b,k-b));b=k+1;}
            v.exe=fs::read_symlink(p.path()/"exe").string();out[v.pid]=std::move(v);
        }catch(const std::exception&){}
    }return out;
}
static bool argument(const Proc& p,const std::string& arg){return std::find(p.args.begin(),p.args.end(),arg)!=p.args.end();}
static bool own_process(const Proc& p){
    std::string exe=p.exe;const std::string deleted=" (deleted)";if(exe.size()>deleted.size()&&exe.compare(exe.size()-deleted.size(),deleted.size(),deleted)==0)exe.resize(exe.size()-deleted.size());
    return p.pid!=proc_self_pid()&&exe==(folder()/"audio-painel-rc").string();
}
static bool own_gui(const Proc& p){
    if(!own_process(p))return false;
    for(const auto& a:{"--ram","--verificar","--testes","--testes-io","--self-test","--desinstalar","--encerrar","--status","--versao","--analisar-dump"})if(argument(p,a))return false;
    return true;
}
static std::map<int,std::string> targets(const std::map<int,Proc>& all){
    std::map<int,std::string> out;for(const auto& p:all)if(own_gui(p.second))out[p.first]=argument(p.second,"--configuracoes")||argument(p.second,"--settings")?"configurações":"bandeja";
    bool changed=true;while(changed){changed=false;for(const auto& p:all)if(!out.count(p.first)&&out.count(p.second.parent)){
        out[p.first]=p.second.args.empty()?"auxiliar":fs::path(p.second.args[0]).filename().string();changed=true;
    }}return out;
}
static std::optional<std::vector<double>> memory(int pid){
    try{std::istringstream s(read_file(fs::path("/proc")/std::to_string(pid)/"smaps_rollup",131072));std::string line;double rss=-1,pss=-1,priv=0;
        while(std::getline(s,line)){std::istringstream r(line);std::string key;double v;if(!(r>>key>>v))continue;
            if(key=="Rss:")rss=v;else if(key=="Pss:")pss=v;else if(key=="Private_Clean:"||key=="Private_Dirty:"||key=="Private_Hugetlb:")priv+=v;}
        if(rss<0||pss<0)return std::nullopt;return std::vector<double>{rss/1024,pss/1024,priv/1024};
    }catch(...){return std::nullopt;}
}
static double median(std::vector<double> v){if(v.empty())return 0;std::sort(v.begin(),v.end());size_t n=v.size();return n%2?v[n/2]:(v[n/2-1]+v[n/2])/2;}
int memory_meter(int count,int interval_ms,int wait_ms,const std::string& label){
    count=std::clamp(count,1,30);interval_ms=std::clamp(interval_ms,200,10000);wait_ms=std::clamp(wait_ms,0,60000);
    std::cout<<"ÁUDIO RC C++ — RAM SOMENTE LEITURA\nCenário: "<<label<<"\nColeta em "<<wait_ms/1000.0<<" s. Abra o painel ou a página desejada.\n"<<std::flush;
    std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
    auto previous=processes();auto prevtime=std::chrono::steady_clock::now();double hz=sysconf(_SC_CLK_TCK);
    std::map<int,std::vector<std::vector<double>>> samples;std::map<int,std::vector<double>> cpu;std::map<int,std::string> labels;std::vector<std::vector<double>> totals;
    for(int k=0;k<count;++k){
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));auto now=std::chrono::steady_clock::now();auto all=processes();auto ids=targets(all);std::vector<double> total(3,0);bool missing=false;
        for(const auto& item:ids){labels[item.first]=item.second;auto values=memory(item.first);if(!values){missing=true;continue;}
            // Confirmar identidade após ler /proc para não misturar PID reciclado.
            auto old=previous.find(item.first);if(old!=previous.end()&&old->second.start!=all.at(item.first).start)continue;
            samples[item.first].push_back(*values);for(size_t n=0;n<3;++n)total[n]+=(*values)[n];
            if(old!=previous.end()){double elapsed=std::chrono::duration<double>(now-prevtime).count();auto ticks=all.at(item.first).ticks;cpu[item.first].push_back(ticks>=old->second.ticks?(ticks-old->second.ticks)/hz/elapsed*100:0);}
        }
        if(!ids.empty()&&!missing)totals.push_back(total);previous=std::move(all);prevtime=now;
    }
    if(labels.empty()){std::cout<<"Nenhum painel desta pasta está aberto. O medidor não inicia o aplicativo.\n";return 0;}
    std::cout<<"\nPID     Componente            RSS MiB   PSS MiB   Privada MiB   CPU %\n"<<std::fixed<<std::setprecision(2);
    for(const auto& p:labels){std::cout<<std::left<<std::setw(8)<<p.first<<std::setw(21)<<p.second<<std::right;
        if(samples[p.first].empty()){std::cout<<"leitura indisponível\n";continue;}
        for(size_t n=0;n<3;++n){std::vector<double> values;for(const auto& v:samples[p.first])values.push_back(v[n]);std::cout<<std::setw(n==2?14:10)<<median(values);}
        std::cout<<std::setw(8)<<median(cpu[p.first])<<'\n';
    }
    if(!totals.empty()){std::cout<<std::left<<std::setw(29)<<"TOTAL MEDIANO"<<std::right;for(size_t n=0;n<3;++n){std::vector<double> values;for(const auto& v:totals)values.push_back(v[n]);std::cout<<std::setw(n==2?14:10)<<median(values);}std::cout<<'\n';}
    else std::cout<<"Total indisponível: pelo menos uma leitura foi negada.\n";
    std::cout<<"\nCompare a soma de PSS nas mesmas condições. RSS repete páginas compartilhadas.\nInclui bandeja, central e seus auxiliares (pw-dump / teste de voz).\nExclui o PipeWire, KMix e automação HDMI já existentes. Nenhum arquivo foi criado.\n";return 0;
}
int uninstall(bool yes,std::ostream* report){
    auto& out=report?*report:std::cout;
    fs::path root=folder();
    if(root==root.root_path()||root==fs::path(getenv("HOME")?getenv("HOME"):"/")||root.string().size()<8)throw std::runtime_error("Pasta de remoção inválida");
    if(fs::is_symlink(root/"IDENTIDADE-RC")||read_file(root/"IDENTIDADE-RC",128)!=std::string(APP_ID)+"\n")throw std::runtime_error("A pasta não tem a identidade desta versão. Remoção cancelada.");
    auto files=removal_manifest(root);if(files.empty())throw std::runtime_error("Lista de remoção vazia");
    out<<"Pasta a remover: "<<root<<"\nArquivos do aplicativo listados: "<<files.size()<<"\n"<<external_report(root)<<"\n";
    if(!yes){out<<"Digite REMOVER para continuar: "<<std::flush;std::string answer;std::getline(std::cin,answer);if(answer!="REMOVER"){out<<"Cancelado.\n";return 0;}}
    // Encerrar somente executáveis desta pasta e deste usuário, nunca pelo
    // nome do processo e nunca com pkill genérico.
    auto all=processes();std::vector<int> stop;
    for(const auto& p:all)if(own_process(p.second)){int target=signal_pid(p.first);if(target<1)throw std::runtime_error("Há uma instância desta pasta em outra sessão isolada. Encerre-a antes de remover.");stop.push_back(target);}
    for(int target:stop)kill(target,SIGTERM);
    for(int n=0;n<40;++n){bool alive=false;for(const auto& p:processes())if(own_process(p.second))alive=true;if(!alive)break;
        if(n==39)throw std::runtime_error("O app ainda está encerrando. Feche suas janelas e repita; os arquivos foram preservados.");std::this_thread::sleep_for(std::chrono::milliseconds(100));}
    // Só remover atalhos com a identidade e o caminho exatos desta pasta.
    recover_installation(root);
    files=removal_manifest(root);
    // Recusa atalhos editados antes de apagar os arquivos do aplicativo.
    remove_recorded_external(root,out);
    if(Config::load().original["integration"]["prepared"].boolean())restore_kmix(true);
    std::vector<fs::path> shortcuts{autostart_path()};
    try{auto record=Config::load().original["autostart_path"].text();if(!record.empty())shortcuts.emplace_back(record);}catch(...){}
    for(const auto& p:shortcuts){
        if(!fs::exists(p)||fs::is_symlink(p))continue;auto content=read_file(p,32768);
        if(content.find(std::string("\nX-RC-App=")+APP_ID+"\n")!=std::string::npos&&content.find("\nX-RC-Root="+root.string()+"\n")!=std::string::npos&&p.filename()=="Audio-Painel-RC-CPP.desktop"){
            fs::remove(p);out<<"Atalho legado removido: "<<p<<'\n';
        }
    }
    std::set<fs::path> dirs;
    Json cfg_cleanup=Config::load().original;
    Json installation_cleanup=fs::exists(root/"instalacao.json")?Json::parse(read_file(root/"instalacao.json")):Json();
    for(const auto& d:cfg_cleanup["autostart_created_dirs"].items()){fs::path p=d.text();if(!p.empty()&&p!=p.root_path())rmdir(p.c_str());}
    for(const auto& rel:files){fs::path p=root/rel;std::error_code error;if(fs::is_directory(p,error)&&!fs::is_symlink(p))continue;fs::remove(p,error);if(error)throw std::runtime_error("Não foi possível remover "+p.string()+": "+error.message());
        auto dir=rel.parent_path();while(!dir.empty()){dirs.insert(dir);dir=dir.parent_path();}}
    std::vector<fs::path> sorted(dirs.begin(),dirs.end());std::sort(sorted.begin(),sorted.end(),[](const auto& a,const auto& b){return a.native().size()>b.native().size();});
    for(const auto& p:sorted)rmdir((root/p).c_str());rmdir(root.c_str());
    for(const auto& d:installation_cleanup["created_parent_dirs"].items()){fs::path parent=d.text();auto relative=root.lexically_relative(parent);if(parent.is_absolute()&&safe_relative_path(relative)&&parent!=parent.root_path()&&parent!=fs::path(getenv("HOME")?getenv("HOME"):"/"))rmdir(parent.c_str());}
    out<<"Aplicativo removido. ZIP, arquivos pessoais, KMix, filtro e automação HDMI preservados.\n";
    if(fs::exists(root))out<<"A pasta permanece porque contém ZIPs ou arquivos preservados: "<<root<<'\n';return 0;
}
}

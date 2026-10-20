#include "integration.hpp"
#include "installation.hpp"
#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
namespace rc {
static std::string trim(const std::string& s){auto b=s.find_first_not_of(" \t\r\n");if(b==std::string::npos)return "";return s.substr(b,s.find_last_not_of(" \t\r\n")-b+1);}
std::optional<std::string> root_value(const std::string& text,const std::string& key){
    size_t pos=0;while(pos<text.size()){size_t end=text.find('\n',pos);if(end==std::string::npos)end=text.size();std::string line=trim(text.substr(pos,end-pos));
        if(!line.empty()&&line.front()=='[')break;size_t equals=line.find('=');if(equals!=std::string::npos&&trim(line.substr(0,equals))==key)return trim(line.substr(equals+1));pos=end+1;
    }return std::nullopt;
}
std::string change_root_value(const std::string& text,const std::string& key,const std::optional<std::string>& value){
    std::string out;size_t pos=0;bool grouped=false,found=false;
    while(pos<text.size()){
        size_t end=text.find('\n',pos);if(end==std::string::npos)end=text.size();else ++end;
        std::string original=text.substr(pos,end-pos),line=trim(original);if(!line.empty()&&line.front()=='[')grouped=true;
        size_t equals=line.find('=');bool matches=!grouped&&equals!=std::string::npos&&trim(line.substr(0,equals))==key;
        if(matches){if(!found&&value)out+=key+'='+*value+'\n';found=true;}else out+=original;pos=end;
    }
    if(!found&&value)out=key+'='+*value+'\n'+out;return out;
}
static fs::path kmixrc(){const char* tde=getenv("TDEHOME");const char* home=getenv("HOME");if(!home||!*home)throw std::runtime_error("HOME não definido");return fs::path(tde&&*tde?tde:std::string(home)+"/.trinity")/"share/config/kmixrc";}
static void write_kmix(const fs::path& path,const std::string& text,unsigned mode){
    auto record=Config::load().original["integration"];auto token="kmix-"+std::to_string(getpid())+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());fs::path temporary=path.parent_path()/(".Audio-Painel-RC-"+token+".tmp");
    auto paths=record["temporary_paths"].items();paths.emplace_back(temporary.string());record["temporary_paths"]=paths;preference("integration",record);replace_registered_external(path,temporary,text,mode);
}
static std::vector<int> kmix_pids(){
    std::vector<int> out;std::error_code error;for(const auto& d:fs::directory_iterator("/proc",error)){
        std::string name=d.path().filename().string();if(name.empty()||!std::all_of(name.begin(),name.end(),[](unsigned char c){return std::isdigit(c);}))continue;
        struct stat st{};if(stat(d.path().c_str(),&st)||st.st_uid!=getuid())continue;
        try{std::string raw=read_file(d.path()/"cmdline",65536);std::string first=raw.substr(0,raw.find('\0'));auto space=first.find(' ');std::string binary=first.substr(0,space);if(fs::path(binary).filename()=="kmix")out.push_back(std::stoi(name));}catch(...){}
    }return out;
}
static void start_kmix(){
    if(!kmix_pids().empty()||!fs::exists("/opt/trinity/bin/kmix"))return;
    pid_t child=fork();if(child==0){pid_t p=fork();if(p<0)_exit(127);if(p>0)_exit(0);setsid();int fd=open("/dev/null",O_RDWR);if(fd>=0){dup2(fd,0);dup2(fd,1);dup2(fd,2);if(fd>2)close(fd);}execl("/opt/trinity/bin/kmix","kmix",static_cast<char*>(nullptr));_exit(127);}
    if(child>0){int s;while(waitpid(child,&s,0)<0&&errno==EINTR){}}
}
void restore_kmix(bool start){
    Config cfg=Config::load();const Json& integration=cfg.original["integration"];
    if(integration["prepared"].boolean()){
        fs::path p=integration["kmixrc"].text();if(p.empty()||p.filename()!="kmixrc")throw std::runtime_error("Registro de restauração inválido");
        if(fs::is_symlink(p))throw std::runtime_error("kmixrc é um link simbólico; restauração interrompida");
        if(fs::exists(p)){
            std::string text=read_file(p,262144);
            // Respeitar mudanças posteriores feitas pelo usuário nesta chave.
            if(root_value(text,"Autostart")==std::optional<std::string>("false")){
                auto original=integration["original_autostart"].null()?std::optional<std::string>{}:std::optional<std::string>{integration["original_autostart"].text()};
                std::string restored=change_root_value(text,"Autostart",original);
                if(!integration["existed"].boolean()&&trim(restored).empty())fs::remove(p);
                else{struct stat st{};if(stat(p.c_str(),&st))throw std::runtime_error("Falha ao ler permissões do KMix");write_kmix(p,restored,unsigned(st.st_mode&0777));}
            }else if(root_value(text,"Autostart")!=(integration["original_autostart"].null()?std::optional<std::string>{}:std::optional<std::string>{integration["original_autostart"].text()}))throw std::runtime_error("O valor Autostart do KMix foi editado depois da troca. O registro e o backup foram preservados para revisão.");
        }
        autostart(integration["had_startup"].boolean());clear_registered_temporaries(p,Config::load().original["integration"]);
        for(const auto& d:integration["created_dirs"].items()){fs::path dir=d.text();if(!dir.empty()&&dir!=dir.root_path())rmdir(dir.c_str());}
        preference("integration",Json::Object{});
    }
    if(start)start_kmix();
}
void integrate_kmix(){
    if(geteuid()==0)throw std::runtime_error("Substituição do KMix deve ser feita sem sudo");
    if(!detect_environment(false).trinity)throw std::runtime_error("A integração com o KMix desta versão é específica do Trinity. As configurações deste desktop foram preservadas.");
    // Esta função só é chamada pelo comando explícito --substituir-kmix.
    // O comando verifica primeiro o novo ícone encaixado e o áudio conectado.
    fs::path path=kmixrc();if(fs::is_symlink(path))throw std::runtime_error("kmixrc é um link simbólico e foi preservado");
    fs::path shortcut=autostart_path();if(fs::is_symlink(shortcut)||(fs::exists(shortcut)&&!autostart_enabled()))throw std::runtime_error("Há um atalho de outra pasta; substituição cancelada");
    const bool had_startup=autostart_enabled();Config cfg=Config::load();
    if(cfg.original["integration"]["active"].boolean()&&had_startup&&fs::exists(path)&&root_value(read_file(path,262144),"Autostart")==std::optional<std::string>("false"))return;
    if(!cfg.original["integration"]["prepared"].boolean()){
        std::string text=fs::exists(path)?read_file(path,262144):"";auto original=root_value(text,"Autostart");
        Json::Array dirs;for(fs::path d=path.parent_path();!d.empty()&&!fs::exists(d);d=d.parent_path())dirs.emplace_back(d.string());
        fs::create_directories(folder()/"integracao-kmix");atomic_write(folder()/"integracao-kmix/kmixrc.antes",text);
        struct stat st{};unsigned mode=stat(path.c_str(),&st)==0?unsigned(st.st_mode&0777):0600;
        Json record=Json::Object{{"prepared",true},{"active",false},{"kmixrc",path.string()},{"existed",fs::exists(path)},{"original_autostart",original?Json(*original):Json()},{"created_dirs",dirs},{"original_sha256",sha256_text(text)},{"original_mode",int(mode)},{"method","Trinity: chave raiz Autostart"},{"had_startup",had_startup}};
        preference("integration",record);
    }
    try{
        std::vector<int> stop;for(int pid:kmix_pids()){int target=signal_pid(pid);if(target<1)throw std::runtime_error("O KMix pertence a outra sessão isolada; encerre-o nessa sessão antes da troca.");stop.push_back(target);}
        for(int target:stop)kill(target,SIGTERM);
        for(int n=0;n<40&&!kmix_pids().empty();++n)std::this_thread::sleep_for(std::chrono::milliseconds(75));
        if(!kmix_pids().empty())throw std::runtime_error("O KMix não encerrou; a troca foi interrompida");
        fs::create_directories(path.parent_path());std::string text=fs::exists(path)?read_file(path,262144):"";struct stat st{};unsigned mode=stat(path.c_str(),&st)==0?unsigned(st.st_mode&0777):0600;
        write_kmix(path,change_root_value(text,"Autostart",std::string("false")),mode);autostart(true);
        Json record=Config::load().original["integration"];record["active"]=true;preference("integration",record);
    }catch(...){try{restore_kmix(true);if(had_startup)autostart(true);}catch(const std::exception& e){std::cerr<<"Restauração: "<<e.what()<<'\n';}throw;}
}
void spawn_tray(){
    pid_t child=fork();if(child<0)throw std::runtime_error("Não foi possível iniciar a bandeja");
    if(child==0){pid_t p=fork();if(p<0)_exit(127);if(p>0)_exit(0);setsid();int fd=open("/dev/null",O_RDWR);if(fd>=0){dup2(fd,0);dup2(fd,1);dup2(fd,2);if(fd>2)close(fd);}auto exe=(folder()/"audio-painel-rc").string();execl(exe.c_str(),exe.c_str(),static_cast<char*>(nullptr));_exit(127);}
    int status;while(waitpid(child,&status,0)<0&&errno==EINTR){}
}
}

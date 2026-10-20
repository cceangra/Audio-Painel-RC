#include "platform.hpp"
#include "installation.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <poll.h>
#include <spawn.h>
#include <sstream>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <signal.h>
extern char** environ;
namespace rc {
fs::path folder(){static fs::path p=[](){char b[8192];ssize_t n=readlink("/proc/self/exe",b,sizeof b-1);if(n<0)throw std::runtime_error("Não foi possível localizar o executável");b[n]=0;return fs::path(b).parent_path();}();return p;}
int proc_self_pid(){static int id=[](){try{return std::stoi(fs::read_symlink("/proc/self").string());}catch(...){return int(getpid());}}();return id;}
int signal_pid(int proc_id){
    // /proc pode ter sido montado por um namespace de PID externo. Nunca usar
    // esses números diretamente em kill(), nem confundir o próprio processo.
    try{
        fs::path p=fs::path("/proc")/std::to_string(proc_id);
        if(fs::read_symlink(p/"ns/pid")!=fs::read_symlink("/proc/self/ns/pid"))return -1;
        std::istringstream status(read_file(p/"status",65536));std::string line;
        while(std::getline(status,line))if(line.rfind("NSpid:",0)==0){std::istringstream ids(line.substr(6));int id=-1,value;while(ids>>value)id=value;return id;}
        return proc_self_pid()==getpid()?proc_id:-1;
    }catch(...){return -1;}
}
std::string read_file(const fs::path& p,size_t limit){
    std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Não foi possível ler "+p.string());
    std::string out;char b[16384];
    while(f){f.read(b,sizeof b);size_t n=size_t(f.gcount());if(out.size()+n>limit)throw std::runtime_error("Arquivo excede limite: "+p.string());out.append(b,n);}return out;
}
void atomic_write(const fs::path& p,const std::string& text,unsigned mode){
    if(fs::is_symlink(p))throw std::runtime_error("Link simbólico recusado: "+p.string());
    std::string tmp=p.string()+".tmp-XXXXXX";std::vector<char> name(tmp.begin(),tmp.end());name.push_back(0);
    int fd=mkstemp(name.data());if(fd<0)throw std::runtime_error("Não foi possível gravar em "+p.parent_path().string());
    try {
        size_t off=0;while(off<text.size()){ssize_t n=write(fd,text.data()+off,text.size()-off);if(n<0&&errno==EINTR)continue;if(n<=0)throw std::runtime_error("Falha ao gravar preferências");off+=size_t(n);}
        if(fchmod(fd,mode)||fsync(fd))throw std::runtime_error("Falha ao confirmar gravação");
        close(fd);fd=-1;if(rename(name.data(),p.c_str()))throw std::runtime_error("Falha ao salvar "+p.string());
        int d=open(p.parent_path().c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(d>=0){fsync(d);close(d);}
    }catch(...){if(fd>=0)close(fd);unlink(name.data());throw;}
}
Config Config::from_json(const Json& j){
    if(!j.object())throw std::runtime_error("preferencias.json deve conter um objeto JSON");
    Config c;c.original=j;
    auto i=[&](const char* k,int fallback,int min,int max){return std::clamp(j[k].integer(fallback),min,max);};
    c.layout=i("layout",4,1,4);if(c.layout==2)c.layout=4;
    c.width=i("width",560,440,760);c.font=i("font",12,11,17);c.radius=i("radius",14,0,24);c.rows=i("rows",4,2,8);
    c.hide_delay=i("hide_delay",350,100,2000);c.margin=i("margin",12,0,40);c.gap=i("gap",3,0,24);c.release_seconds=i("release_seconds",20,0,300);
    c.opacity=std::clamp(j["opacity"].number(0.94),0.70,1.0);
    c.theme=j["theme"].text("dark");if(c.theme!="dark"&&c.theme!="light"&&c.theme!="system")c.theme="dark";
    c.density=j["density"].text("normal");if(c.density!="compact"&&c.density!="normal")c.density="normal";
    c.accent=j["accent"].text("#91a8ff");
    if(c.accent.size()!=7||c.accent[0]!='#'||!std::all_of(c.accent.begin()+1,c.accent.end(),[](unsigned char ch){return std::isxdigit(ch);}))c.accent="#91a8ff";
    c.hide_leave=j["hide_leave"].boolean(true);c.animation=j["animation"].boolean(true);return c;
}
Config Config::load(){fs::path p=folder()/"preferencias.json";return fs::exists(p)?from_json(Json::parse(read_file(p,65536))):Config{};}
Json Config::json()const{
    Json j=original;j["version"]=2;j["layout"]=layout;j["width"]=width;j["font"]=font;j["radius"]=radius;j["rows"]=rows;
    j["hide_delay"]=hide_delay;j["margin"]=margin;j["gap"]=gap;j["release_seconds"]=release_seconds;j["opacity"]=opacity;
    j["theme"]=theme;j["accent"]=accent;j["density"]=density;j["hide_leave"]=hide_leave;j["animation"]=animation;return j;
}
void preference(const std::string& key,const Json& value){
    int lock=open((folder()/"IDENTIDADE-RC").c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(lock<0||flock(lock,LOCK_EX)){if(lock>=0)close(lock);throw std::runtime_error("Falha ao bloquear preferências na pasta do app");}
    try{Json j=Config::load().json();j[key]=value;atomic_write(folder()/"preferencias.json",Config::from_json(j).json().dump()+"\n");}
    catch(...){close(lock);throw;}close(lock);
}
std::vector<std::string> child_arguments(const std::vector<std::string>& args,bool die_with_parent){
    if(!die_with_parent)return args;
    std::vector<std::string> wrapped={"/proc/self/exe","--_filho",std::to_string(getpid())};wrapped.insert(wrapped.end(),args.begin(),args.end());return wrapped;
}
pid_t spawn_pipe(const std::vector<std::string>& args,int& fd,bool merge_stderr,bool die_with_parent){
    if(args.empty())throw std::runtime_error("Comando vazio");
    int pipes[2];if(pipe2(pipes,O_CLOEXEC))throw std::runtime_error("Falha ao criar pipe");
    posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions,STDIN_FILENO,"/dev/null",O_RDONLY,0);
    posix_spawn_file_actions_adddup2(&actions,pipes[1],STDOUT_FILENO);
    if(merge_stderr)posix_spawn_file_actions_adddup2(&actions,pipes[1],STDERR_FILENO);
    else posix_spawn_file_actions_addopen(&actions,STDERR_FILENO,"/dev/null",O_WRONLY,0);
    posix_spawn_file_actions_addclose(&actions,pipes[0]);posix_spawn_file_actions_addclose(&actions,pipes[1]);
    auto execargs=child_arguments(args,die_with_parent);std::vector<char*> argv;for(const auto& a:execargs)argv.push_back(const_cast<char*>(a.c_str()));argv.push_back(nullptr);
    pid_t pid=-1;int err=posix_spawnp(&pid,execargs[0].c_str(),&actions,nullptr,argv.data(),environ);
    posix_spawn_file_actions_destroy(&actions);close(pipes[1]);
    if(err){close(pipes[0]);throw std::runtime_error("Não foi possível executar "+args[0]+": "+strerror(err));}
    fcntl(pipes[0],F_SETFL,fcntl(pipes[0],F_GETFL)|O_NONBLOCK);fd=pipes[0];return pid;
}
Result run_read(const std::vector<std::string>& args,int timeout_ms,size_t limit){
    Result r;int fd=-1;pid_t pid=-1;
    try{pid=spawn_pipe(args,fd);}
    catch(const std::exception& e){r.text=e.what();return r;}
    auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout_ms);bool eof=false;int status=0;
    while(true){
        char b[16384];ssize_t n;
        while((n=read(fd,b,sizeof b))>0){if(r.text.size()+size_t(n)>limit){kill(pid,SIGKILL);r.text="Saída excedeu limite de leitura";r.timeout=true;break;}r.text.append(b,size_t(n));}
        if(n==0)eof=true;
        if(waitpid(pid,&status,WNOHANG)==pid){r.status=WIFEXITED(status)?WEXITSTATUS(status):128+(WIFSIGNALED(status)?WTERMSIG(status):0);break;}
        if(r.timeout||std::chrono::steady_clock::now()>=until){r.timeout=true;kill(pid,SIGKILL);while(waitpid(pid,&status,0)<0&&errno==EINTR){};r.status=124;break;}
        pollfd p{fd,short(POLLIN|POLLHUP),0};if(!eof)poll(&p,1,20);else std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    char b[4096];ssize_t n;while((n=read(fd,b,sizeof b))>0&&r.text.size()+size_t(n)<=limit)r.text.append(b,size_t(n));close(fd);return r;
}
void spawn_settings(bool demo){
    pid_t child=fork();if(child<0)throw std::runtime_error("Falha ao abrir a central");
    if(child==0){pid_t second=fork();if(second<0)_exit(127);if(second>0)_exit(0);setsid();
        int fd=open("/dev/null",O_RDWR);if(fd>=0){dup2(fd,0);dup2(fd,1);dup2(fd,2);if(fd>2)close(fd);}
        std::string exe=(folder()/"audio-painel-rc").string();
        if(demo)execl(exe.c_str(),exe.c_str(),"--configuracoes","--demo",static_cast<char*>(nullptr));
        else execl(exe.c_str(),exe.c_str(),"--configuracoes",static_cast<char*>(nullptr));_exit(127);
    }int status;while(waitpid(child,&status,0)<0&&errno==EINTR){}
}
std::string endpoint(const std::string& role){
    uint64_t hash=14695981039346656037ULL;for(unsigned char c:folder().string()){hash^=c;hash*=1099511628211ULL;}
    std::ostringstream s;s<<"rc-audio-cpp-"<<getuid()<<'-'<<std::hex<<hash<<'-'<<role;return s.str();
}
static sockaddr_un address(const std::string& role,socklen_t& len){
    sockaddr_un a{};a.sun_family=AF_UNIX;auto e=endpoint(role);if(e.size()+1>=sizeof a.sun_path)throw std::runtime_error("Endereço IPC excedido");
    memcpy(a.sun_path+1,e.data(),e.size());len=socklen_t(offsetof(sockaddr_un,sun_path)+1+e.size());return a;
}
int claim(const std::string& role){
    int fd=socket(AF_UNIX,SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0);if(fd<0)throw std::runtime_error("IPC local indisponível: "+std::string(strerror(errno)));
    socklen_t len;sockaddr_un a=address(role,len);
    if(bind(fd,reinterpret_cast<sockaddr*>(&a),len)<0){int e=errno;close(fd);if(e==EADDRINUSE)return -1;throw std::runtime_error("Falha de IPC: "+std::string(strerror(e)));}
    if(listen(fd,8)){close(fd);throw std::runtime_error("Falha ao escutar IPC");}return fd;
}
Json ipc(const std::string& role,const std::string& action,int timeout_ms){
    int fd=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);if(fd<0)throw std::runtime_error("IPC indisponível");
    timeval tv{timeout_ms/1000,(timeout_ms%1000)*1000};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof tv);setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof tv);
    try{
        socklen_t len;sockaddr_un a=address(role,len);if(connect(fd,reinterpret_cast<sockaddr*>(&a),len)<0)throw std::runtime_error("Instância não encontrada");
        ucred peer{};socklen_t pl=sizeof peer;if(getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&peer,&pl)||peer.uid!=getuid())throw std::runtime_error("Outro usuário recusado no IPC");
        std::string req=Json(Json::Object{{"action",action}}).dump()+"\n";size_t pos=0;
        while(pos<req.size()){ssize_t n=send(fd,req.data()+pos,req.size()-pos,MSG_NOSIGNAL);if(n<=0)throw std::runtime_error("Falha ao enviar IPC");pos+=size_t(n);}
        std::string data;char b[1024];while(data.find('\n')==std::string::npos&&data.size()<65536){ssize_t n=recv(fd,b,sizeof b,0);if(n<=0)break;data.append(b,size_t(n));}
        close(fd);fd=-1;return Json::parse(data);
    }catch(...){if(fd>=0)close(fd);throw;}
}
static std::string env(const char* k,const std::string& fallback=""){const char* v=getenv(k);return v&&*v?v:fallback;}
std::string autostart_path(){return startup_location().string();}
static bool owned_shortcut(const fs::path& p){
    if(!fs::exists(p)||fs::is_symlink(p))return false;
    try {auto s=read_file(p,32768);return s.find(std::string("\nX-RC-App=")+APP_ID+"\n")!=std::string::npos&&s.find("\nX-RC-Root="+folder().string()+"\n")!=std::string::npos;}catch(...){return false;}
}
bool autostart_enabled(){return owned_shortcut(autostart_path());}
void autostart(bool enable){
    if(geteuid()==0)throw std::runtime_error("Inicialização automática deve ser configurada como usuário normal, sem sudo.");
    fs::path p=autostart_path();
    if(fs::is_symlink(p)||(fs::exists(p)&&!owned_shortcut(p)))throw std::runtime_error("O atalho existente pertence a outra pasta e foi preservado.");
    if(enable){
        Json::Array created=Config::load().original["autostart_created_dirs"].items();
        for(fs::path d=p.parent_path();!d.empty()&&!fs::exists(d);d=d.parent_path())created.emplace_back(d.string());
        preference("autostart_path",p.string());preference("autostart_created_dirs",created);
        write_external_shortcut(folder(),{p,"autostart",shortcut_text(folder(),"autostart",detect_environment(false).trinity)});
    }else{
        remove_external_shortcut(folder(),"autostart");
        // Compatibilidade com atalhos v0.2 que ainda não tinham integracoes.json.
        if(fs::exists(p)&&owned_shortcut(p))fs::remove(p);
        Json::Array created=Config::load().original["autostart_created_dirs"].items();
        for(const auto& d:created){fs::path dir=d.text();if(!dir.empty()&&dir!=dir.root_path())rmdir(dir.c_str());}
        preference("autostart_path",Json());preference("autostart_created_dirs",Json::Array{});
    }
}
int diagnostics(){
    std::cout<<"ÁUDIO PAINEL RC C++ "<<VERSION<<" — DIAGNÓSTICO SOMENTE LEITURA\n";
    std::cout<<"Pasta: "<<folder()<<"\n"<<detect_environment(false).text()<<"\nDISPLAY: "<<env("DISPLAY","ausente")<<'\n';
    auto packages=run_read({"dpkg-query","-W","-f=${Package} ${Version}\n","wireplumber","pipewire","libgtk-3-0t64"});std::cout<<packages.text;
    // wpctl 0.5.8 não suporta --version. Não confundir a versão da libpipewire
    // anunciada pelo cliente com a versão do WirePlumber.
    for(const auto& args:std::vector<std::vector<std::string>>{{"wpctl","--help"},{"pw-dump","--version"},{"wpctl","status"},{"wpctl","get-volume","@DEFAULT_AUDIO_SINK@"},{"wpctl","get-volume","@DEFAULT_AUDIO_SOURCE@"}}){
        std::cout<<"\n$";for(const auto& s:args)std::cout<<' '<<s;auto r=run_read(args,5000,262144);std::cout<<"\n"<<r.text<<"\nCódigo: "<<r.status<<(r.timeout?" (tempo limite)":"")<<'\n';
    }
    auto r=run_read({"pw-dump","-N"},6000);if(r.status==0)try{
        Graph g=read_graph(r.text);State s=g.state();
        std::cout<<"\nNós de áudio: "<<s.nodes.size()<<" · grupos de aplicativos: "<<s.apps().size()<<"\n";
        std::cout<<"Filtro associado: "<<(s.filtered?"sim":"não")<<"\nEntrada física: "<<(s.physical?s.physical->label:"não identificada")<<"\n";
        if(s.physical)std::cout<<"Mudo físico: "<<(s.physical->muted?"sim":"não")<<"\n";
        for(const auto& c:s.outputs)std::cout<<"Saída: "<<c.label<<" · rota "<<c.route<<" · "<<c.node.name<<'\n';
        for(const auto& c:s.inputs)std::cout<<"Entrada: "<<c.label<<" · rota "<<c.route<<" · "<<c.node.name<<'\n';
    }catch(const std::exception& e){std::cout<<e.what()<<'\n';}
    std::cout<<"\nAutostart próprio: "<<(autostart_enabled()?"ativo":"desativado")<<"\n"<<external_report()<<"\nKMix, filtro e automação HDMI não foram alterados. Nenhum arquivo foi criado.\n";
    return 0;
}
Rect anchor(Rect a,Rect i,bool have,int orientation,int w,int h,int gap){
    int x=a.x+a.width-w-gap,y=a.y+a.height-h-gap;
    if(have){
        if(orientation==1){bool left=i.x+i.width/2<a.x+a.width/2;x=left?std::max(i.x+i.width,a.x)+gap:std::min(i.x,a.x+a.width)-w-gap;y=i.y+i.height-h;}
        else{x=i.x+i.width-w;bool top=i.y+i.height/2<a.y+a.height/2;y=top?std::max(i.y+i.height,a.y)+gap:std::min(i.y,a.y+a.height)-h-gap;}
    }
    x=std::max(a.x,std::min(x,a.x+a.width-w));y=std::max(a.y,std::min(y,a.y+a.height-h));return {x,y,w,h};
}
bool in_area(Rect r,Point p,int m){return p.x>=r.x-m&&p.x<r.x+r.width+m&&p.y>=r.y-m&&p.y<r.y+r.height+m;}
bool pointer_safe(Rect p,Rect i,Point point,int margin,bool have){
    if(in_area(p,point,margin))return true;if(!have)return false;if(in_area(i,point,margin))return true;
    // Corredor estreito entre o ícone e a borda do painel, para atravessar
    // os poucos pixels de distância sem disparar o fechamento.
    if(i.y>=p.y+p.height){Rect r{i.x-margin,p.y+p.height,i.width+2*margin,std::max(0,i.y-p.y-p.height)};return in_area(r,point);}
    if(i.y+i.height<=p.y){Rect r{i.x-margin,i.y+i.height,i.width+2*margin,p.y-i.y-i.height};return in_area(r,point);}
    if(i.x>=p.x+p.width){Rect r{p.x+p.width,i.y-margin,i.x-p.x-p.width,i.height+2*margin};return in_area(r,point);}
    if(i.x+i.width<=p.x){Rect r{i.x+i.width,i.y-margin,p.x-i.x-i.width,i.height+2*margin};return in_area(r,point);}
    return false;
}
}

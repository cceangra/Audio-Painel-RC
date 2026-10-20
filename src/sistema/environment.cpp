#include "environment.hpp"
#include <fstream>
#include <set>
#include <sstream>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
namespace rc {
std::string env_value(const char* key,const std::string& fallback){const char* p=getenv(key);return p&&*p?p:fallback;}
bool program_exists(const std::string& name){
    if(name.find('/')!=std::string::npos)return access(name.c_str(),X_OK)==0;
    std::istringstream path(env_value("PATH"));std::string part;
    while(std::getline(path,part,':'))if(!part.empty()&&access((fs::path(part)/name).c_str(),X_OK)==0)return true;return false;
}
static std::string os_name(){
    try{std::istringstream f(read_file("/etc/os-release",65536));std::string line;
        while(std::getline(f,line))if(line.rfind("PRETTY_NAME=",0)==0){auto s=line.substr(12);if(s.size()>=2&&s.front()=='"'&&s.back()=='"')s=s.substr(1,s.size()-2);return s;}}
    catch(...){}return "Linux (distribuição não identificada)";
}
Environment detect_environment(bool probe_audio){
    Environment e;e.distro=os_name();utsname un{};e.architecture=uname(&un)==0?un.machine:"desconhecida";
    e.desktop=env_value("XDG_CURRENT_DESKTOP",env_value("XDG_SESSION_DESKTOP",env_value("DESKTOP_SESSION","não identificado")));
    std::string lower=e.desktop;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return char(std::tolower(c));});
    e.trinity=lower.find("trinity")!=std::string::npos||lower=="tde"||!env_value("TDE_FULL_SESSION").empty();
    if(e.trinity&&lower=="não identificado")e.desktop="Trinity";
    e.session=env_value("XDG_SESSION_TYPE");e.wayland=e.session=="wayland"||!env_value("WAYLAND_DISPLAY").empty();
    if(e.session.empty())e.session=e.wayland?"wayland":env_value("DISPLAY").empty()?"sem sessão gráfica":"x11";
    bool pw=program_exists("pw-dump"),wp=program_exists("wpctl");
    e.backend=pw&&wp?"PipeWire / WirePlumber":program_exists("pactl")?"PulseAudio ou compatibilidade PulseAudio":"não identificado";
    e.audio_note="Controle completo requer PipeWire, WirePlumber, pw-dump e wpctl disponíveis na sessão.";
    if(pw&&wp&&probe_audio){auto r=run_read({"wpctl","get-volume","@DEFAULT_AUDIO_SINK@"},2500,8192);e.audio_ready=r.status==0;
        e.audio_note=e.audio_ready?"Saída padrão responde. Mixer e dispositivos serão acompanhados pelo PipeWire.":"Ferramentas encontradas, mas a saída padrão não respondeu. Verifique a sessão de áudio após instalar.";
    }else if(pw&&wp)e.audio_note="Ferramentas PipeWire encontradas; sessão de áudio ainda não testada.";
    std::set<std::string> agents;std::error_code error;
    const std::set<std::string> names={"kmix","kded","kded5","kded6","gsd-media-keys","xfce4-pulseaudio-plugin","mate-settings-daemon","csd-media-keys","lxqt-globalkeysd"};
    for(const auto& p:fs::directory_iterator("/proc",error))try{
        auto name=p.path().filename().string();if(name.empty()||!std::all_of(name.begin(),name.end(),[](unsigned char c){return std::isdigit(c);}))continue;
        struct stat st{};if(stat(p.path().c_str(),&st)||st.st_uid!=getuid())continue;
        auto cmd=read_file(p.path()/"cmdline",32768);cmd=cmd.substr(0,cmd.find('\0'));cmd=fs::path(cmd.substr(0,cmd.find(' '))).filename().string();if(names.count(cmd))agents.insert(cmd);
    }catch(...){}
    e.volume_agents.assign(agents.begin(),agents.end());return e;
}
std::string Environment::text()const{
    std::string s=distro+" · "+architecture+"\nDesktop: "+desktop+" · sessão: "+session+"\nÁudio: "+backend+"\n"+audio_note;
    if(!volume_agents.empty()){s+="\nPossíveis responsáveis por teclas: ";for(size_t n=0;n<volume_agents.size();++n)s+=(n?", ":"")+volume_agents[n];s+=". A presença não confirma conflito.";}
    if(wayland)s+="\nWayland: esta versão acompanha as teclas do desktop em modo compatibilidade; não usa XGrabKey pelo XWayland.";
    return s;
}
static fs::path absolute_home(){fs::path home=env_value("HOME");if(!home.is_absolute()||home==home.root_path())throw std::runtime_error("Pasta pessoal inválida");return home;}
fs::path startup_location(){
    std::string desktop=env_value("XDG_CURRENT_DESKTOP",env_value("DESKTOP_SESSION"));std::transform(desktop.begin(),desktop.end(),desktop.begin(),[](unsigned char c){return char(std::tolower(c));});
    bool trinity=desktop=="tde"||desktop.find("trinity")!=std::string::npos||!env_value("TDE_FULL_SESSION").empty();
    if(trinity){fs::path tde=env_value("TDEHOME",(absolute_home()/".trinity").string());if(!tde.is_absolute())throw std::runtime_error("TDEHOME precisa ser absoluto");return tde/"Autostart/Audio-Painel-RC-CPP.desktop";}
    fs::path xdg=env_value("XDG_CONFIG_HOME",(absolute_home()/".config").string());if(!xdg.is_absolute())throw std::runtime_error("XDG_CONFIG_HOME precisa ser absoluto");return xdg/"autostart/Audio-Painel-RC-CPP.desktop";
}
fs::path menu_location(){fs::path xdg=env_value("XDG_DATA_HOME",(absolute_home()/".local/share").string());if(!xdg.is_absolute())throw std::runtime_error("XDG_DATA_HOME precisa ser absoluto");return xdg/"applications/Audio-Painel-RC-CPP.desktop";}
fs::path desktop_location(){
    // Apenas leitura do formato XDG, sem source/eval ou expansão de comandos.
    fs::path config=env_value("XDG_CONFIG_HOME",(absolute_home()/".config").string());
    try{std::istringstream f(read_file(config/"user-dirs.dirs",65536));std::string line;while(std::getline(f,line))if(line.rfind("XDG_DESKTOP_DIR=\"",0)==0){auto end=line.find('"',17);if(end==std::string::npos)continue;auto v=line.substr(17,end-17);if(v.rfind("$HOME/",0)==0)v=absolute_home().string()+v.substr(5);fs::path p=v;if(p.is_absolute()&&p!=absolute_home())return p/"Audio-Painel-RC-CPP.desktop";}}
    catch(...){}return absolute_home()/"Desktop/Audio-Painel-RC-CPP.desktop";
}
std::vector<std::string> keyboard_settings_command(){
    auto e=detect_environment(false);std::string d=e.desktop;std::transform(d.begin(),d.end(),d.begin(),[](unsigned char c){return char(std::tolower(c));});
    if(e.trinity&&program_exists("/opt/trinity/bin/tdecmshell"))return {"/opt/trinity/bin/tdecmshell","keys"};
    if(d.find("xfce")!=std::string::npos&&program_exists("xfce4-keyboard-settings"))return {"xfce4-keyboard-settings"};
    if(d.find("gnome")!=std::string::npos&&program_exists("gnome-control-center"))return {"gnome-control-center","keyboard"};
    if(d.find("cinnamon")!=std::string::npos&&program_exists("cinnamon-settings"))return {"cinnamon-settings","keyboard"};
    if(d.find("mate")!=std::string::npos&&program_exists("mate-keybinding-properties"))return {"mate-keybinding-properties"};
    if(d.find("kde")!=std::string::npos||d.find("plasma")!=std::string::npos){if(program_exists("systemsettings"))return {"systemsettings","kcm_keys"};if(program_exists("systemsettings5"))return {"systemsettings5","kcm_keys"};}
    return {};
}
void launch_detached(const fs::path& exe,const std::vector<std::string>& args){
    pid_t child=fork();if(child<0)throw std::runtime_error("Não foi possível abrir o aplicativo");
    if(child==0){pid_t second=fork();if(second<0)_exit(127);if(second>0)_exit(0);setsid();int fd=open("/dev/null",O_RDWR);if(fd>=0){dup2(fd,0);dup2(fd,1);dup2(fd,2);if(fd>2)close(fd);}
        std::string executable=exe.string();std::vector<char*> av{executable.data()};for(const auto& a:args)av.push_back(const_cast<char*>(a.c_str()));av.push_back(nullptr);execvp(executable.c_str(),av.data());_exit(127);
    }int status;while(waitpid(child,&status,0)<0&&errno==EINTR){}
}
}

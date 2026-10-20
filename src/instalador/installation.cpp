#include "installation.hpp"
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
namespace rc {
// SHA-256 incremental: evita bibliotecas criptográficas e carregar ZIPs na RAM.
class Sha256 {
    std::array<uint32_t,8> h{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    std::array<unsigned char,64> buffer{};size_t used=0;uint64_t bytes=0;
    static uint32_t r(uint32_t x,int n){return (x>>n)|(x<<(32-n));}
    void block(const unsigned char* p){
        static constexpr uint32_t k[]={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
        uint32_t w[64];for(int i=0;i<16;++i)w[i]=uint32_t(p[4*i])<<24|uint32_t(p[4*i+1])<<16|uint32_t(p[4*i+2])<<8|p[4*i+3];
        for(int i=16;i<64;++i)w[i]=w[i-16]+(r(w[i-15],7)^r(w[i-15],18)^(w[i-15]>>3))+w[i-7]+(r(w[i-2],17)^r(w[i-2],19)^(w[i-2]>>10));
        auto a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],z=h[7];
        for(int i=0;i<64;++i){uint32_t t=z+(r(e,6)^r(e,11)^r(e,25))+((e&f)^(~e&g))+k[i]+w[i];z=g;g=f;f=e;e=d+t;d=c;c=b;b=a;a=t+(r(b,2)^r(b,13)^r(b,22))+((b&c)^(b&d)^(c&d));}
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=z;
    }
public:
    void add(const void* data,size_t n){auto p=static_cast<const unsigned char*>(data);bytes+=n;while(n){size_t amount=std::min(n,64-used);memcpy(buffer.data()+used,p,amount);p+=amount;n-=amount;used+=amount;if(used==64){block(buffer.data());used=0;}}}
    std::string finish(){uint64_t bits=bytes*8;unsigned char end=0x80;add(&end,1);unsigned char zero=0;while(used!=56)add(&zero,1);unsigned char length[8];for(int i=0;i<8;++i)length[7-i]=static_cast<unsigned char>(bits>>(8*i));add(length,8);std::ostringstream s;s<<std::hex<<std::setfill('0');for(auto v:h)s<<std::setw(8)<<v;return s.str();}
};
std::string sha256_text(const std::string& text){Sha256 h;h.add(text.data(),text.size());return h.finish();}
std::string sha256_file(const fs::path& path){
    int fd=open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)throw std::runtime_error("Não foi possível verificar "+path.string());
    Sha256 h;char b[65536];ssize_t n;while((n=read(fd,b,sizeof b))>0)h.add(b,size_t(n));int error=errno;close(fd);if(n<0)throw std::runtime_error("Falha de leitura: "+std::string(strerror(error)));return h.finish();
}
bool safe_relative_path(const fs::path& p){if(p.empty()||p.is_absolute())return false;for(const auto& c:p)if(c=="."||c==".."||c.empty())return false;return p.string().find_first_of("\r\n") == std::string::npos;}
void reject_links(const fs::path& path){
    fs::path part;for(const auto& c:fs::absolute(path).lexically_normal()){part/=c;std::error_code ec;if(fs::is_symlink(part,ec))throw std::runtime_error("Link simbólico no caminho; escolha uma pasta real: "+part.string());}
}
static void safe_root(const fs::path& root){
    if(!root.is_absolute()||root==root.root_path()||root==fs::path(env_value("HOME"))||root.string().size()<8||root.string().find_first_of("\r\n")!=std::string::npos)throw std::runtime_error("Escolha uma subpasta própria para o aplicativo.");reject_links(root);
}
static bool within(const fs::path& p,const fs::path& base){auto rel=p.lexically_relative(base);return !rel.empty()&&rel!="."&&safe_relative_path(rel);}
static bool zip_file(const fs::path& p){auto ext=p.extension().string();std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return char(std::tolower(c));});return ext==".zip";}
static Json load_json(const fs::path& p,size_t limit=4*1024*1024){reject_links(p);return fs::exists(p)?Json::parse(read_file(p,limit)):Json(Json::Object{});}
static void save_json(const fs::path& p,const Json& j){atomic_write(p,j.dump()+"\n");}
static std::string now(){return std::to_string(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());}
static std::vector<fs::path> listed(const fs::path& root){
    reject_links(root/"IDENTIDADE-RC");if(read_file(root/"IDENTIDADE-RC",128)!=std::string(APP_ID)+"\n")throw std::runtime_error("A pasta não tem a identidade do Áudio RC.");
    std::set<fs::path> out;std::istringstream stream(read_file(root/"ARQUIVOS.txt",262144));std::string line;
    while(std::getline(stream,line)){if(line.empty())continue;fs::path p=line;if(!safe_relative_path(p))throw std::runtime_error("Caminho inválido em ARQUIVOS.txt: "+line);reject_links(root/p);if(!fs::is_regular_file(root/p))throw std::runtime_error("Arquivo do pacote ausente: "+line);out.insert(p);}
    if(!out.count("audio-painel-rc")||!out.count("IDENTIDADE-RC")||!out.count("ARQUIVOS.txt"))throw std::runtime_error("Lista de arquivos incompleta.");return {out.begin(),out.end()};
}
static std::string desktop_quote(const std::string& text){
    std::string out="\"";for(char c:text){if(c=='%' ){out+="%%";continue;}if(c=='"'||c=='`'||c=='$'||c=='\\')out+='\\';if(c=='\n'||c=='\r')throw std::runtime_error("Caminho contém quebra de linha");out+=c;}return out+'"';
}
static std::string desktop_value(const std::string& text){std::string out;for(char c:text){if(c=='\\')out+="\\\\";else if(c=='\n'||c=='\r')throw std::runtime_error("Caminho contém quebra de linha");else out+=c;}return out;}
std::string shortcut_text(const fs::path& root,const std::string& kind,bool trinity){
    std::string s="[Desktop Entry]\nVersion=1.0\nType=Application\nName=Áudio Painel RC\nComment=Volume, microfone e aplicativos\nExec="+desktop_quote((root/"audio-painel-rc").string())+(kind=="autostart"?"":" --janela")+"\nTryExec="+desktop_value((root/"audio-painel-rc").string())+"\nIcon="+desktop_value((root/"icones/tray-high.svg").string())+"\nTerminal=false\nCategories=AudioVideo;Audio;Mixer;\nStartupNotify=false\nX-RC-App="+APP_ID+"\nX-RC-Root="+desktop_value(root.string())+"\nX-RC-Kind="+kind+"\n";
    if(kind=="autostart"&&trinity)s+="OnlyShowIn=TDE;\nX-TDE-autostart-phase=2\nX-TDE-autostart-after=panel\nX-TDE-StartupNotify=false\n";
    else if(kind=="autostart")s+="NotShowIn=TDE;\n";return s;
}
static bool marker_matches(const fs::path& p,const fs::path& root){
    if(!fs::exists(p)||fs::is_symlink(p))return false;auto text=read_file(p,65536);return text.find(std::string("\nX-RC-App=")+APP_ID+"\n")!=std::string::npos&&text.find("\nX-RC-Root="+desktop_value(root.string())+"\n")!=std::string::npos;
}
static Json ledger(const fs::path& root){
    auto j=load_json(root/"integracoes.json");if(!j["app"].null()&&j["app"].text()!=APP_ID)throw std::runtime_error("Registro de integrações pertence a outro programa.");
    if(j["entries"].null())j["entries"]=Json::Array{};j["schema"]=1;j["app"]=APP_ID;return j;
}
static bool entry_hash_matches(const fs::path&,const Json&);
static void assert_external_allowed(const fs::path& root,const ExternalSpec& s,const fs::path& oldroot={}){
    if(!s.path.is_absolute()||s.path.filename()!="Audio-Painel-RC-CPP.desktop"||within(s.path,root))throw std::runtime_error("Caminho de integração inválido: "+s.path.string());reject_links(s.path);
    if(!fs::exists(s.path))return;
    for(const auto& r:{root,oldroot})if(!r.empty()&&fs::exists(r)){
        auto l=ledger(r);for(const auto& e:l["entries"].items())if(e["path"].text()==s.path.string()&&!e["sha256"].text().empty()){
            if(!entry_hash_matches(s.path,e))throw std::runtime_error("Atalho editado depois da instalação; foi preservado: "+s.path.string());
            if(marker_matches(s.path,r)||(!l["root"].text().empty()&&marker_matches(s.path,l["root"].text())))return;
        }
    }
    for(auto r:{root,oldroot})if(!r.empty()&&marker_matches(s.path,r)){
        auto l=ledger(r);for(const auto& e:l["entries"].items())if(e["path"].text()==s.path.string()&&!e["sha256"].text().empty()&&!entry_hash_matches(s.path,e))throw std::runtime_error("Atalho editado depois da instalação; foi preservado: "+s.path.string());return;
    }throw std::runtime_error("Já existe um atalho de outra instalação: "+s.path.string());
}
static Json::Array missing_parents(const fs::path& p){Json::Array dirs;for(auto d=p.parent_path();!d.empty()&&!fs::exists(d);d=d.parent_path())dirs.emplace_back(d.string());return dirs;}
static fs::path external_temporary(const fs::path& path,const std::string& token){return path.parent_path()/(".Audio-Painel-RC-"+token+".tmp");}
static void publish_external(const fs::path& path,const fs::path& temporary,const std::string& text,unsigned mode){
    reject_links(path);reject_links(temporary);if(path.parent_path()!=temporary.parent_path()||temporary.filename().string().rfind(".Audio-Painel-RC-",0)!=0)throw std::runtime_error("Temporário de integração inválido");
    fs::create_directories(path.parent_path());int fd=open(temporary.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)throw std::runtime_error("Não foi possível criar o temporário registrado: "+temporary.string());
    try{size_t off=0;while(off<text.size()){ssize_t n=write(fd,text.data()+off,text.size()-off);if(n<0&&errno==EINTR)continue;if(n<=0)throw std::runtime_error("Falha ao gravar integração");off+=size_t(n);}if(fchmod(fd,mode)||fsync(fd))throw std::runtime_error("Falha ao confirmar integração");close(fd);fd=-1;
        if(rename(temporary.c_str(),path.c_str()))throw std::runtime_error("Falha ao publicar integração: "+path.string());int dir=open(path.parent_path().c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(dir>=0){fsync(dir);close(dir);}
    }catch(...){if(fd>=0)close(fd);unlink(temporary.c_str());throw;}
}
static void clear_external_temporaries(const fs::path& path,const Json& entry){
    for(const auto& value:entry["temporary_paths"].items()){fs::path temp=value.text();if(temp.parent_path()!=path.parent_path()||temp.filename().string().rfind(".Audio-Painel-RC-",0)!=0||temp.extension()!=".tmp")throw std::runtime_error("Temporário externo inválido no registro");reject_links(temp);
        struct stat st{};if(lstat(temp.c_str(),&st)==0){if(!S_ISREG(st.st_mode)||st.st_uid!=getuid())throw std::runtime_error("Temporário externo de outro usuário; preservado: "+temp.string());fs::remove(temp);}}
}
static bool entry_hash_matches(const fs::path& path,const Json& e){auto h=sha256_file(path);return h==e["sha256"].text()||(e["state"].text()=="prepared"&&!e["prior_sha256"].text().empty()&&h==e["prior_sha256"].text());}
void replace_registered_external(const fs::path& path,const fs::path& temporary,const std::string& text,unsigned mode){publish_external(path,temporary,text,mode);}
void clear_registered_temporaries(const fs::path& path,const Json& record){clear_external_temporaries(path,record);}
void write_external_shortcut(const fs::path& root,const ExternalSpec& spec){
    assert_external_allowed(root,spec);auto j=ledger(root);auto entries=j["entries"].items();Json::Array keep;Json entry;
    for(const auto& e:entries)if(e["path"].text()==spec.path.string())entry=e;else keep.push_back(e);
    auto dirs=entry["created_dirs"].items();auto added=missing_parents(spec.path);dirs.insert(dirs.end(),added.begin(),added.end());
    auto temporary=external_temporary(spec.path,now()+"-"+std::to_string(getpid())+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));auto temporaries=entry["temporary_paths"].items();temporaries.emplace_back(temporary.string());
    entry=Json::Object{{"path",spec.path.string()},{"kind",spec.kind},{"sha256",sha256_text(spec.content)},{"prior_sha256",fs::exists(spec.path)?sha256_file(spec.path):""},{"state","prepared"},{"created_dirs",dirs},{"temporary_paths",temporaries},{"recorded_at",now()}};
    keep.push_back(entry);j["entries"]=keep;j["root"]=root.string();save_json(root/"integracoes.json",j);
    publish_external(spec.path,temporary,spec.content,spec.kind=="desktop"?0755:0644);
    keep.back()["state"]="active";j["entries"]=keep;save_json(root/"integracoes.json",j);
}
static void prune_dirs(const Json& entry){for(const auto& d:entry["created_dirs"].items()){fs::path p=d.text();if(p.is_absolute()&&p!=p.root_path()&&p!=fs::path(env_value("HOME")))rmdir(p.c_str());}}
void remove_external_shortcut(const fs::path& root,const std::string& kind){
    auto j=ledger(root);Json::Array keep;for(const auto& e:j["entries"].items()){
        if(!kind.empty()&&e["kind"].text()!=kind){keep.push_back(e);continue;}
        fs::path p=e["path"].text();if(!p.is_absolute()||p.filename()!="Audio-Painel-RC-CPP.desktop")throw std::runtime_error("Registro externo inválido");reject_links(p);
        if(fs::exists(p)){if(!entry_hash_matches(p,e))throw std::runtime_error("Atalho alterado posteriormente. Registro preservado para revisão: "+p.string());fs::remove(p);}clear_external_temporaries(p,e);prune_dirs(e);
    }j["entries"]=keep;save_json(root/"integracoes.json",j);
}
bool installed_here(const fs::path& root){try{auto j=load_json(root/"instalacao.json");return j["app"].text()==APP_ID&&j["installed"].boolean()&&j["root"].text()==root.string();}catch(...){return false;}}
static void ensure_not_running(const fs::path& root){
    std::error_code error;for(const auto& p:fs::directory_iterator("/proc",error))try{auto id=p.path().filename().string();if(id.empty()||!std::all_of(id.begin(),id.end(),[](unsigned char c){return std::isdigit(c);})||std::stoi(id)==proc_self_pid())continue;
        auto exe=fs::read_symlink(p.path()/"exe").string();if(exe==(root/"audio-painel-rc").string()||exe==(root/"audio-painel-rc").string()+" (deleted)")throw std::runtime_error("Feche o painel e a central desta pasta antes de instalar: "+root.string());
    }catch(const fs::filesystem_error&){}catch(const std::invalid_argument&){}
}
InstallPlan plan_install(const InstallOptions& raw){
    InstallPlan p;p.options=raw;auto& o=p.options;o.source=fs::absolute(o.source).lexically_normal();o.destination=fs::absolute(o.destination).lexically_normal();safe_root(o.source);safe_root(o.destination);
    p.inplace=o.source==o.destination;if(!p.inplace&&(within(o.destination,o.source)||within(o.source,o.destination)))throw std::runtime_error("A origem e o destino não podem ficar uma dentro da outra. Use 'Usar esta pasta' ou outra pasta independente.");
    p.files=listed(o.source);ensure_not_running(o.source);if(!p.inplace)ensure_not_running(o.destination);
    if(fs::exists(o.destination)&&!fs::is_directory(o.destination))throw std::runtime_error("O destino não é uma pasta.");
    if(!p.inplace&&fs::exists(o.destination)&&!fs::is_empty(o.destination)){
        bool pending_only=true;for(const auto& f:fs::directory_iterator(o.destination)){if(f.path().filename().string().rfind(".instalacao-pendente-",0)!=0)pending_only=false;else if(fs::is_directory(f.symlink_status())){
            auto record=load_json(f.path()/"transacao.json");if(record["app"].text()==APP_ID&&record["root"].text()==o.destination.string())p.pending_recovery=true;}}
        if(!pending_only&&!p.pending_recovery&&(!fs::exists(o.destination/"IDENTIDADE-RC")||read_file(o.destination/"IDENTIDADE-RC",128)!=std::string(APP_ID)+"\n"))throw std::runtime_error("O destino contém outros arquivos. Escolha uma pasta vazia ou uma instalação do Áudio RC.");p.update=!pending_only;
    }
    std::set<fs::path> known(p.files.begin(),p.files.end());
    for(const auto& path:{"instalacao.json","integracoes.json","RELATORIO-INSTALACAO.txt","RELATORIO-REMOCAO.txt"})if(fs::is_regular_file(o.source/path)){if(known.insert(path).second)p.files.emplace_back(path);}
    if(fs::exists(o.source/"integracao-kmix"))for(const auto& f:fs::directory_iterator(o.source/"integracao-kmix"))if(fs::is_regular_file(f)&&!fs::is_symlink(f)){auto rel=f.path().lexically_relative(o.source);if(known.insert(rel).second)p.files.push_back(rel);}
    if(!p.inplace)for(const auto& f:fs::recursive_directory_iterator(o.source,fs::directory_options::none)){
        auto rel=f.path().lexically_relative(o.source);if(fs::is_directory(f.symlink_status()))continue;if(!known.count(rel))p.extras.push_back(rel);
    }
    if(p.files.size()+p.extras.size()>20000)throw std::runtime_error("A pasta de origem tem mais de 20 mil arquivos. Use uma extração limpa do ZIP.");
    for(const auto& f:p.files){reject_links(o.source/f);p.bytes+=fs::file_size(o.source/f);}
    if(o.gather_extras)for(const auto& f:p.extras)if(fs::is_regular_file(fs::symlink_status(o.source/f)))p.bytes+=fs::file_size(o.source/f);
    if(!o.archive.empty()){
        o.archive=fs::absolute(o.archive).lexically_normal();reject_links(o.archive);if(!zip_file(o.archive)||!fs::is_regular_file(o.archive))throw std::runtime_error("Selecione um ZIP de download válido.");
        if(within(o.archive,o.destination))o.archive.clear();else p.bytes+=fs::file_size(o.archive);
    }
    bool tde=detect_environment(false).trinity;
    if(o.startup)p.external.push_back({startup_location(),"autostart",shortcut_text(o.destination,"autostart",tde)});
    if(o.menu)p.external.push_back({menu_location(),"menu",shortcut_text(o.destination,"menu",tde)});
    if(o.desktop)p.external.push_back({desktop_location(),"desktop",shortcut_text(o.destination,"desktop",tde)});
    // Migrar atalhos registrados da origem, inclusive os de outro desktop anterior.
    auto source_ledger=ledger(o.source);for(const auto& e:source_ledger["entries"].items()){
        fs::path path=e["path"].text();if(!fs::exists(path))continue;bool found=std::any_of(p.external.begin(),p.external.end(),[&](const auto& x){return x.path==path;});
        if(!found&&(!p.inplace||source_ledger["root"].text()!=o.destination.string()))p.external.push_back({path,e["kind"].text(),shortcut_text(o.destination,e["kind"].text(),path.parent_path().filename()=="Autostart")});
    }
    // O registro legado v0.2 também é migrado, sem duplicar o autostart.
    auto cfg=load_json(o.source/"preferencias.json",65536);auto legacy=cfg["autostart_path"].text();
    if(!p.inplace&&!legacy.empty()&&marker_matches(legacy,o.source)&&std::none_of(p.external.begin(),p.external.end(),[&](const auto& s){return s.path==fs::path(legacy);}))p.external.push_back({legacy,"autostart",shortcut_text(o.destination,"autostart",fs::path(legacy).parent_path().filename()=="Autostart")});
    for(const auto& s:p.external)assert_external_allowed(o.destination,s,o.source);
    for(const auto& f:p.files)reject_links(o.destination/f);
    return p;
}
std::string InstallPlan::summary()const{
    std::ostringstream s;s<<(inplace?"Modo portátil nesta pasta":"Destino da instalação")<<":\n"<<options.destination.string()<<"\n\n"<<files.size()<<" arquivos do aplicativo · "<<std::fixed<<std::setprecision(1)<<bytes/1048576.0<<" MiB para copiar/verificar.\n";
    if(update)s<<"Atualização: preferências e registros anteriores do destino serão preservados.\n";
    if(pending_recovery)s<<"Há uma instalação interrompida registrada. O instalador tentará restaurá-la antes de continuar e preservará qualquer edição posterior.\n";
    if(!inplace){s<<(options.gather_source?"A origem será recolhida após verificar a instalação.":"A pasta de origem será preservada fora do destino.")<<"\n";if(!extras.empty())s<<extras.size()<<" arquivo(s) adicional(is): "<<(options.gather_extras?"recolher para instalador/arquivos-preservados; preservar ao desinstalar.":"permanecer na origem, com seus caminhos registrados.")<<"\n";}
    if(!options.archive.empty())s<<"ZIP: mover para instalador/"<<options.archive.filename().string()<<". Não será apagado na desinstalação.\n";
    else s<<"ZIP de download não selecionado. Se estiver fora desta pasta, permanecerá no local onde você o guardou.\n";
    s<<"\nArquivos externos do aplicativo:\n";if(external.empty())s<<"Nenhum atalho externo novo.\n";
    for(const auto& e:external)s<<"• "<<e.path.string()<<"\n  "<<(e.kind=="autostart"?"O desktop lê este atalho ao entrar na sessão.":e.kind=="menu"?"O menu de aplicativos lê este diretório.":"Atalho opcional na área de trabalho.")<<"\n";
    s<<"\nCada integração é registrada nesta pasta; o desinstalador verifica o conteúdo antes de remover. Temporários de atalhos, quando necessários, usam os mesmos diretórios e também são registrados. KMix, servidor de áudio e atalhos atuais de volume não serão substituídos.\n";return s.str();
}
struct CopyItem {fs::path source,relative;bool managed=true;};
static void copy_verified(const fs::path& from,const fs::path& to,std::atomic<bool>& cancel){
    reject_links(from);reject_links(to);fs::create_directories(to.parent_path());int input=open(from.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(input<0)throw std::runtime_error("Falha ao abrir "+from.string());
    struct stat st{};if(fstat(input,&st)||!S_ISREG(st.st_mode)){close(input);throw std::runtime_error("Arquivo de origem não é regular: "+from.string());}
    int output=open(to.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(output<0){close(input);throw std::runtime_error("Falha ao copiar para "+to.string());}
    try{char b[65536];for(;;){if(cancel)throw std::runtime_error("Instalação cancelada; alterações em andamento serão desfeitas.");ssize_t n=read(input,b,sizeof b);if(n<0&&errno==EINTR)continue;if(n<0)throw std::runtime_error("Erro de leitura ao copiar");if(!n)break;size_t pos=0;while(pos<size_t(n)){ssize_t k=write(output,b+pos,size_t(n)-pos);if(k<0&&errno==EINTR)continue;if(k<=0)throw std::runtime_error("Erro de gravação ao copiar (verifique o espaço livre)");pos+=size_t(k);}}
        if(fchmod(output,unsigned(st.st_mode&0777))||fsync(output))throw std::runtime_error("Falha ao confirmar a cópia");close(input);input=-1;close(output);output=-1;
        if(sha256_file(from)!=sha256_file(to))throw std::runtime_error("A verificação SHA-256 da cópia falhou: "+from.string());
    }catch(...){if(input>=0)close(input);if(output>=0)close(output);throw;}
}
static void clean_private_stage(const fs::path& stage){
    // Somente a árvore privada gerada pelo instalador, nunca a pasta escolhida.
    if(stage.filename().string().rfind(".instalacao-pendente-",0)!=0)throw std::runtime_error("Pasta temporária inválida");reject_links(stage);fs::remove_all(stage);
}
static void rollback_stage(const fs::path& root,const fs::path& stage){
    auto j=load_json(stage/"transacao.json");if(j["app"].text()!=APP_ID||j["root"].text()!=root.string())throw std::runtime_error("Registro de recuperação inválido: "+stage.string());
    if(j["committed"].boolean()){clean_private_stage(stage);return;}
    auto actions=j["actions"].items();
    // Verificar TODOS antes de restaurar qualquer um.
    for(const auto& a:actions){fs::path p=a["path"].text();if(!p.is_absolute()||(!within(p,root)&&p.filename()!="Audio-Painel-RC-CPP.desktop")||!safe_relative_path(a["backup"].text()))throw std::runtime_error("Caminho inválido na recuperação");reject_links(p);if(fs::exists(p)){auto h=sha256_file(p);if(h!=a["new_hash"].text()&&h!=a["old_hash"].text())throw std::runtime_error("Arquivo alterado após a interrupção; recuperação manual necessária: "+p.string());}}
    std::atomic<bool> cancel{false};for(auto it=actions.rbegin();it!=actions.rend();++it){const auto& a=*it;fs::path p=a["path"].text();
        if(fs::exists(p)&&sha256_file(p)==a["new_hash"].text())fs::remove(p);
        if(a["existed"].boolean()&&!fs::exists(p)){fs::path rel=a["backup"].text();if(!safe_relative_path(rel))throw std::runtime_error("Backup inválido");auto b=stage/rel;if(sha256_file(b)!=a["old_hash"].text())throw std::runtime_error("Backup danificado: "+b.string());copy_verified(b,p,cancel);}
        if(!a["temporary"].text().empty())clear_external_temporaries(p,Json::Object{{"temporary_paths",Json::Array{a["temporary"]}}});
    }
    for(const auto& d:j["created_dirs"].items()){fs::path p=d.text();bool related=std::any_of(actions.begin(),actions.end(),[&](const auto& a){return within(fs::path(a["path"].text()),p);});if(related&&p.is_absolute()&&p!=p.root_path()&&p!=fs::path(env_value("HOME")))rmdir(p.c_str());}clean_private_stage(stage);
}
static void recover_unlocked(const fs::path& root){std::vector<fs::path> stages;for(const auto& f:fs::directory_iterator(root))if(f.path().filename().string().rfind(".instalacao-pendente-",0)==0)stages.push_back(f.path());for(const auto& s:stages)rollback_stage(root,s);}
void recover_installation(const fs::path& root){safe_root(root);if(!fs::exists(root))return;int fd=open(root.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(fd<0||flock(fd,LOCK_EX|LOCK_NB)){if(fd>=0)close(fd);throw std::runtime_error("Outra instalação está usando esta pasta.");}try{recover_unlocked(root);close(fd);}catch(...){close(fd);throw;}}
InstallResult perform_install(const InstallPlan& initial,std::atomic<bool>& cancel,const std::function<void(double,const std::string&)>& progress){
    auto p=plan_install(initial.options);auto o=p.options;InstallResult result;result.root=o.destination;
    auto say=[&](double v,const std::string& s){if(progress)progress(v,s);};say(.01,"Verificando caminhos e permissões");
    auto created_parent_dirs=missing_parents(o.destination);fs::create_directories(o.destination);
    // A trava fica na árvore de destino. Nenhum .lock em /tmp ou no HOME.
    int lock=open(o.destination.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(lock<0||flock(lock,LOCK_EX|LOCK_NB)){if(lock>=0)close(lock);throw std::runtime_error("Outra instalação está usando esta pasta.");}
    try{recover_unlocked(o.destination);}catch(...){close(lock);throw;}
    std::string pattern=(o.destination/".instalacao-pendente-XXXXXX").string();std::vector<char> tmp(pattern.begin(),pattern.end());tmp.push_back(0);if(!mkdtemp(tmp.data())){close(lock);throw std::runtime_error("Não é possível gravar na pasta escolhida.");}fs::path stage=tmp.data();
    Json journal=Json::Object{{"app",APP_ID},{"root",o.destination.string()},{"committed",false},{"actions",Json::Array{}},{"created_dirs",Json::Array{}}};bool journal_ready=false,committed=false;
    std::vector<CopyItem> copies;
    for(const auto& f:p.files)if(f!="instalacao.json"&&f!="integracoes.json"&&f!="RELATORIO-INSTALACAO.txt"&&f!="RELATORIO-REMOCAO.txt"&&f!="preferencias.json")copies.push_back({o.source/f,f,true});
    if(!p.inplace&&o.gather_extras)for(const auto& f:p.extras)if(o.source/f!=o.archive&&fs::is_regular_file(fs::symlink_status(o.source/f)))copies.push_back({o.source/f,fs::path("instalador/arquivos-preservados")/f,false});
    // Preservar o estado anterior dentro do instalador antes de substituir seus
    // registros. Assim preferências antigas não precisam sobrar fora da pasta.
    if(!p.inplace)for(const auto& f:p.files)if(f=="preferencias.json"||f=="instalacao.json"||f=="integracoes.json"||f=="RELATORIO-INSTALACAO.txt"||f=="RELATORIO-REMOCAO.txt")copies.push_back({o.source/f,fs::path("instalador/registros-origem")/f,true});
    fs::path archive_dest;if(!o.archive.empty()){archive_dest=fs::path("instalador")/o.archive.filename();copies.push_back({o.archive,archive_dest,false});}
    Json::Array managed,archives,preserved;auto previous=load_json(o.destination/"instalacao.json");
    for(const auto& f:previous["managed_files"].items())managed.push_back(f);
    for(const auto& f:previous["archives"].items())archives.push_back(f);
    for(const auto& f:previous["preserved_files"].items())preserved.push_back(f);
    std::vector<std::pair<fs::path,fs::path>> source_cleanup;
    try{
        save_json(stage/"transacao.json",journal);journal_ready=true;
        auto add_action=[&](const fs::path& to,const fs::path& staged){
            reject_links(to);bool existed=fs::exists(to);if(existed&&!fs::is_regular_file(to))throw std::runtime_error("O destino não é arquivo regular: "+to.string());
            std::string oldhash;fs::path backup=fs::path("backup")/std::to_string(journal["actions"].items().size());
            if(existed){oldhash=sha256_file(to);copy_verified(to,stage/backup,cancel);}
            auto dirs=journal["created_dirs"].items();auto missing=missing_parents(to);dirs.insert(dirs.end(),missing.begin(),missing.end());journal["created_dirs"]=dirs;
            auto a=journal["actions"].items();a.emplace_back(Json::Object{{"path",to.string()},{"staged",staged.lexically_relative(stage).string()},{"backup",backup.string()},{"existed",existed},{"old_hash",oldhash},{"new_hash",sha256_file(staged)}});journal["actions"]=a;
            if(!within(to,o.destination)){auto temp=external_temporary(to,stage.filename().string()+"-"+std::to_string(a.size()));reject_links(temp);if(fs::exists(temp))throw std::runtime_error("Já existe um temporário no caminho reservado; ele foi preservado: "+temp.string());a.back()["temporary"]=temp.string();journal["actions"]=a;}
        };
        size_t n=0;for(const auto& c:copies){if(cancel)throw std::runtime_error("Instalação cancelada.");say(.05+.50*double(n++)/std::max(size_t(1),copies.size()),"Copiando e verificando · "+c.relative.string());
            if(!safe_relative_path(c.relative))throw std::runtime_error("Destino interno inválido");auto staged=stage/"novos"/c.relative;copy_verified(c.source,staged,cancel);
            // Arquivos pessoais recolhidos nunca substituem arquivos anteriores.
            if(!c.managed&&fs::exists(o.destination/c.relative)&&sha256_file(o.destination/c.relative)!=sha256_file(staged))throw std::runtime_error("Já há outro arquivo guardado com esse nome: "+c.relative.string());
            add_action(o.destination/c.relative,staged);if(c.managed)managed.emplace_back(c.relative.string());else if(zip_file(c.relative))archives.emplace_back(c.relative.string());else preserved.emplace_back(c.relative.string());
            if(!p.inplace&&within(c.source,o.source))source_cleanup.emplace_back(c.source,c.relative);
        }
        if(access((stage/"novos/audio-painel-rc").c_str(),X_OK)!=0)throw std::runtime_error("A pasta de destino não permite executar o aplicativo. Verifique permissões ou a opção noexec do dispositivo. A origem foi preservada.");
        Json cfg=load_json((p.update&&fs::exists(o.destination/"preferencias.json")?o.destination:o.source)/"preferencias.json",65536);
        if(!o.keyboard["mode"].null()&&(!p.update||o.keyboard_changed))cfg["keyboard"]=o.keyboard;
        if(o.startup)cfg["autostart_path"]=startup_location().string();
        if(!p.inplace&&cfg["autostart_path"].text().empty()&&!p.external.empty())for(const auto& e:p.external)if(e.kind=="autostart")cfg["autostart_path"]=e.path.string();
        auto config_stage=stage/"novos/preferencias.json";fs::create_directories(config_stage.parent_path());save_json(config_stage,Config::from_json(cfg).json());add_action(o.destination/"preferencias.json",config_stage);managed.emplace_back("preferencias.json");
        Json external=ledger(p.update?o.destination:o.source);auto entries=external["entries"].items();
        for(const auto& e:p.external){assert_external_allowed(o.destination,e,o.source);Json::Array dirs=missing_parents(e.path);Json::Array keep;
            for(const auto& a:entries)if(a["path"].text()==e.path.string()){auto old=a["created_dirs"].items();dirs.insert(dirs.end(),old.begin(),old.end());}else keep.push_back(a);
            auto s=stage/"externos"/std::to_string(journal["actions"].items().size());fs::create_directories(s.parent_path());atomic_write(s,e.content,e.kind=="desktop"?0755:0644);add_action(e.path,s);
            auto temp_path=journal["actions"].items().back()["temporary"].text();keep.emplace_back(Json::Object{{"path",e.path.string()},{"kind",e.kind},{"sha256",sha256_text(e.content)},{"state","active"},{"created_dirs",dirs},{"temporary_paths",Json::Array{temp_path}},{"recorded_at",now()}});entries=keep;
        }
        external["entries"]=entries;external["root"]=o.destination.string();auto ledger_stage=stage/"novos/integracoes.json";save_json(ledger_stage,external);add_action(o.destination/"integracoes.json",ledger_stage);managed.emplace_back("integracoes.json");
        // Estes arquivos existem apenas dentro da pasta da instalação.
        managed.emplace_back("instalacao.json");managed.emplace_back("RELATORIO-INSTALACAO.txt");managed.emplace_back("RELATORIO-REMOCAO.txt");
        std::set<std::string> unique;Json::Array clean;for(const auto& f:managed)if(unique.insert(f.text()).second)clean.push_back(f);managed=clean;
        Json::Array pending_cleanup;
        if(!p.inplace)for(const auto& c:source_cleanup)pending_cleanup.emplace_back(Json::Object{{"path",c.first.string()},{"reason","Recolhimento da origem ainda não confirmado; conferir após interrupção."},{"remove_on_uninstall",false}});
        if(!o.archive.empty())pending_cleanup.emplace_back(Json::Object{{"path",o.archive.string()},{"reason","Transferência do ZIP ainda não confirmada; preservar se estiver presente."},{"remove_on_uninstall",false}});
        Json record=Json::Object{{"schema",1},{"app",APP_ID},{"version",VERSION},{"root",o.destination.string()},{"installed",true},{"installed_at",now()},{"source",o.source.string()},{"managed_files",managed},{"archives",archives},{"preserved_files",preserved},{"leftovers",pending_cleanup}};
        auto parent_dirs=previous["created_parent_dirs"].items();parent_dirs.insert(parent_dirs.end(),created_parent_dirs.begin(),created_parent_dirs.end());record["created_parent_dirs"]=parent_dirs;
        auto rs=stage/"novos/RELATORIO-INSTALACAO.txt";atomic_write(rs,p.summary());add_action(o.destination/"RELATORIO-INSTALACAO.txt",rs);
        auto rec=stage/"novos/instalacao.json";save_json(rec,record);add_action(o.destination/"instalacao.json",rec);
        save_json(stage/"transacao.json",journal);journal_ready=true;
        // Pré-validar o estado inteiro antes da primeira publicação.
        for(const auto& a:journal["actions"].items()){fs::path path=a["path"].text();reject_links(path);if(fs::exists(path)!=a["existed"].boolean()||(fs::exists(path)&&sha256_file(path)!=a["old_hash"].text()))throw std::runtime_error("Um arquivo mudou durante a instalação: "+path.string());}
        n=0;for(const auto& a:journal["actions"].items()){
            if(cancel)throw std::runtime_error("Instalação cancelada.");fs::path to=a["path"].text();say(.58+.28*double(n++)/std::max(size_t(1),journal["actions"].items().size()),"Confirmando · "+to.filename().string());
            reject_links(to);if(fs::exists(to)!=a["existed"].boolean()||(fs::exists(to)&&sha256_file(to)!=a["old_hash"].text()))throw std::runtime_error("Arquivo alterado durante a confirmação: "+to.string());
            fs::create_directories(to.parent_path());
            if(!a["temporary"].text().empty()){auto staged=stage/a["staged"].text();struct stat st{};if(stat(staged.c_str(),&st))throw std::runtime_error("Temporário interno ausente");publish_external(to,a["temporary"].text(),read_file(staged,65536),unsigned(st.st_mode&0777));}
            else if(rename((stage/a["staged"].text()).c_str(),to.c_str()))throw std::runtime_error("Falha ao confirmar "+to.string());
            int fd=open(to.parent_path().c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(fd>=0){fsync(fd);close(fd);}
        }
        journal["committed"]=true;save_json(stage/"transacao.json",journal);committed=true;clean_private_stage(stage);
        say(.90,"Recolhendo a origem após conferir a instalação");
        Json::Array leftovers;
        auto leftover=[&](const fs::path& path,const std::string& reason,bool removable=false,const std::string& hash=""){leftovers.emplace_back(Json::Object{{"path",path.string()},{"reason",reason},{"remove_on_uninstall",removable},{"sha256",hash}});result.warnings.push_back(path.string()+" — "+reason);};
        if(!p.inplace){
            std::set<fs::path> copied;for(const auto& c:copies)copied.insert(c.source);
            for(const auto& f:p.files){auto from=o.source/f;if(copied.count(from))continue;fs::path to=o.destination/f;
                if(fs::exists(to)&&sha256_file(from)==sha256_file(to))source_cleanup.emplace_back(from,f);else if(fs::exists(from))leftover(from,"Registro ou preferência anterior diferente; preservado para não perder configuração.");
            }
            for(const auto& c:source_cleanup){try{if(!o.gather_source){leftover(c.first,"Você escolheu preservar a pasta de origem.");continue;}
                reject_links(c.first);if(fs::exists(c.first)&&sha256_file(c.first)==sha256_file(o.destination/c.second))fs::remove(c.first);
                else if(fs::exists(c.first))leftover(c.first,"O arquivo mudou após a cópia; preservado.");
            }catch(const std::exception& e){leftover(c.first,e.what(),true,fs::exists(o.destination/c.second)?sha256_file(o.destination/c.second):"");}}
            for(const auto& f:p.extras)if(fs::exists(fs::symlink_status(o.source/f)))leftover(o.source/f,fs::is_symlink(o.source/f)?"Link adicional: mantido para não mover seu destino.":"Arquivo adicional mantido na origem.");
            std::vector<fs::path> dirs;for(const auto& f:fs::recursive_directory_iterator(o.source))if(fs::is_directory(f.symlink_status()))dirs.push_back(f.path());std::sort(dirs.begin(),dirs.end(),[](const auto& a,const auto& b){return a.native().size()>b.native().size();});for(const auto& d:dirs)rmdir(d.c_str());rmdir(o.source.c_str());
        }
        if(!o.archive.empty()&&fs::exists(o.archive))try{if(sha256_file(o.archive)==sha256_file(o.destination/archive_dest))fs::remove(o.archive);else leftover(o.archive,"O ZIP mudou após a cópia; original preservado.");}catch(const std::exception& e){leftover(o.archive,std::string("ZIP preservado na origem: ")+e.what());}
        record["leftovers"]=leftovers;save_json(o.destination/"instalacao.json",record);
        result.report=p.summary()+"\nResultado: instalação verificada e concluída.\n";
        if(leftovers.empty())result.report+="Nenhum arquivo conhecido da origem ficou pendente fora da pasta.\n";else{result.report+="\nArquivos que permaneceram fora:\n";for(const auto& w:result.warnings)result.report+="• "+w+"\n";}
        result.report+="\n"+external_report(o.destination)+"\nEstado compartilhado: o desktop pode manter caches e o WirePlumber pode salvar volumes em sua pasta de estado. Esses arquivos são de outros componentes e não são apagados.\n";
        atomic_write(o.destination/"RELATORIO-INSTALACAO.txt",result.report);close(lock);say(1,"Instalação concluída");return result;
    }catch(...){
        auto error=std::current_exception();std::string recovery_error;
        try{if(!committed){if(journal_ready)rollback_stage(o.destination,stage);else clean_private_stage(stage);}}
        catch(const std::exception& e){recovery_error=e.what();}close(lock);
        if(!recovery_error.empty())throw std::runtime_error("Instalação interrompida. O registro de recuperação foi preservado em "+stage.string()+". "+recovery_error);
        if(committed)throw std::runtime_error("A instalação foi confirmada, mas o recolhimento da origem não terminou. Abra o aplicativo em "+o.destination.string()+" e consulte o registro de instalação.");std::rethrow_exception(error);
    }
}
std::string external_report(const fs::path& root){
    std::string out="Arquivos externos registrados pelo Áudio RC:\n";bool any=false;auto j=ledger(root);
    for(const auto& e:j["entries"].items()){out+="• "+e["path"].text()+"\n  "+e["kind"].text()+" · "+(fs::exists(e["path"].text())?"presente":"já ausente")+"\n";for(const auto& t:e["temporary_paths"].items())if(fs::exists(t.text()))out+="  Temporário pendente registrado: "+t.text()+"\n";any=true;}
    auto cfg=load_json(root/"preferencias.json",65536);if(cfg["integration"]["prepared"].boolean()){out+="• "+cfg["integration"]["kmixrc"].text()+"\n  Configuração compartilhada do KMix; restaurar somente a chave registrada.\n";any=true;}
    auto installation=load_json(root/"instalacao.json");for(const auto& e:installation["leftovers"].items()){out+="• "+e["path"].text()+"\n  "+e["reason"].text()+"\n";any=true;}
    if(!any)out+="Nenhum arquivo externo próprio registrado.\n";
    return out+"\nZIPs e arquivos pessoais recolhidos serão preservados. Estado compartilhado do servidor de áudio e caches do desktop não pertencem ao app.\n";
}
void remove_recorded_external(const fs::path& root,std::ostream& out){
    auto j=ledger(root);for(const auto& e:j["entries"].items()){fs::path p=e["path"].text();if(!p.is_absolute()||p.filename()!="Audio-Painel-RC-CPP.desktop")throw std::runtime_error("Registro externo inválido");reject_links(p);if(fs::exists(p)&&!entry_hash_matches(p,e))throw std::runtime_error("Atalho editado; remoção interrompida antes de apagar o aplicativo: "+p.string());}
    for(const auto& e:j["entries"].items())out<<"Integração removida ou já ausente: "<<e["path"].text()<<'\n';remove_external_shortcut(root,"");
    auto installation=load_json(root/"instalacao.json");for(const auto& e:installation["leftovers"].items())if(e["remove_on_uninstall"].boolean()){
        fs::path p=e["path"].text();if(zip_file(p))continue;reject_links(p);if(fs::exists(p)){if(e["sha256"].text().empty()||sha256_file(p)!=e["sha256"].text())throw std::runtime_error("Resto da instalação foi editado e foi preservado: "+p.string());fs::remove(p);out<<"Resto conhecido removido: "<<p<<'\n';}
    }
}
std::vector<fs::path> removal_manifest(const fs::path& root){
    std::set<fs::path> files;std::istringstream stream(read_file(root/"ARQUIVOS.txt",262144));std::string line;while(std::getline(stream,line))if(!line.empty())files.insert(line);
    auto record=load_json(root/"instalacao.json");if(!record["app"].null()&&record["app"].text()!=APP_ID)throw std::runtime_error("Registro de remoção inválido");for(const auto& f:record["managed_files"].items())files.insert(f.text());
    for(const auto& path:{"preferencias.json","instalacao.json","integracoes.json","RELATORIO-INSTALACAO.txt","RELATORIO-REMOCAO.txt","RELATORIO-TECLADO.txt"})files.insert(path);
    if(fs::exists(root/"integracao-kmix/kmixrc.antes"))files.insert("integracao-kmix/kmixrc.antes");
    for(const auto& f:record["preserved_files"].items())files.erase(f.text());for(const auto& f:record["archives"].items())files.erase(f.text());
    std::vector<fs::path> out;for(const auto& f:files){if(!safe_relative_path(f))throw std::runtime_error("Registro contém caminho inseguro: "+f.string());reject_links(root/f);if(!zip_file(f))out.push_back(f);}return out;
}
// Identifica o ZIP da mesma versão pelo tamanho e CRC do executável no diretório
// central. Não extrai arquivos nem executa conteúdo; a transferência usa SHA-256.
static uint32_t crc_file(const fs::path& p){std::ifstream f(p,std::ios::binary);uint32_t c=~0u;char b[65536];while(f){f.read(b,sizeof b);for(std::streamsize i=0;i<f.gcount();++i){c^=static_cast<unsigned char>(b[i]);for(int n=0;n<8;++n)c=(c>>1)^(0xedb88320u&uint32_t(-int(c&1)));}}return ~c;}
static uint32_t le(const unsigned char* p,int n){uint32_t v=0;for(int i=0;i<n;++i)v|=uint32_t(p[i])<<(8*i);return v;}
fs::path find_release_archive(const fs::path& root){
    try{uint32_t crc=crc_file(root/"audio-painel-rc");auto size=fs::file_size(root/"audio-painel-rc");fs::path found;
        for(const auto& item:fs::directory_iterator(root.parent_path())){
            if(fs::is_symlink(item)||!fs::is_regular_file(item)||!zip_file(item.path()))continue;std::ifstream f(item.path(),std::ios::binary);auto total=fs::file_size(item.path());if(total<22||total>uintmax_t(0xffffffffu))continue;
            size_t amount=std::min(uintmax_t(65557),total);std::string tail(amount,0);f.seekg(total-amount);f.read(tail.data(),amount);size_t end=tail.rfind(std::string("PK\x05\x06",4));if(end==std::string::npos||end+22>tail.size())continue;
            auto e=reinterpret_cast<const unsigned char*>(tail.data()+end);uint32_t count=le(e+10,2),central_size=le(e+12,4),offset=le(e+16,4);if(central_size>4*1024*1024||uintmax_t(offset)+central_size>total)continue;
            std::string central(central_size,0);f.clear();f.seekg(offset);f.read(central.data(),central.size());bool executable=false,identity=false;size_t pos=0;
            for(uint32_t n=0;n<count&&pos+46<=central.size();++n){auto c=reinterpret_cast<const unsigned char*>(central.data()+pos);if(le(c,4)!=0x02014b50)break;auto len=le(c+28,2),extra=le(c+30,2),comment=le(c+32,2);if(pos+46+len+extra+comment>central.size())break;std::string name=central.substr(pos+46,len);
                if(name=="Audio-Painel-RC-CPP/audio-painel-rc"&&le(c+16,4)==crc&&le(c+24,4)==size)executable=true;if(name=="Audio-Painel-RC-CPP/IDENTIDADE-RC")identity=true;pos+=46+len+extra+comment;
            }
            if(executable&&identity){if(!found.empty())return {};found=item.path();}
        }return found;
    }catch(...){return {};}
}
}

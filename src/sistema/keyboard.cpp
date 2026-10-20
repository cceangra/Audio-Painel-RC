#include "keyboard.hpp"
#include <set>
namespace rc {
// Tipos e assinaturas públicas de Xlib. A biblioteca é opcional e carregada
// somente no assistente de teclas ou com controle direto explicitamente ativo.
// Não há /dev/input, XRecord, polling do teclado ou registro de digitação.
namespace {
struct Display;
using Window=unsigned long;using KeySym=unsigned long;
struct KeyEvent {int type;unsigned long serial;int send_event;Display* display;Window window,root,subwindow;unsigned long time;int x,y,x_root,y_root;unsigned state,keycode;int same_screen;};
union Event {int type;KeyEvent key;long pad[24];};
struct XError {int type;Display* display;unsigned long resourceid,serial;unsigned char error_code,request_code,minor_code;};
struct ModifierMap {int max_keypermod;unsigned char* modifiermap;};
class XApi {
    void* library=nullptr;
    template<class F> F load(const char* n){auto f=dlsym(library,n);if(!f)throw std::runtime_error(std::string("X11 indisponível: ")+n);return reinterpret_cast<F>(f);}
public:
    Display* display=nullptr;Window root=0;unsigned locks=2;
    Display*(*open)(const char*);int(*close)(Display*);Window(*default_root)(Display*);int(*connection)(Display*);
    unsigned char(*keycode)(Display*,KeySym);KeySym(*keysym)(Display*,unsigned char,int,int);
    int(*grab)(Display*,int,unsigned,Window,int,int,int);int(*ungrab)(Display*,int,unsigned,Window);
    int(*sync)(Display*,int);int(*pending)(Display*);int(*next)(Display*,Event*);
    ModifierMap*(*modifiers)(Display*);int(*free_modifiers)(ModifierMap*);
    using Handler=int(*)(Display*,XError*);Handler(*set_handler)(Handler);
    static inline Display* checking=nullptr;static inline int error=0;static inline Handler previous=nullptr;
    static int on_error(Display* d,XError* e){if(d==checking){error=e->error_code;return 0;}return previous?previous(d,e):0;}
    XApi(){
        if(detect_environment(false).wayland)throw std::runtime_error("Wayland: controle direto X11 desativado. Use o modo compatibilidade e os atalhos do desktop.");
        library=dlopen("libX11.so.6",RTLD_NOW|RTLD_LOCAL);if(!library)throw std::runtime_error("Biblioteca X11 não encontrada.");
        try{
            open=load<decltype(open)>("XOpenDisplay");close=load<decltype(close)>("XCloseDisplay");default_root=load<decltype(default_root)>("XDefaultRootWindow");connection=load<decltype(connection)>("XConnectionNumber");
            keycode=load<decltype(keycode)>("XKeysymToKeycode");keysym=load<decltype(keysym)>("XkbKeycodeToKeysym");grab=load<decltype(grab)>("XGrabKey");ungrab=load<decltype(ungrab)>("XUngrabKey");sync=load<decltype(sync)>("XSync");pending=load<decltype(pending)>("XPending");next=load<decltype(next)>("XNextEvent");
            modifiers=load<decltype(modifiers)>("XGetModifierMapping");free_modifiers=load<decltype(free_modifiers)>("XFreeModifiermap");set_handler=load<decltype(set_handler)>("XSetErrorHandler");
            display=open(nullptr);if(!display)throw std::runtime_error("Não foi possível conectar ao X11.");root=default_root(display);
            auto m=modifiers(display);if(m){for(int mod=0;mod<8;++mod)for(int k=0;k<m->max_keypermod;++k){auto c=m->modifiermap[mod*m->max_keypermod+k];if(c&&keysym(display,c,0,0)==0xff7f)locks|=1u<<mod;}free_modifiers(m);}
        }catch(...){if(display)close(display);dlclose(library);library=nullptr;throw;}
    }
    ~XApi(){if(display)close(display);if(library)dlclose(library);}
    std::vector<unsigned> lock_variants()const{std::vector<unsigned> out;for(unsigned v=0;v<256;++v)if((v&~locks)==0)out.push_back(v);return out;}
    bool reserve(const std::vector<KeyBinding>& keys,std::string& reason){
        std::set<std::pair<unsigned,unsigned>> physical;for(const auto& b:keys)if(!physical.insert({keycode(display,b.keyval),b.modifiers&~locks}).second){reason="Duas funções usam a mesma tecla física e modificadores.";return false;}
        sync(display,0);checking=display;error=0;previous=set_handler(on_error);bool missing=false;
        for(const auto& b:keys){auto code=keycode(display,b.keyval);if(!code){missing=true;continue;}for(auto l:lock_variants())grab(display,code,(b.modifiers&~locks)|l,root,0,1,1);}
        sync(display,0);set_handler(previous);checking=nullptr;
        if(error||missing){for(const auto& b:keys)if(auto c=keycode(display,b.keyval))for(auto l:lock_variants())ungrab(display,c,(b.modifiers&~locks)|l,root);sync(display,0);
            reason=missing?"Uma combinação não existe no mapeamento atual.":error==10?"Conflito confirmado: pelo menos uma combinação já está reservada no X11. O responsável não pode ser identificado com segurança.":"O X11 recusou o registro (erro "+std::to_string(error)+").";return false;}
        reason="Registro global verificado no X11. A disponibilidade será verificada novamente ao ativar.";return true;
    }
};
}
std::vector<KeyBinding> key_bindings(const Json& j){
    std::vector<KeyBinding> out{{"mute",0x1008ff12,0,false},{"down",0x1008ff11,0,false},{"up",0x1008ff13,0,false}};
    for(auto& b:out)for(const auto& stored:j["bindings"].items())if(stored["action"].text()==b.action){int val=stored["keyval"].integer();unsigned mods=unsigned(stored["modifiers"].integer())&0xcd;
        if(val>0&&valid_shortcut(unsigned(val),mods)){b.keyval=unsigned(val);b.modifiers=mods;b.confirmed=stored["confirmed"].boolean();}}
    return out;
}
bool valid_shortcut(unsigned key,unsigned mods){
    bool media=key==0x1008ff12||key==0x1008ff11||key==0x1008ff13;
    bool modifier=(key>=0xffe1&&key<=0xffee)||key==0xff7f;
    return key>0&&!modifier&&(media||(mods&(4|8|64))!=0);
}
Json keyboard_json(const std::vector<KeyBinding>& keys,const std::string& mode,int step){
    Json::Array bindings;std::set<std::pair<unsigned,unsigned>> used;
    for(const auto& b:keys){if(!valid_shortcut(b.keyval,b.modifiers)||!used.insert({b.keyval,b.modifiers}).second)throw std::runtime_error("As combinações de volume precisam ser válidas e diferentes.");bindings.emplace_back(Json::Object{{"action",b.action},{"keyval",int(b.keyval)},{"modifiers",int(b.modifiers)},{"confirmed",b.confirmed}});}
    return Json::Object{{"mode",mode=="direct"?"direct":"compatibility"},{"step",std::clamp(step,1,15)},{"bindings",bindings}};
}
KeyProbe probe_shortcuts(const std::vector<KeyBinding>& keys){
    KeyProbe p;try{std::set<std::pair<unsigned,unsigned>> seen;for(const auto& k:keys)if(!valid_shortcut(k.keyval,k.modifiers)||!seen.insert({k.keyval,k.modifiers}).second)throw std::runtime_error("Combinação inválida ou repetida.");
        XApi x;p.supported=true;p.available=x.reserve(keys,p.message);
    }catch(const std::exception& e){p.message=e.what();}return p;
}
struct KeyboardRuntime::Impl {
    LoopApi& loop;XApi x;unsigned watch=0;std::vector<KeyBinding> keys;int step;
    std::function<void(const std::string&,int)> callback;
    Impl(LoopApi& l,const Json& j,std::function<void(const std::string&,int)> cb):loop(l),step(std::clamp(j["step"].integer(5),1,15)),callback(std::move(cb)){
        for(const auto& b:key_bindings(j))if(b.confirmed)keys.push_back(b);if(keys.empty())throw std::runtime_error("Controle direto não ativado: nenhuma tecla foi confirmada no teste.");
        std::string reason;if(!x.reserve(keys,reason))throw std::runtime_error(reason+" O desktop continua responsável pelo volume.");
        watch=loop.g_unix_fd_add(x.connection(x.display),1|8|16,+[](int,int condition,void* data)->int{
            auto* s=static_cast<Impl*>(data);if(condition&(8|16)){s->watch=0;return 0;}
            while(s->x.pending(s->x.display)){Event event{};s->x.next(s->x.display,&event);if(event.type!=2)continue;
                for(const auto& b:s->keys)if(event.key.keycode==s->x.keycode(s->x.display,b.keyval)&&(event.key.state&~s->x.locks&0xff)==(b.modifiers&~s->x.locks)){s->callback(b.action,s->step);break;}}
            return 1;
        },this);
    }
    ~Impl(){loop.cancel(watch);}
};
KeyboardRuntime::KeyboardRuntime(LoopApi& loop,const Json& j,std::function<void(const std::string&,int)> cb,std::function<void(const std::string&)> error){
    if(j["mode"].text()!="direct")return;try{impl_=std::make_unique<Impl>(loop,j,std::move(cb));}catch(const std::exception& e){error(e.what());}
}
KeyboardRuntime::~KeyboardRuntime()=default;
int keyboard_x11_tests(){
    if(env_value("RC_TESTE_X11_ISOLADO")!="1")throw std::runtime_error("Este teste exige uma sessão X11 virtual isolada e RC_TESTE_X11_ISOLADO=1. Não o execute no desktop de uso diário.");
    if(!program_exists("xdotool"))throw std::runtime_error("xdotool necessário somente para o teste X11 isolado.");
    LoopApi api;void* loop=api.g_main_loop_new(nullptr,0);auto keys=key_bindings(Json::Object{});for(size_t n=0;n<3;++n){keys[n].keyval=n==0?'m':n==1?'o':'p';keys[n].modifiers=12;keys[n].confirmed=true;}
    std::vector<std::string> seen;std::string error;int failed=0;
    auto runtime=std::make_unique<KeyboardRuntime>(api,keyboard_json(keys,"direct",7),[&](const std::string& action,int step){seen.push_back(action);if(step!=7)error="passo incorreto";},[&](const std::string& e){error=e;});
    auto collision=probe_shortcuts(keys);if(collision.available||!collision.supported||collision.message.find("Conflito confirmado")==std::string::npos){++failed;std::cerr<<"ERRO  Reserva concorrente não reconhecida\n";}else std::cout<<"OK  X11: conflito confirmado por BadAccess; reserva original preservada\n";
    auto sent=run_read({"xdotool","key","--clearmodifiers","ctrl+alt+m","ctrl+alt+o","ctrl+alt+p","Caps_Lock","ctrl+alt+m","Caps_Lock"},3000,8192);
    api.later([&](){api.g_main_loop_quit(loop);},1000);api.g_main_loop_run(loop);
    if(sent.status||!error.empty()||seen!=std::vector<std::string>{"mute","down","up","mute"}){++failed;std::cerr<<"ERRO  Eventos / passo / Caps Lock: "<<error<<" ("<<seen.size()<<" eventos)\n";}else std::cout<<"OK  X11: quatro eventos globais, passo 7 e Caps Lock\n";
    runtime.reset();auto released=probe_shortcuts(keys);if(!released.available){++failed;std::cerr<<"ERRO  Teclas não foram liberadas: "<<released.message<<'\n';}else std::cout<<"OK  X11: liberação das reservas ao desativar\n";
    api.g_main_loop_unref(loop);return failed?1:0;
}
}

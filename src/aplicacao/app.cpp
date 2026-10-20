#include "ui.hpp"
#include <fcntl.h>
#include <iomanip>
#include <signal.h>
#include <sstream>
#include <sys/inotify.h>
#include <sys/socket.h>
#include <unistd.h>
namespace rc {
App::App(GtkApi& u,bool settings,bool demonstration,bool standalone,int server):server_(server),standalone_(standalone),ui(u),demo(demonstration),settings_mode(settings){
    config=Config::load();int dark=0;char* theme=nullptr;ui.g_object_get(ui.gtk_settings_get_default(),"gtk-application-prefer-dark-theme",&dark,"gtk-theme-name",&theme,nullptr);original_dark_=dark;
    if(theme){std::string name(theme);std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});original_dark_|=name.find("dark")!=std::string::npos;ui.g_free(theme);}
}
App::~App(){
    closing_=true;ui.cancel(server_watch_);ui.cancel(config_watch_);ui.cancel(release_);ui.cancel(signal1_);ui.cancel(signal2_);ui.cancel(tray_check_);ui.cancel(key_resume_);keyboard_.reset();
    for(auto& kv:clients_){ui.g_source_remove(kv.second);close(kv.first);}clients_.clear();
    if(server_>=0)close(server_);if(config_fd_>=0)close(config_fd_);
    close_menu();settings_.reset();popup_.reset();backend_.reset();
    if(tray_){ui.gtk_status_icon_set_visible(tray_,0);ui.g_object_unref(tray_);}
    if(provider_){ui.gtk_style_context_remove_provider_for_screen(ui.gdk_screen_get_default(),provider_);ui.g_object_unref(provider_);}
}
void App::monitor_config(){
    config_fd_=inotify_init1(IN_NONBLOCK|IN_CLOEXEC);if(config_fd_<0)return;
    if(inotify_add_watch(config_fd_,folder().c_str(),IN_CLOSE_WRITE|IN_MOVED_TO)<0){close(config_fd_);config_fd_=-1;return;}
    config_watch_=ui.g_unix_fd_add(config_fd_,1,+[](int fd,int,void* data)->int{
        auto* a=static_cast<App*>(data);alignas(inotify_event) char b[8192];ssize_t n;bool changed=false;
        while((n=read(fd,b,sizeof b))>0)for(size_t p=0;p+sizeof(inotify_event)<=size_t(n);){auto* e=reinterpret_cast<const inotify_event*>(b+p);if(e->len&&std::string(e->name)=="preferencias.json")changed=true;p+=sizeof(inotify_event)+e->len;}
        if(changed)a->reload();return 1;
    },this);
}
int App::run(bool show){
    backend_=std::make_unique<Backend>(ui,demo,[this](){refresh();},[this](const std::string& e){message(e);});
    style();monitor_config();if(!settings_mode&&!demo)configure_keyboard();
    ui.connect(ui.gdk_screen_get_default(),"composited-changed",+[](void*,void* data){auto* a=static_cast<App*>(data);try{a->style();a->refresh();}catch(const std::exception& e){a->message(e.what());}},this);
    if(server_>=0)server_watch_=ui.g_unix_fd_add(server_,1,+[](int,int,void* data)->int{static_cast<App*>(data)->serve();return 1;},this);
    signal1_=ui.g_unix_signal_add(SIGTERM,+[](void* data)->int{static_cast<App*>(data)->signal1_=0;static_cast<App*>(data)->quit();return 0;},this);
    signal2_=ui.g_unix_signal_add(SIGINT,+[](void* data)->int{static_cast<App*>(data)->signal2_=0;static_cast<App*>(data)->quit();return 0;},this);
    if(settings_mode)settings_=std::make_unique<SettingsView>(*this);
    else {
        tray_=ui.gtk_status_icon_new_from_file((folder()/"icones/tray-high.svg").c_str());
        ui.gtk_status_icon_set_name(tray_,"audio-painel-rc-cpp");ui.gtk_status_icon_set_title(tray_,"Áudio RC");ui.gtk_status_icon_set_visible(tray_,1);
        ui.connect(tray_,"activate",+[](void*,void* data){static_cast<App*>(data)->show_popup(true);},this);
        ui.connect(tray_,"popup-menu",+[](void*,unsigned button,unsigned time,void* data){
            auto* a=static_cast<App*>(data);a->close_menu();void* menu=a->ui.gtk_menu_new();a->ui.cls(menu,"rc-menu");
            auto item=[&](const char* title,std::function<void()> fn){void* w=a->ui.gtk_menu_item_new_with_label(title);a->ui.gtk_menu_shell_append(menu,w);auto* cb=new std::function<void()>(std::move(fn));a->ui.g_signal_connect_data(w,"activate",reinterpret_cast<void(*)()>(+[](void*,void* p){(*static_cast<std::function<void()>*>(p))();}),cb,+[](void* p,void*){delete static_cast<std::function<void()>*>(p);},0);};
            item("Abrir painel",[a](){a->show_popup();});item("Configurações",[a](){a->open_settings();});item("Sair do Áudio RC",[a](){a->quit();});a->show_menu(menu,button,time,true);
        },this);
        ui.connect(tray_,"scroll-event",+[](void*,void* event,void* data)->int{
            auto* a=static_cast<App*>(data);if(!a->state().sink)return 1;int dir=0;double x=0,y=0;double delta=0;
            if(a->ui.gdk_event_get_scroll_direction(event,&dir)&&dir<4)delta=dir==1||dir==3?-5:5;
            else if(a->ui.gdk_event_get_scroll_deltas(event,&x,&y))delta=y>0?-5:5;
            if(delta){Action act;act.kind=Action::Volume;act.refs={a->state().sink->ref};act.value=a->state().sink->volume*100+delta;a->act(act);}return 1;
        },this);
        if(show)show_popup();
        if(!demo)tray_check_=ui.later([this](){tray_check_=0;if(tray_&&!ui.gtk_status_icon_is_embedded(tray_)){free_window_=true;close_menu();popup_.reset();show_popup();message("O desktop não disponibilizou uma bandeja compatível. O painel fica aberto como uma janela.");}},1600);
    }
    backend_->start();ui.gtk_main();return 0;
}
void App::act(Action a){last_error_.clear();backend_->submit(std::move(a));}
void App::message(const std::string& text){last_error_=text.substr(0,2048);if(!last_error_.empty())std::cerr<<last_error_<<'\n';refresh();}
void App::refresh(){
    if(closing_)return;
    const State& s=state();if(tray_){
        std::string icon=!s.sink||s.sink->muted?"muted":s.sink->volume<.34?"low":s.sink->volume<.67?"medium":"high";
        if(icon!=last_icon_){ui.gtk_status_icon_set_from_file(tray_,(folder()/"icones"/("tray-"+icon+".svg")).c_str());last_icon_=icon;}
        std::string tip="Áudio RC · ";tip+=s.sink?s.sink->label+" · "+std::to_string(int(std::lround(s.sink->volume*100)))+"%":s.error.empty()?"Sem saída padrão":s.error;
        if(s.sink&&s.sink->muted)tip+=" · silenciado";if(demo)tip+=" · demonstração";
        if(tip!=last_tip_){ui.gtk_status_icon_set_tooltip_text(tray_,tip.c_str());last_tip_=tip;}
    }
    if(popup_&&popup_->visible())popup_->update(s);if(settings_)settings_->update(s);
}
void App::show_popup(bool toggle){
    if(closing_)return;ui.cancel(release_);if(!popup_)popup_=std::make_unique<Popup>(*this);
    if(toggle&&popup_->visible())popup_->hide();else popup_->show();
}
void App::hidden(){
    ui.cancel(release_);if(config.release_seconds>0)release_=ui.later([this](){release_=0;if(popup_&&!popup_->visible())popup_.reset();},unsigned(config.release_seconds*1000));
}
void App::open_settings(){
    if(popup_&&!free_window_)popup_->hide();try{ipc(demo?"demo-settings":"settings","show");}
    catch(...){try{spawn_settings(demo);}catch(const std::exception& e){message(e.what());}}
}
void App::quit(){if(closing_)return;closing_=true;ui.gtk_main_quit();}
void App::set(const std::string& key,const Json& value){
    if(demo&&key=="autostart_path"){message("Inicialização não é alterada na demonstração.");return;}
    try{preference(key,value);reload();}catch(const std::exception& e){message(e.what());}
}
void App::reload(){
    if(closing_)return;try{
        Config next=Config::load();if(next.json()==config.json())return;bool was=popup_&&popup_->visible();
        config=std::move(next);close_menu();popup_.reset();style();if(!settings_mode&&!demo&&keyboard_config_!=config.original["keyboard"])configure_keyboard();if(was)show_popup();refresh();
    }catch(const std::exception& e){message("Preferências: "+std::string(e.what()));}
}
void App::show_menu(void* w,unsigned button,unsigned time,bool by_tray){
    close_menu();ui.g_object_ref_sink(w);menu=w;ui.gtk_widget_show_all(w);
    ui.connect(w,"deactivate",+[](void* w,void* data){auto* a=static_cast<App*>(data);if(a->menu==w)a->menu=nullptr;
        a->ui.later([a,w](){a->ui.gtk_widget_destroy(w);a->ui.g_object_unref(w);});
    },this);
    if(!time)time=ui.gtk_get_current_event_time();ui.gtk_menu_popup(w,nullptr,nullptr,by_tray&&tray_?ui.gtk_status_icon_position_menu:nullptr,by_tray?tray_:nullptr,button,time);
}
void App::close_menu(){if(menu){void* w=menu;menu=nullptr;ui.gtk_menu_popdown(w);/* deactivate agenda a destruição */}}
bool App::any_combo_popup()const{
    for(auto* s:selectors){int open=0;ui.g_object_get(s->widget,"popup-shown",&open,nullptr);if(open)return true;}return false;
}
bool App::dragging()const{return ui.gtk_grab_get_current()!=nullptr;}
void App::serve(){
    int fd;while((fd=accept4(server_,nullptr,nullptr,SOCK_NONBLOCK|SOCK_CLOEXEC))>=0){
        ucred peer{};socklen_t n=sizeof peer;if(getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&peer,&n)||peer.uid!=getuid()||clients_.size()>=8){close(fd);continue;}
        messages_[fd]="";clients_[fd]=ui.g_unix_fd_add(fd,1|8|16,+[](int fd,int,void* data)->int{
            auto* a=static_cast<App*>(data);bool keep=a->receive(fd);if(!keep){a->clients_.erase(fd);a->messages_.erase(fd);close(fd);}return keep;
        },this);
    }
}
bool App::receive(int fd){
    auto& data=messages_[fd];char b[1024];ssize_t n;while((n=recv(fd,b,sizeof b,0))>0){data.append(b,size_t(n));if(data.size()>8192)return false;}
    if(data.find('\n')==std::string::npos)return n!=0;
    try{
        auto request=Json::parse(data);auto action=request["action"].text();
        if(action=="show"){if(settings_)settings_->present();else show_popup();}
        else if(action=="toggle")show_popup(true);
        else if(action=="reload")reload();
        else if(action=="quit")ui.later([this](){quit();});
        else if(action=="settings")open_settings();
        else if(action=="keys-pause"){keys_paused_=true;keyboard_.reset();ui.cancel(key_resume_);key_resume_=ui.later([this](){key_resume_=0;keys_paused_=false;configure_keyboard();},180000);}
        else if(action=="keys-resume"){ui.cancel(key_resume_);keys_paused_=false;configure_keyboard();}
        Json response=Json::Object{{"pid",int(getpid())},{"settings",settings_mode},{"demo",demo},{"popup_visible",bool(popup_&&popup_->visible())},{"embedded",bool(tray_&&ui.gtk_status_icon_is_embedded(tray_))},{"sink",state().sink.has_value()},{"error",state().error},{"version",VERSION}};
        auto text=response.dump()+"\n";send(fd,text.data(),text.size(),MSG_NOSIGNAL);
    }catch(const std::exception&){}return false;
}
void App::close_client(int fd){auto i=clients_.find(fd);if(i!=clients_.end()){ui.g_source_remove(i->second);clients_.erase(i);}messages_.erase(fd);close(fd);}
void App::configure_keyboard(){
    keyboard_.reset();keyboard_config_=config.original["keyboard"];if(keys_paused_)return;
    struct Target{Ref ref;double volume=0;std::chrono::steady_clock::time_point time;};auto target=std::make_shared<Target>();
    keyboard_=std::make_unique<KeyboardRuntime>(ui,keyboard_config_,[this,target](const std::string& action,int step){
        if(!state().sink)return;const auto& sink=*state().sink;Action a;a.refs={sink.ref};
        if(action=="mute"){a.kind=Action::Mute;a.muted=!sink.muted;}
        else{auto now=std::chrono::steady_clock::now();if(target->ref!=sink.ref||now-target->time>std::chrono::milliseconds(400))target->volume=sink.volume*100;
            target->ref=sink.ref;target->time=now;target->volume=std::clamp(target->volume+(action=="up"?step:-step),0.,100.);a.kind=Action::Volume;a.value=target->volume;}
        act(a);
    },[this](const std::string& error){message(error);});
}
}

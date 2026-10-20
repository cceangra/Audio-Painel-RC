#include "ui.hpp"
namespace rc {
static std::vector<Node> one(const std::optional<Node>& n){return n?std::vector<Node>{*n}:std::vector<Node>{};}
Popup::Popup(App& app):app_(app){
    auto& u=app.ui;window=u.gtk_window_new(0);u.gtk_window_set_title(window,"Áudio RC");u.gtk_window_set_wmclass(window,"audio-rc-popup","AudioRcCpp");
    u.gtk_window_set_decorated(window,0);u.gtk_window_set_resizable(window,0);u.gtk_window_set_skip_taskbar_hint(window,1);u.gtk_window_set_skip_pager_hint(window,1);u.gtk_window_set_keep_above(window,1);u.gtk_window_set_type_hint(window,5);
    if(app.free_window()){u.gtk_window_set_decorated(window,1);u.gtk_window_set_skip_taskbar_hint(window,0);u.gtk_window_set_skip_pager_hint(window,0);u.gtk_window_set_keep_above(window,0);u.gtk_window_set_type_hint(window,0);u.gtk_window_set_position(window,1);}
    u.cls(window,"rc-root");u.cls(window,"rc-popup");
    if(void* visual=u.gdk_screen_get_rgba_visual(u.gtk_widget_get_screen(window)))u.gtk_widget_set_visual(window,visual);
    // O GTK desenha o fundo RGBA pelo CSS. Limpar manualmente o Cairo aqui
    // apaga regiões que o GTK pode estar reutilizando durante menus/popups.
    u.gtk_widget_set_app_paintable(window,0);
    u.connect(window,"delete-event",+[](void*,void*,void* p)->int{static_cast<Popup*>(p)->hide();return 1;},this);
    u.connect(window,"key-press-event",+[](void*,void* e,void* p)->int{auto* self=static_cast<Popup*>(p);unsigned key=0;self->app_.ui.gdk_event_get_keyval(e,&key);if(key==0xff1b){self->hide();return 1;}return 0;},this);
    u.connect(window,"size-allocate",+[](void*,void*,void* p){auto* self=static_cast<Popup*>(p);if(self->visible()&&!self->animation_)self->position();},this);
    build();
}
Popup::~Popup(){
    auto& u=app_.ui;u.cancel(pointer_);u.cancel(animation_);apps_.reset();output_.reset();input_.reset();master_.reset();mic_.reset();u.gtk_widget_destroy(window);
}
void Popup::build(){
    auto& a=app_;auto& u=a.ui;const auto& c=a.config;
    void* root=a.box(true,c.density=="compact"?10:14);u.cls(root,"rc-surface");u.gtk_container_add(window,root);
    void* header=a.box(false,8);a.pack(root,header);void* branding=a.box(true,0);a.pack(branding,a.label("Áudio RC","title"));
    if(a.demo)a.pack(branding,a.label("DEMONSTRAÇÃO · áudio simulado","dim"));
    a.pack(header,branding,true);a.pack(header,a.button("emblem-system-symbolic","Abrir configurações",[this](){app_.open_settings();},"flat"));
    a.pack(header,a.button("window-close-symbolic","Ocultar painel",[this](){hide();},"flat"));
    master_=std::make_unique<VolumeControl>(a,"MASTER");mic_=std::make_unique<VolumeControl>(a,"MICROFONE",true);
    output_=std::make_unique<Selector>(a,[this](const Choice& c){Action action;action.kind=Action::Choose;action.choice=c;app_.act(action);});
    input_=std::make_unique<Selector>(a,[this](const Choice& c){Action action;action.kind=Action::Choose;action.choice=c;action.port_only=app_.state().filtered;app_.act(action);});
    apps_=std::make_unique<AppsView>(a);
    void* controls=c.layout==3?nullptr:a.box(c.layout!=4,12);if(controls)u.cls(controls,"card");
    if(c.layout!=3){a.pack(controls,master_->widget,true);
        if(c.layout==4){void* sep=u.gtk_separator_new(1);a.pack(controls,sep);}
        a.pack(controls,mic_->widget,true);
    }
    void* mic_notice=a.box(false,6);mic_note_=a.label("","dim");a.pack(mic_notice,mic_note_,true);
    enable_=a.text_button("Ativar entrada",[this](){if(app_.state().physical){Action action;action.kind=Action::Mute;action.refs={app_.state().physical->ref};action.muted=false;app_.act(action);}},"flat");
    u.gtk_widget_set_no_show_all(enable_,1);a.pack(mic_notice,enable_);
    void* devices=c.layout==3?nullptr:a.box(c.layout!=4,12);
    void* out=a.box(true,6);a.pack(out,a.label("SAÍDA DO SOM","eyebrow"));a.pack(out,output_->widget);
    void* in=a.box(true,6);a.pack(in,a.label("FONTE DO MICROFONE","eyebrow"));a.pack(in,input_->widget);
    if(c.layout==4){u.gtk_widget_set_size_request(out,(c.width-46)/2,-1);u.gtk_widget_set_size_request(in,(c.width-46)/2,-1);}
    if(c.layout!=3){a.pack(devices,out,true);a.pack(devices,in,true);}
    void* appbox=a.box(true,8);a.section(appbox,"Aplicativos");
    Rect area;void* display=u.gdk_display_get_default();void* monitor=u.gdk_display_get_primary_monitor(display);if(!monitor)monitor=u.gdk_display_get_monitor(display,0);u.gdk_monitor_get_workarea(monitor,&area);
    int rowheight=c.density=="compact"?45:49;
    int reserve=(c.layout==1?570:c.layout==3?260:450)+(c.font-12)*8;
    void* scroller=a.scroll(apps_->widget,std::clamp(c.rows*rowheight+2,60,std::max(60,area.height-reserve)));a.pack(appbox,scroller);
    if(c.layout==3){
        void* tabs=u.gtk_notebook_new();a.pack(root,tabs);void* general=a.box(true,12);u.gtk_container_set_border_width(general,10);
        a.pack(general,master_->widget);a.pack(general,out);
        u.gtk_notebook_append_page(tabs,general,a.label("Geral"));
        u.gtk_notebook_append_page(tabs,appbox,a.label("Aplicativos"));
        void* micpage=a.box(true,12);u.gtk_container_set_border_width(micpage,10);
        a.pack(micpage,mic_->widget);a.pack(micpage,in);a.pack(micpage,mic_notice);
        a.pack(micpage,a.text_button("Configurações do microfone",[this](){app_.open_settings();}));
        u.gtk_notebook_append_page(tabs,micpage,a.label("Microfone"));
        u.connect(tabs,"switch-page",+[](void*,void*,unsigned,void* p){auto* self=static_cast<Popup*>(p);self->app_.ui.gtk_widget_queue_resize(self->window);},this);
    }else{
        a.pack(root,controls);a.pack(root,mic_notice);
        if(c.layout==1){void* expander=u.gtk_expander_new("Mixer de aplicativos");u.gtk_container_add(expander,appbox);a.pack(root,expander);}
        else a.pack(root,appbox);
        a.section(root,"Dispositivos");a.pack(root,devices);
    }
    error_=a.label("","error",true);u.gtk_label_set_lines(error_,3);u.gtk_label_set_ellipsize(error_,3);u.gtk_widget_set_no_show_all(error_,1);a.pack(root,error_);
    u.gtk_window_set_default_size(window,c.layout==4?c.width:c.layout==1?350:390,-1);
    u.gtk_widget_show_all(root);update(a.state());
}
bool Popup::visible()const{return app_.ui.gtk_widget_get_visible(window);}
void Popup::update(const State& s){
    master_->update(one(s.sink));mic_->update(one(s.source));output_->update(s.outputs,s.sink?s.sink->ref.name:"");
    std::vector<Choice> inputs;std::string target=s.source?s.source->ref.name:"";
    if(s.filtered){
        target=s.physical?s.physical->ref.name:"";
        for(auto c:s.inputs)if(c.node.name==target){c.label="Filtro de ruído · "+c.label;inputs.push_back(std::move(c));}
    }else inputs=s.inputs;
    input_->update(std::move(inputs),target);apps_->update(s);
    auto& u=app_.ui;
    std::string note=s.filtered?"Filtro de ruído ativo":s.source?"Microfone conectado":"Sem fonte padrão de microfone";
    if(s.filtered&&s.physical&&s.physical->muted)note="Entrada física silenciada · filtro ativo";
    if(s.filtered&&!s.physical)note="Filtro ativo · origem física não identificada";
    u.gtk_label_set_text(mic_note_,note.c_str());u.gtk_widget_set_visible(enable_,s.filtered&&s.physical&&s.physical->muted);
    std::string error=app_.error().empty()?s.error:app_.error();u.gtk_label_set_text(error_,error.c_str());u.gtk_widget_set_visible(error_,!error.empty());
}
void Popup::position(){
    if(app_.free_window())return;
    auto& u=app_.ui;void* screen=nullptr;icon_={};orientation_=0;have_icon_=app_.tray()&&u.gtk_status_icon_is_embedded(app_.tray())&&u.gtk_status_icon_get_geometry(app_.tray(),&screen,&icon_,&orientation_);
    void* display=u.gdk_display_get_default();void* monitor=have_icon_?u.gdk_display_get_monitor_at_point(display,icon_.x+icon_.width/2,icon_.y+icon_.height/2):u.gdk_display_get_primary_monitor(display);
    if(!monitor)monitor=u.gdk_display_get_monitor(display,0);u.gdk_monitor_get_workarea(monitor,&area_);
    int w=0,h=0;u.gtk_window_get_size(window,&w,&h);panel_=anchor(area_,icon_,have_icon_,orientation_,w,h,app_.config.gap);u.gtk_window_move(window,panel_.x,panel_.y);
}
void Popup::show(){
    auto& u=app_.ui;u.cancel(animation_);outside_pending_=false;opened_=std::chrono::steady_clock::now();
    update(app_.state());u.gtk_widget_show(window);u.gtk_window_present(window);position();begin_pointer();
    if(app_.config.animation&&!app_.free_window()){
        const auto start=std::chrono::steady_clock::now();const Rect target=panel_;
        animation_=u.g_timeout_add_full(0,16,+[](void* data)->int{
            auto* args=static_cast<std::pair<Popup*,std::pair<std::chrono::steady_clock::time_point,Rect>>*>(data);auto* p=args->first;
            double t=std::min(1.0,std::chrono::duration<double>(std::chrono::steady_clock::now()-args->second.first).count()/0.12);
            auto r=args->second.second;p->app_.ui.gtk_window_move(p->window,r.x,r.y+int(5*std::pow(1-t,3)));
            if(t>=1){p->animation_=0;return 0;}return 1;
        },new std::pair<Popup*,std::pair<std::chrono::steady_clock::time_point,Rect>>(this,{start,target}),+[](void* d){delete static_cast<std::pair<Popup*,std::pair<std::chrono::steady_clock::time_point,Rect>>*>(d);});
    }
}
void Popup::hide(){if(app_.free_window()){app_.quit();return;}app_.ui.cancel(pointer_);app_.ui.cancel(animation_);app_.close_menu();app_.ui.gtk_widget_hide(window);app_.hidden();}
void Popup::begin_pointer(){
    auto& u=app_.ui;u.cancel(pointer_);if(!app_.config.hide_leave||app_.free_window()||env_value("XDG_SESSION_TYPE")=="wayland"||!env_value("WAYLAND_DISPLAY").empty())return;
    pointer_=u.g_timeout_add(80,+[](void* p)->int{auto* self=static_cast<Popup*>(p);return self->watch_pointer();},this);
}
bool Popup::watch_pointer(){
    if(!visible()){pointer_=0;return false;}
    auto& u=app_.ui;auto now=std::chrono::steady_clock::now();
    if(std::chrono::duration_cast<std::chrono::milliseconds>(now-opened_).count()<800||app_.menu||app_.any_combo_popup()||app_.dragging()){
        outside_pending_=false;return true;
    }
    void* seat=u.gdk_display_get_default_seat(u.gdk_display_get_default());void* mouse=seat?u.gdk_seat_get_pointer(seat):nullptr;if(!mouse)return true;
    Point point;u.gdk_device_get_position(mouse,nullptr,&point.x,&point.y);Rect panel=panel_;
    if(void* native=u.gtk_widget_get_window(window))u.gdk_window_get_origin(native,&panel.x,&panel.y);
    u.gtk_window_get_size(window,&panel.width,&panel.height);
    if(pointer_safe(panel,icon_,point,app_.config.margin,have_icon_)){outside_pending_=false;return true;}
    if(!outside_pending_){outside_pending_=true;outside_=now;return true;}
    if(std::chrono::duration_cast<std::chrono::milliseconds>(now-outside_).count()>=app_.config.hide_delay){pointer_=0;hide();return false;}return true;
}
}

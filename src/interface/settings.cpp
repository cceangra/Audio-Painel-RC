#include "ui.hpp"
#include "installation.hpp"
#include <unistd.h>
namespace rc {
static std::vector<Node> one(const std::optional<Node>& n){return n?std::vector<Node>{*n}:std::vector<Node>{};}
static void watch(GtkApi& ui,void* w,const char* signal,std::function<void(void*)> cb){
    auto* f=new std::function<void(void*)>(std::move(cb));
    ui.g_signal_connect_data(w,signal,reinterpret_cast<void(*)()>(+[](void* sender,void* data){try{(*static_cast<std::function<void(void*)>*>(data))(sender);}catch(const std::exception& e){std::cerr<<e.what()<<'\n';}}),f,+[](void* data,void*){delete static_cast<std::function<void(void*)>*>(data);},0);
}
SettingsView::SettingsView(App& a):app_(a){
    auto& u=a.ui;window=u.gtk_window_new(0);u.gtk_window_set_title(window,"Áudio RC — Configurações");u.gtk_window_set_wmclass(window,"audio-rc-settings","AudioRcSettings");
    u.gtk_window_set_default_size(window,760,570);u.gtk_window_set_position(window,1);u.gtk_window_set_icon_name(window,"audio-volume-high");u.cls(window,"rc-root");u.cls(window,"rc-settings");
    u.connect(window,"delete-event",+[](void*,void*,void* data)->int{auto* s=static_cast<SettingsView*>(data);s->stop_voice();s->app_.quit();return 1;},this);
    u.connect(window,"unmap-event",+[](void*,void*,void* data)->int{static_cast<SettingsView*>(data)->stop_voice();return 0;},this);
    auto mapped=+[](void*,void*,void* data)->int{auto* s=static_cast<SettingsView*>(data);s->update(s->app_.state());return 0;};
    u.connect(window,"map-event",mapped,this);u.connect(window,"window-state-event",mapped,this);
    void* root=a.box(true,16);u.gtk_container_set_border_width(root,20);u.gtk_container_add(window,root);
    void* header=a.box(false,10);a.pack(root,header);a.pack(header,a.label("Configurações de áudio","title"),true);
    a.pack(header,a.label(a.demo?"DEMONSTRAÇÃO":"ÁUDIO RC","eyebrow"));
    void* body=a.box(false,20);a.pack(root,body,true);
    stack_=u.gtk_stack_new();u.gtk_stack_set_transition_type(stack_,0);void* sidebar=u.gtk_stack_sidebar_new();u.gtk_stack_sidebar_set_stack(sidebar,stack_);u.gtk_widget_set_size_request(sidebar,140,-1);a.pack(body,sidebar);a.pack(body,stack_,true);
    for(const auto& p:std::vector<std::pair<std::string,std::string>>{{"devices","Dispositivos"},{"mic","Microfone"},{"apps","Aplicativos"},{"appearance","Aparência"},{"keyboard","Teclado e Fn"},{"diagnostics","Diagnósticos"},{"about","Sobre / Instalação"}}){
        void* page=a.box();pages_[p.first]=page;u.gtk_stack_add_titled(stack_,page,p.first.c_str(),p.second.c_str());
    }
    u.connect(stack_,"notify::visible-child-name",+[](void*,void*,void* data){static_cast<SettingsView*>(data)->changed_page();},this);
    error_=a.label("","error",true);u.gtk_label_set_lines(error_,3);u.gtk_label_set_ellipsize(error_,3);u.gtk_widget_set_no_show_all(error_,1);a.pack(root,error_);
    u.gtk_widget_show_all(window);changed_page();
}
SettingsView::~SettingsView(){
    app_.ui.cancel(opacity_save_);stop_voice();voice_.reset();diagnosis_.reset();if(integration_)integration_->detach();integration_.reset();apps_.reset();source_.reset();output_.reset();input_.reset();master_.reset();mic_.reset();physical_.reset();preferences_.clear();app_.ui.gtk_widget_destroy(window);
}
void SettingsView::present(){app_.ui.gtk_window_present(window);update(app_.state());}
void SettingsView::changed_page(){
    const char* name=app_.ui.gtk_stack_get_visible_child_name(stack_);if(!name)return;std::string key=name;
    if(key!="mic")stop_voice();
    if(!built_.count(key)){
        built_.insert(key);void* content=app_.box(true,14);app_.ui.gtk_container_set_border_width(content,4);build_page(key,content);
        void* scroll=app_.scroll(content,470);app_.ui.gtk_widget_set_vexpand(scroll,1);app_.pack(pages_.at(key),scroll,true);app_.ui.gtk_widget_show_all(pages_.at(key));
    }
    update(app_.state());
}
void SettingsView::build_page(const std::string& name,void* content){
    auto& a=app_;auto& u=a.ui;
    if(name=="devices"){
        a.pack(content,a.label("Reprodução","title"));master_=std::make_unique<VolumeControl>(a,"VOLUME GERAL");a.pack(content,master_->widget);
        a.pack(content,a.label("Saída de som","section"));output_=std::make_unique<Selector>(a,[this](const Choice& c){stop_voice();Action action;action.kind=Action::Choose;action.choice=c;app_.act(action);});a.pack(content,output_->widget);
        a.pack(content,a.label("As portas P2 e HDMI são as informadas pelo PipeWire. A automação existente pode restabelecer a saída após uma escolha manual.","dim",true));
        info_=a.label("","dim",true);u.gtk_label_set_selectable(info_,1);a.pack(content,info_);
    }else if(name=="mic"){
        a.pack(content,a.label("Microfone","title"));mic_=std::make_unique<VolumeControl>(a,"FONTE PADRÃO",true);a.pack(content,mic_->widget);
        a.pack(content,a.label("Fonte padrão do sistema","section"));source_=std::make_unique<Selector>(a,[this](const Choice& c){stop_voice();Action action;action.kind=Action::Choose;action.choice=c;app_.act(action);});a.pack(content,source_->widget);
        a.pack(content,a.label("Entrada / porta física","section"));input_=std::make_unique<Selector>(a,[this](const Choice& c){stop_voice();Action action;action.kind=Action::Choose;action.choice=c;action.port_only=app_.state().filtered;app_.act(action);});a.pack(content,input_->widget);
        mic_note_=a.label("","dim",true);a.pack(content,mic_note_);
        enable_=a.text_button("Ativar entrada física",[this](){if(app_.state().physical){Action action;action.kind=Action::Mute;action.refs={app_.state().physical->ref};action.muted=false;app_.act(action);}},"primary");u.gtk_widget_set_no_show_all(enable_,1);a.pack(content,enable_);
        a.section(content,"Teste de voz · retorno de 1 segundo");
        a.pack(content,a.label("Fale e ouça sua voz com atraso para conferir o microfone e o fone. Use fones e comece com volume baixo para evitar apito nos alto-falantes.","dim",true));
        void* line=a.box(false,8);a.pack(content,line);
        voice_start_=a.text_button("Iniciar teste",[this](){start_voice();},"primary");voice_stop_=a.text_button("Parar",[this](){stop_voice();});a.pack(line,voice_start_);a.pack(line,voice_stop_);
        voice_meter_=u.gtk_drawing_area_new();u.gtk_widget_set_size_request(voice_meter_,-1,5);a.pack(content,voice_meter_);
        u.connect(voice_meter_,"draw",+[](void* widget,void* cr,void* data)->int{
            auto* s=static_cast<SettingsView*>(data);auto& u=s->app_.ui;double width=u.gtk_widget_get_allocated_width(widget),height=u.gtk_widget_get_allocated_height(widget);
            u.cairo_set_source_rgba(cr,.19,.21,.26,1);u.cairo_rectangle(cr,0,0,width,height);u.cairo_fill(cr);
            double level=s->voice_&&s->voice_->running?std::min(1.0,s->voice_->peak()*2):0;
            const auto& hex=s->app_.config.accent;u.cairo_set_source_rgba(cr,std::stoi(hex.substr(1,2),nullptr,16)/255.,std::stoi(hex.substr(3,2),nullptr,16)/255.,std::stoi(hex.substr(5,2),nullptr,16)/255.,1);
            u.cairo_rectangle(cr,0,0,width*level,height);u.cairo_fill(cr);return 0;
        },this);
        voice_status_=a.label("Parado · nenhuma gravação é salva.","dim",true);a.pack(content,voice_status_);
        void* advanced=u.gtk_expander_new("Ganho da entrada física · antes do filtro");void* physicalbox=a.box(true,6);u.gtk_container_set_border_width(physicalbox,8);u.gtk_container_add(advanced,physicalbox);a.pack(content,advanced);
        physical_=std::make_unique<VolumeControl>(a,"GANHO DA ENTRADA FÍSICA",true);a.pack(physicalbox,physical_->widget);
        a.pack(physicalbox,a.label("Este ganho atua antes da redução de ruído. O mudo da entrada física é independente do mudo do filtro.","dim",true));
        update_voice();
    }else if(name=="apps"){
        a.pack(content,a.label("Mixer de aplicativos","title"));a.pack(content,a.label("Fluxos do mesmo aplicativo e processo são controlados juntos. O menu ⋮ também permite acompanhar a saída padrão.","dim",true));
        apps_=std::make_unique<AppsView>(a,true);a.pack(content,apps_->widget);
    }else if(name=="appearance")appearance(content);
    else if(name=="keyboard"){
        a.pack(content,a.label("Teclado e teclas Fn","title"));
        auto keyboard=a.config.original["keyboard"];std::string mode=keyboard["mode"].text()=="direct"?"Controle direto solicitado no X11":"Compatibilidade: o desktop controla as teclas";
        a.pack(content,a.label(mode,"section",true));
        std::string text;for(const auto& key:key_bindings(keyboard)){char* label=u.gtk_accelerator_get_label(key.keyval,key.modifiers);text+=(key.action=="mute"?"Silenciar":key.action=="down"?"Diminuir":"Aumentar")+std::string(": ")+(label?label:"")+(key.confirmed?" · confirmada":" · pendente")+"\n";if(label)u.g_free(label);}
        a.pack(content,a.label(text,nullptr,true));a.pack(content,a.label("Variação: "+std::to_string(std::clamp(keyboard["step"].integer(5),1,15))+"% por toque","dim"));
        a.pack(content,a.text_button("Refazer configuração das teclas",[this](){launch_detached(folder()/"audio-painel-rc",app_.demo?std::vector<std::string>{"--teclado","--demo"}:std::vector<std::string>{"--teclado"});},"primary"));
        a.pack(content,a.label("O assistente confirma eventos reais e verifica conflitos no X11. Somente as combinações aprovadas são reservadas; nenhum serviço adicional acompanha o teclado. Ao desativar o controle direto, as reservas são liberadas.","dim",true));
        a.pack(content,a.label("Wayland usa o modo compatibilidade nesta versão. O registro global pelo portal e a transferência automática de atalhos do desktop ainda não estão implementados. O botão de atalhos abre a ferramenta específica do ambiente quando reconhecida.","dim",true));
    }
    else if(name=="diagnostics"){
        a.pack(content,a.label("Estado do áudio","title"));diagnostics_=a.label("",nullptr,true);u.gtk_label_set_selectable(diagnostics_,1);a.pack(content,diagnostics_);
        void* buttons=a.box(false,8);a.pack(content,buttons);
        a.pack(buttons,a.text_button("Atualizar diagnóstico",[this](){run_diagnosis();}));
        a.pack(buttons,a.text_button("Copiar",[this](){auto text=diagnosis_text_.empty()?diagnostic_text():diagnosis_text_;void* clip=app_.ui.gtk_clipboard_get(app_.ui.gdk_atom_intern_static_string("CLIPBOARD"));app_.ui.gtk_clipboard_set_text(clip,text.c_str(),int(text.size()));}));
    }else if(name=="about"){
        a.pack(content,a.label("Áudio Painel RC","title"));a.pack(content,a.label(std::string("C++ · versão ")+VERSION,"dim"));
        a.pack(content,a.label("Controle do áudio existente no Linux. PipeWire e WirePlumber cuidam dos dispositivos e rotas; o painel mantém suas preferências e registros na própria pasta. Compatibilidade varia com o desktop.",nullptr,true));
        a.pack(content,a.label("A central tem processo independente e pode ser minimizada. Fechá-la mantém a bandeja. A substituição do KMix é opcional e só ocorre pelo comando explícito em Aparência.","dim",true));
        a.pack(content,a.label("O teste de voz usa um buffer de um segundo em RAM e funciona apenas enquanto você o inicia. Ele para ao sair da página, minimizar, fechar a central ou completar 30 segundos.","dim",true));
        a.pack(content,a.label("Para remover, execute Desinstalar.sh na pasta do aplicativo. O ZIP e os arquivos que não pertencem ao app são preservados.","dim",true));
        a.pack(content,a.label("Processo desta central: "+std::to_string(getpid())+"\nPasta: "+folder().string(),"dim",true));
        a.pack(content,a.label(external_report(),"dim",true));
        a.pack(content,a.text_button("Abrir assistente de instalação",[this](){launch_detached(folder()/"audio-painel-rc",app_.demo?std::vector<std::string>{"--instalar","--demo"}:std::vector<std::string>{"--instalar"});}));
        if(!a.demo)a.pack(content,a.text_button("Desinstalar Áudio Painel RC",[](){launch_detached(folder()/"audio-painel-rc",{"--desinstalador"});}));
    }
}
void SettingsView::appearance(void* content){
    auto& a=app_;auto& u=a.ui;const Config c=a.config;
    a.pack(content,a.label("Aparência do painel","title"));
    auto choice=[&](const std::string& title,const std::string& key,const std::vector<std::pair<std::string,std::string>>& values,const std::string& active){
        void* row=a.box(true,5);a.pack(row,a.label(title,"section"));
        auto select=std::make_unique<Selector>(a,[this,key](const Choice& c){if(key=="layout")app_.set(key,std::stoi(c.node.name));else app_.set(key,c.node.name);});
        std::vector<Choice> choices;for(const auto& kv:values){Choice c;c.key=kv.first;c.node.name=kv.first;c.label=kv.second;choices.push_back(c);}select->update(choices,active);a.pack(row,select->widget);a.pack(content,row);preferences_.push_back(std::move(select));
    };
    choice("Layout","layout",{{"4","4 · Master + microfone, aplicativos e dispositivos"},{"1","1 · Compacto, com mixer recolhível"},{"3","3 · Compacto com abas"}},std::to_string(c.layout));
    choice("Tema","theme",{{"dark","Escuro"},{"light","Claro"},{"system","Acompanhar o sistema"}},c.theme);
    choice("Cor de destaque","accent",{{"#91a8ff","Lavanda"},{"#69b9df","Azul"},{"#7ac6ad","Verde"},{"#caa2df","Violeta"},{"#d9b37e","Âmbar"},{"#e0a0b4","Rosa"}},c.accent);
    void* customrow=a.box(false,10);a.pack(customrow,a.label("Ou escolha uma cor personalizada","dim"),true);
    Rgba color{std::stoi(c.accent.substr(1,2),nullptr,16)/255.0,std::stoi(c.accent.substr(3,2),nullptr,16)/255.0,std::stoi(c.accent.substr(5,2),nullptr,16)/255.0,1};
    void* picker=u.gtk_color_button_new_with_rgba(&color);u.gtk_color_button_set_title(picker,"Cor de destaque do Áudio RC");a.pack(customrow,picker);a.pack(content,customrow);
    watch(u,picker,"color-set",[this](void* w){Rgba v{};app_.ui.gtk_color_chooser_get_rgba(w,&v);char hex[8];std::snprintf(hex,sizeof hex,"#%02x%02x%02x",int(std::lround(v.red*255)),int(std::lround(v.green*255)),int(std::lround(v.blue*255)));app_.set("accent",hex);});
    choice("Densidade","density",{{"normal","Confortável"},{"compact","Compacta"}},c.density);
    auto spin=[&](const std::string& label,const std::string& key,double value,double min,double max,double step){
        void* row=a.box(false,10);a.pack(row,a.label(label,"dim"),true);void* input=u.gtk_spin_button_new_with_range(min,max,step);u.gtk_spin_button_set_value(input,value);a.pack(row,input);a.pack(content,row);
        watch(u,input,"value-changed",[this,key](void* w){app_.set(key,int(app_.ui.gtk_spin_button_get_value(w)));});
    };
    spin("Largura do layout 4 (px)","width",c.width,440,760,10);spin("Tamanho do texto (px)","font",c.font,11,17,1);spin("Cantos arredondados (px)","radius",c.radius,0,24,1);spin("Linhas de aplicativos visíveis","rows",c.rows,2,8,1);
    void* opacity=a.box(true,4);a.pack(opacity,a.label("Opacidade do fundo · 70% a 100%","section"));void* slider=u.gtk_scale_new_with_range(0,70,100,1);u.gtk_range_set_value(slider,c.opacity*100);u.gtk_scale_set_draw_value(slider,1);a.pack(opacity,slider);a.pack(content,opacity);
    // Um salvamento após a pausa no ajuste, em vez de gravar a cada pixel.
    watch(u,slider,"value-changed",[this](void* w){double value=app_.ui.gtk_range_get_value(w)/100.0;app_.ui.cancel(opacity_save_);opacity_save_=app_.ui.later([this,value](){opacity_save_=0;app_.set("opacity",value);},180);});
    a.pack(content,a.label(a.composited()?"Composição ativa: apenas o fundo recebe transparência.":"A composição do Trinity está desativada. O fundo ficará opaco até ela ser ativada nas configurações do desktop.","dim",true));
    a.section(content,"Comportamento");
    auto check=[&](const std::string& label,const std::string& key,bool value){void* w=u.gtk_check_button_new_with_label(label.c_str());u.gtk_toggle_button_set_active(w,value);a.pack(content,w);watch(u,w,"toggled",[this,key](void* w){app_.set(key,bool(app_.ui.gtk_toggle_button_get_active(w)));});};
    check("Esconder quando o mouse sair da área","hide_leave",c.hide_leave);spin("Espera antes de esconder (ms)","hide_delay",c.hide_delay,100,2000,50);spin("Margem de segurança do mouse (px)","margin",c.margin,0,40,1);spin("Distância da barra (px)","gap",c.gap,0,24,1);check("Animação curta ao abrir","animation",c.animation);
    spin("Liberar painel oculto após (s; 0 mantém pronto)","release_seconds",c.release_seconds,0,300,5);
    a.pack(content,a.label("Os botões e textos permanecem opacos. O mixer tem rolagem e a altura é limitada à área disponível da tela.","dim",true));
    a.section(content,"Inicialização");void* automatic=u.gtk_check_button_new_with_label("Iniciar este painel junto com a sessão");autostart_toggle_=automatic;u.gtk_toggle_button_set_active(automatic,autostart_enabled());u.gtk_widget_set_sensitive(automatic,!a.demo);a.pack(content,automatic);
    watch(u,automatic,"toggled",[this](void* w){if(syncing_startup_)return;try{autostart(app_.ui.gtk_toggle_button_get_active(w));app_.message("");}catch(const std::exception& e){app_.message(e.what());}});
    a.pack(content,a.label("Esta opção cria somente o atalho deste painel. O KMix continua instalado e sua inicialização não é alterada.","dim",true));
    a.pack(content,a.label("Arquivo externo opcional: "+autostart_path()+"\nRegistrado em integracoes.json para remoção verificada.","dim",true));
    if(!detect_environment(false).trinity)return;
    a.section(content,"Substituição opcional do KMix");
    a.pack(content,a.label("Para usar um único ícone de som, a troca explícita desativa o autostart do KMix e inicia o Áudio RC no login. O valor anterior fica registrado nesta pasta para desfazer a troca. A inicialização normal nunca faz essa substituição.","dim",true));
    void* integration_buttons=a.box(false,8);a.pack(content,integration_buttons);
    a.pack(integration_buttons,a.text_button("Substituir KMix",[this](){integration(true);}));a.pack(integration_buttons,a.text_button("Restaurar KMix",[this](){integration(false);}));
    integration_note_=a.label("KMix não foi alterado automaticamente.","dim",true);a.pack(content,integration_note_);
}
void SettingsView::update(const State& s){
    auto& a=app_;auto& u=a.ui;if(!u.gtk_widget_get_mapped(window))return;
    void* native=u.gtk_widget_get_window(window);if(native&&(u.gdk_window_get_state(native)&2)){stop_voice();return;}
    const char* current=u.gtk_stack_get_visible_child_name(stack_);std::string name=current?current:"";
    if(voice_&&voice_->running&&connection_signature(s)!=voice_routes_){
        stop_voice();if(voice_status_)u.gtk_label_set_text(voice_status_,"Teste encerrado: a fonte, saída ou porta física mudou.");
    }
    if(name=="devices"&&master_){master_->update(one(s.sink));output_->update(s.outputs,s.sink?s.sink->ref.name:"");
        std::string text="Saída atual: "+(s.sink?s.sink->label:"não definida")+"\nFonte atual: "+(s.source?s.source->label:"não definida");u.gtk_label_set_text(info_,text.c_str());
    }else if(name=="mic"&&mic_){
        mic_->update(one(s.source));physical_->update(one(s.physical));
        std::set<std::string> seen;std::vector<Choice> sources;for(const auto& c:s.inputs)if(seen.insert(c.node.name).second){Choice p;p.key=c.node.name;p.node=c.node;auto n=std::find_if(s.nodes.begin(),s.nodes.end(),[&](const Node& n){return n.ref==c.node;});p.label=n==s.nodes.end()?c.label:n->label;sources.push_back(p);}
        source_->update(sources,s.source?s.source->ref.name:"");
        std::vector<Choice> inputs;std::string target=s.source?s.source->ref.name:"";
        if(s.filtered){target=s.physical?s.physical->ref.name:"";for(const auto& c:s.inputs)if(c.node.name==target)inputs.push_back(c);}else inputs=s.inputs;
        input_->update(inputs,target);
        std::string note=s.filtered?"Filtro de ruído ativo. A porta física pode ser alterada sem desativá-lo.":"A fonte selecionada é usada pelos aplicativos que acompanham o padrão do sistema.";
        if(s.filtered&&s.physical&&s.physical->muted)note+=" A entrada física está silenciada.";
        if(s.filtered&&!s.physical)note+=" A origem física não foi identificada; nenhum dispositivo será escolhido automaticamente.";
        u.gtk_label_set_text(mic_note_,note.c_str());u.gtk_widget_set_visible(enable_,s.filtered&&s.physical&&s.physical->muted);update_voice();
    }else if(name=="apps"&&apps_)apps_->update(s);
    else if(name=="diagnostics"&&diagnostics_&&diagnosis_text_.empty())u.gtk_label_set_text(diagnostics_,diagnostic_text().c_str());
    else if(name=="appearance"&&autostart_toggle_){syncing_startup_=true;u.gtk_toggle_button_set_active(autostart_toggle_,autostart_enabled());syncing_startup_=false;}
    std::string error=a.error().empty()?s.error:a.error();u.gtk_label_set_text(error_,error.c_str());u.gtk_widget_set_visible(error_,!error.empty());
}
void SettingsView::start_voice(){
    if(app_.demo){app_.message("A demonstração não captura áudio. Abra a central em modo normal para testar sua voz.");return;}
    const auto& s=app_.state();if(!s.source||!s.sink){app_.message("Selecione uma fonte e uma saída disponíveis para iniciar o teste.");return;}
    if(!voice_)voice_=std::make_unique<VoiceTest>(app_.ui,[this](const std::string& text){app_.ui.cancel(voice_timer_);update_voice();if(voice_status_)app_.ui.gtk_label_set_text(voice_status_,text.c_str());});
    voice_routes_=connection_signature(s);voice_->start(s.source->ref,s.sink->ref);app_.ui.gtk_label_set_text(voice_status_,"Retorno ativo · atraso de 1 s + latência dos dispositivos · limite de 30 s.");
    app_.ui.cancel(voice_timer_);voice_timer_=app_.ui.g_timeout_add(100,+[](void* data)->int{static_cast<SettingsView*>(data)->update_voice();return 1;},this);update_voice();
}
void SettingsView::stop_voice(){
    app_.ui.cancel(voice_timer_);if(voice_)voice_->stop();update_voice();if(voice_status_)app_.ui.gtk_label_set_text(voice_status_,"Parado · nenhuma gravação é salva.");
}
void SettingsView::update_voice(){
    if(!voice_start_)return;bool running=voice_&&voice_->running;
    app_.ui.gtk_widget_set_sensitive(voice_start_,!running&&app_.state().source.has_value()&&app_.state().sink.has_value());app_.ui.gtk_widget_set_sensitive(voice_stop_,running);
    app_.ui.gtk_widget_queue_draw(voice_meter_);
}
std::string SettingsView::diagnostic_text()const {
    const auto& s=app_.state();return std::string("Áudio RC C++ ")+VERSION+" · PID "+std::to_string(getpid())+"\n\nConexão: "+(s.error.empty()?"PipeWire conectado":s.error)+
        "\nSaída: "+(s.sink?s.sink->ref.name:"não definida")+"\nFonte: "+(s.source?s.source->ref.name:"não definida")+
        "\nFiltro associado: "+(s.filtered?"sim":"não")+"\nMudo físico: "+(s.physical?(s.physical->muted?"sim":"não"):"não identificado")+
        "\nGrupos de aplicativos: "+std::to_string(s.apps().size())+"\nLayout: "+std::to_string(app_.config.layout)+" · tema: "+app_.config.theme+
        "\nComposição: "+(app_.composited()?"ativa":"desativada")+"\n\nPasta: "+folder().string()+"\nAutostart próprio: "+(autostart_enabled()?"ativo":"desativado")+"\nKMix e arquivos do filtro/HDMI preservados.";
}
void SettingsView::run_diagnosis(){
    if(diagnosis_)return;diagnosis_text_.clear();app_.ui.gtk_label_set_text(diagnostics_,"Consultando o áudio…");
    try{diagnosis_=std::make_unique<Child>(app_.ui,std::vector<std::string>{(folder()/"audio-painel-rc").string(),"--verificar"},true,35000,
        [this](std::string_view s){if(diagnosis_text_.size()+s.size()>262144)throw std::runtime_error("Diagnóstico excedeu limite");diagnosis_text_.append(s);},
        [this](int status,const std::string& err){diagnosis_.reset();if(!err.empty())diagnosis_text_+="\n"+err;if(status)diagnosis_text_+="\nCódigo: "+std::to_string(status);app_.ui.gtk_label_set_text(diagnostics_,diagnosis_text_.c_str());});
    }catch(const std::exception& e){app_.message(e.what());}
}
void SettingsView::integration(bool replace){
    if(app_.demo){app_.message("A demonstração não altera o KMix nem a inicialização.");return;}if(integration_)return;
    app_.ui.gtk_label_set_text(integration_note_,replace?"Validando o novo ícone e o áudio antes de trocar…":"Restaurando o KMix…");
    auto output=std::make_shared<std::string>();
    try{integration_=std::make_unique<Child>(app_.ui,std::vector<std::string>{(folder()/"audio-painel-rc").string(),replace?"--substituir-kmix":"--restaurar-kmix"},true,25000,
        [output](std::string_view s){if(output->size()+s.size()<8192)output->append(s);},
        [this,output](int status,const std::string& error){integration_.reset();std::string text=error.empty()?*output:error;app_.ui.gtk_label_set_text(integration_note_,text.c_str());if(status)app_.message(text);app_.reload();},true);
    }catch(const std::exception& e){app_.message(e.what());}
}
}

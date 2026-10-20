#include "setup.hpp"
#include "installation.hpp"
#include "keyboard.hpp"
#include <array>
#include <mutex>
#include <sstream>
#include <thread>
#include <unistd.h>
namespace rc {
namespace {
const char* setup_css=R"CSS(
window.rc-setup {background:linear-gradient(125deg,#071426,#102c4f);color:#edf4ff;font-family:Sans;font-size:13px;}
.rc-setup label {color:inherit;}
.rc-setup .rail {background:alpha(#061325,0.78);border-right:1px solid #1b3655;padding:26px 20px;}
.rc-setup .brand {font-size:18px;font-weight:700;letter-spacing:0.2px;}
.rc-setup .overline {font-size:10px;letter-spacing:1.4px;color:#93b6e1;font-weight:600;}
.rc-setup .hero {font-size:27px;font-weight:600;letter-spacing:-0.5px;}
.rc-setup .sub {color:#a4b9d4;font-size:13px;}
.rc-setup .step {padding:10px 12px;border-radius:8px;color:#7993b3;font-size:12px;}
.rc-setup .step.active {background:#17385f;color:#d7e8ff;font-weight:600;}
.rc-setup .step.finished {color:#a6bdda;}
.rc-setup .card {background:#102c4b;border:1px solid #28486c;border-radius:10px;padding:15px;}
.rc-setup .caption {font-size:12px;font-weight:600;color:#c5d6ec;}
.rc-setup .path {font-family:monospace;font-size:11px;color:#a5bede;}
.rc-setup .success {color:#86dbba;font-weight:600;}
.rc-setup .warning {color:#ecc18d;font-size:12px;}
.rc-setup .error {color:#ffb2b9;font-size:12px;}
.rc-setup entry,.rc-setup spinbutton {background:#091e36;color:#edf4ff;border:1px solid #33577c;border-radius:6px;box-shadow:none;min-height:30px;padding:4px 9px;}
.rc-setup entry:focus {border-color:#85b5ff;}
.rc-setup button {background:#173654;color:#e2edff;border:1px solid #35597e;border-radius:7px;padding:7px 12px;min-height:24px;box-shadow:none;text-shadow:none;font-size:12px;}
.rc-setup button:hover {background:#23496d;border-color:#85b5ff;}
.rc-setup button:active {background:#2e527d;}
.rc-setup button.primary {background:#8bb8ff;color:#07172c;border-color:#8bb8ff;font-weight:700;}
.rc-setup button.primary label {color:#07172c;}
.rc-setup button:disabled {opacity:0.40;}
.rc-setup check {background:#0a203a;border:1px solid #466a91;border-radius:4px;min-height:14px;min-width:14px;}
.rc-setup check:checked {background:#8bb8ff;color:#07172c;}
.rc-setup checkbutton {padding:3px 0;}
.rc-setup separator {background:#244361;min-height:1px;}
.rc-setup scrolledwindow,.rc-setup viewport {background:transparent;border:0;}
.rc-setup scrollbar {background:transparent;}
.rc-setup scrollbar slider {background:#35577b;min-width:5px;min-height:25px;border-radius:4px;}
.rc-setup progressbar trough {background:#0b203b;border:1px solid #2e4f76;border-radius:5px;min-height:8px;}
.rc-setup progressbar progress {background:#8bb8ff;border:0;border-radius:5px;min-height:8px;}
)CSS";
class Frame {
public:
    GtkApi& u;void* window=nullptr;void* provider=nullptr;void* content=nullptr;void* holder=nullptr;void* title=nullptr;void* subtitle=nullptr;void* status=nullptr;void* back=nullptr;void* next=nullptr;void* cancel=nullptr;
    std::vector<void*> steps;
    Frame(GtkApi& api,const std::string& name,const std::vector<std::string>& labels):u(api){
        provider=u.gtk_css_provider_new();Error* e=nullptr;if(!u.gtk_css_provider_load_from_data(provider,setup_css,-1,&e)){std::string error=e?e->message:"Tema inválido";if(e)u.g_error_free(e);throw std::runtime_error(error);}u.gtk_style_context_add_provider_for_screen(u.gdk_screen_get_default(),provider,900);
        window=u.gtk_window_new(0);u.gtk_window_set_title(window,name.c_str());u.gtk_window_set_wmclass(window,"audio-rc-installer","AudioRcInstaller");u.gtk_window_set_default_size(window,920,650);u.gtk_window_set_position(window,1);u.gtk_window_set_icon_name(window,"audio-volume-high");u.cls(window,"rc-setup");
        auto root=box(false,0);u.gtk_container_add(window,root);auto rail=box(true,10);u.cls(rail,"rail");u.gtk_widget_set_size_request(rail,172,-1);pack(root,rail);
        auto mark=u.gtk_drawing_area_new();u.gtk_widget_set_size_request(mark,54,56);u.gtk_widget_set_halign(mark,1);pack(rail,mark);
        u.connect(mark,"draw",+[](void*,void* cr,void* data)->int{auto& u=*static_cast<GtkApi*>(data);int heights[]={16,30,44,25,12};for(int i=0;i<5;++i){u.cairo_set_source_rgba(cr,.55,.73,1,.68+i*.06);u.cairo_rectangle(cr,4+i*10,28-heights[i]/2,4,heights[i]);u.cairo_fill(cr);}return 0;},&u);
        pack(rail,label("Áudio Painel RC","brand"));pack(rail,label("C++  /  LINUX","overline"));auto space=box();u.gtk_widget_set_size_request(space,-1,18);pack(rail,space);
        for(const auto& text:labels){auto l=label(text,"step");pack(rail,l);steps.push_back(l);}
        pack(rail,box(),true);pack(rail,label("Sua pasta. Seu painel.","sub"));pack(rail,label(std::string("VERSÃO ")+VERSION,"overline"));
        auto main=box(true,13);u.gtk_container_set_border_width(main,26);pack(root,main,true);
        pack(main,label("ÁUDIO RC  /  ASSISTENTE","overline"));title=label("","hero",true);pack(main,title);subtitle=label("","sub",true);pack(main,subtitle);
        auto scroll=u.gtk_scrolled_window_new(nullptr,nullptr);u.gtk_scrolled_window_set_policy(scroll,2,1);u.gtk_widget_set_vexpand(scroll,1);pack(main,scroll,true);holder=box();u.gtk_container_add(scroll,holder);
        status=label("","error",true);u.gtk_label_set_lines(status,3);pack(main,status);pack(main,u.gtk_separator_new(0));
        auto foot=box(false,9);pack(main,foot);cancel=button("Cancelar",{});pack(foot,cancel);pack(foot,box(),true);back=button("Voltar",{});pack(foot,back);next=button("Continuar",{},"primary");pack(foot,next);
    }
    ~Frame(){if(window)u.gtk_widget_destroy(window);u.gtk_style_context_remove_provider_for_screen(u.gdk_screen_get_default(),provider);u.g_object_unref(provider);}
    void* box(bool vertical=true,int gap=10){return u.gtk_box_new(vertical?1:0,gap);}
    void pack(void* parent,void* child,bool expand=false){u.gtk_box_pack_start(parent,child,expand,expand,0);}
    void* label(const std::string& text,const char* cls=nullptr,bool wrap=false){auto l=u.gtk_label_new(text.c_str());u.gtk_label_set_xalign(l,0);if(cls)u.cls(l,cls);if(wrap){u.gtk_label_set_line_wrap(l,1);u.gtk_label_set_line_wrap_mode(l,2);u.gtk_label_set_max_width_chars(l,65);}return l;}
    void* button(const std::string& text,std::function<void()> cb,const char* cls=nullptr){auto b=u.gtk_button_new_with_label(text.c_str());if(cls)u.cls(b,cls);if(cb)u.clicked(b,std::move(cb));return b;}
    void* check(const std::string& text,bool value){auto c=u.gtk_check_button_new_with_label(text.c_str());u.gtk_toggle_button_set_active(c,value);return c;}
    void* card(){auto c=box();u.cls(c,"card");pack(content,c);return c;}
    void clear(){if(content)u.gtk_widget_destroy(content);content=box(true,12);pack(holder,content);u.gtk_label_set_text(status,"");}
    fs::path choose(const char* caption,bool directory){
        auto dialog=u.gtk_file_chooser_dialog_new(caption,window,directory?2:0,"Cancelar",-6,"Selecionar",-3,static_cast<const char*>(nullptr));u.gtk_file_chooser_set_local_only(dialog,1);
        // O seletor GTK usa GSettings em memória; não grava seu histórico no dconf.
        u.gtk_file_chooser_set_current_folder(dialog,folder().c_str());fs::path result;if(u.gtk_dialog_run(dialog)==-3){char* p=u.gtk_file_chooser_get_filename(dialog);if(p){result=p;u.g_free(p);}}u.gtk_widget_destroy(dialog);return result;
    }
    void heading(const std::string& a,const std::string& b){u.gtk_label_set_text(title,a.c_str());u.gtk_label_set_text(subtitle,b.c_str());}
    void active(int index){for(size_t n=0;n<steps.size();++n){u.uncls(steps[n],"active");u.uncls(steps[n],"finished");if(int(n)==index)u.cls(steps[n],"active");else if(int(n)<index)u.cls(steps[n],"finished");}}
    void show(){u.gtk_widget_show_all(window);u.gtk_window_present(window);}
};
class Setup {
    Frame f;GtkApi& u;bool keys_only,demo;int page=0,testing=-1;unsigned timeout=0,timer=0;
    bool closing=false,running=false,complete=false,worker_done=false,detecting=false;
    bool archive_edited=false,updating_archive=false;
    std::chrono::steady_clock::time_point heartbeat=std::chrono::steady_clock::now();
    std::atomic<bool> cancelled{false};std::thread worker;std::mutex mutex;
    double fraction=0;std::string progress_text,error,device_text;Environment environment;InstallResult installed;
    Environment detected_environment;std::string detected_devices;fs::path detected_archive;
    InstallOptions options;std::optional<InstallPlan> plan;std::vector<KeyBinding> keys;bool direct=false;int step=5;
    Json initial_keyboard;
    void* path_entry=nullptr;void* archive_entry=nullptr;void* gather=nullptr;void* extras=nullptr;
    void* direct_check=nullptr;void* custom_check=nullptr;void* step_spin=nullptr;void* key_note=nullptr;void* system_label=nullptr;
    void* startup_check=nullptr;void* menu_check=nullptr;void* desktop_check=nullptr;void* external_label=nullptr;
    void* progress=nullptr;void* progress_label=nullptr;void* start_check=nullptr;
    std::array<void*,3> key_labels{};
    static const char* action_name(size_t n){return n==0?"Silenciar áudio":n==1?"Diminuir volume":"Aumentar volume";}
    void stop_test(){u.cancel(timeout);testing=-1;}
    void join(){if(worker.joinable())worker.join();}
    void detect(){
        if(worker.joinable())join();detecting=true;worker_done=false;
        worker=std::thread([this](){auto e=detect_environment(!demo);std::string devices;
            if(!demo&&program_exists("pw-dump"))try{auto r=run_read({"pw-dump","-N"},3000,16*1024*1024);if(r.status==0){auto s=read_graph(r.text).state();devices="Saída: "+(s.sink?s.sink->label:"nenhuma padrão")+"\nMicrofone: "+(s.source?s.source->label:"nenhum padrão")+"\n"+std::to_string(s.outputs.size())+" opções de saída · "+std::to_string(s.inputs.size())+" opções de entrada";}}catch(...){}
            auto archive=find_release_archive(folder());std::lock_guard<std::mutex> lock(mutex);detected_environment=std::move(e);detected_devices=devices;detected_archive=archive;worker_done=true;
        });
    }
    void snapshot(){
        if(page==0&&path_entry){options.destination=u.gtk_entry_get_text(path_entry);options.archive=u.gtk_entry_get_text(archive_entry);options.gather_source=u.gtk_toggle_button_get_active(gather);options.gather_extras=u.gtk_toggle_button_get_active(extras);}
        if(page==2&&direct_check){direct=u.gtk_toggle_button_get_active(direct_check);step=int(u.gtk_spin_button_get_value(step_spin));if(key_config()!=initial_keyboard)options.keyboard_changed=true;}
        if(page==3&&startup_check){options.startup=u.gtk_toggle_button_get_active(startup_check);options.menu=u.gtk_toggle_button_get_active(menu_check);options.desktop=u.gtk_toggle_button_get_active(desktop_check);}
    }
    Json key_config(){return keyboard_json(keys,direct?"direct":"compatibility",step);}
    void update_key_labels(){for(size_t n=0;n<3;++n)if(key_labels[n]){char* name=u.gtk_accelerator_get_label(keys[n].keyval,keys[n].modifiers);std::string text=(keys[n].confirmed?"✓ Confirmada · ":"Pendente · ")+std::string(name?name:"tecla");if(name)u.g_free(name);u.gtk_label_set_text(key_labels[n],text.c_str());if(keys[n].confirmed)u.cls(key_labels[n],"success");else u.uncls(key_labels[n],"success");}
        if(page==2&&!keys_only)u.gtk_button_set_label(f.next,std::any_of(keys.begin(),keys.end(),[](const auto& k){return k.confirmed;})?"Continuar":"Configurar depois");}
    void start_test(size_t n){stop_test();testing=int(n);std::string text="Pressione "+std::string(action_name(n))+" uma vez. ";if(u.gtk_toggle_button_get_active(custom_check))text="Pressione a combinação que deseja usar para "+std::string(action_name(n))+". Use Ctrl, Alt ou Super nas combinações personalizadas. ";text+="O teste termina em 15 segundos; Esc cancela.";u.gtk_label_set_text(key_note,text.c_str());u.gtk_widget_grab_focus(f.window);
        timeout=u.later([this](){timeout=0;testing=-1;if(key_note)u.gtk_label_set_text(key_note,"Evento não recebido. A tecla continua pendente. O desktop pode reservar essa combinação; você pode continuar em modo compatibilidade.");},15000);
    }
    bool key_event(void* event){
        if(page!=2||testing<0)return false;unsigned val=0,mods=0;u.gdk_event_get_keyval(event,&val);u.gdk_event_get_state(event,&mods);mods&=0xcd;
        if(val==0xff1b){stop_test();u.gtk_label_set_text(key_note,"Teste cancelado. Você pode tentar novamente ou configurar depois.");return true;}
        bool custom=u.gtk_toggle_button_get_active(custom_check);unsigned expected=testing==0?0x1008ff12:testing==1?0x1008ff11:0x1008ff13;
        if(!custom&&val!=expected)return false;if(!valid_shortcut(val,mods)){u.gtk_label_set_text(key_note,"Use uma tecla multimídia ou uma combinação com Ctrl, Alt ou Super.");return true;}
        for(size_t n=0;n<keys.size();++n)if(int(n)!=testing&&keys[n].keyval==val&&keys[n].modifiers==mods){u.gtk_label_set_text(key_note,"Essa combinação já foi escolhida para outra função.");return true;}
        auto& b=keys[size_t(testing)];b.keyval=val;b.modifiers=mods;b.confirmed=true;options.keyboard_changed=true;stop_test();update_key_labels();u.gtk_label_set_text(key_note,"Evento confirmado nesta janela. Abra Controle direto e outras combinações para verificar o registro global, ou mantenha a compatibilidade.");return true;
    }
    void verify(){snapshot();auto selected=keys;selected.erase(std::remove_if(selected.begin(),selected.end(),[](const auto& b){return !b.confirmed;}),selected.end());if(selected.empty())selected=keys;auto p=probe_shortcuts(selected);u.gtk_label_set_text(key_note,p.message.c_str());if(!p.available){direct=false;u.gtk_toggle_button_set_active(direct_check,0);}}
    void preview_external(){
        options.startup=u.gtk_toggle_button_get_active(startup_check);options.menu=u.gtk_toggle_button_get_active(menu_check);options.desktop=u.gtk_toggle_button_get_active(desktop_check);
        std::string text;if(options.startup)text+="• "+startup_location().string()+"\n  Iniciar ao entrar na sessão.\n";if(options.menu)text+="• "+menu_location().string()+"\n  Aparecer no menu de aplicativos.\n";if(options.desktop)text+="• "+desktop_location().string()+"\n  Abrir pela área de trabalho.\n";
        if(text.empty())text="Nenhum atalho externo será criado por estas opções.\n";text+="\nOs caminhos e o conteúdo criado ficarão registrados na pasta do app para remoção verificada.";u.gtk_label_set_text(external_label,text.c_str());
    }
    void go(int target){
        snapshot();if(target==4){options.keyboard=key_config();plan=plan_install(options);}stop_test();page=target;path_entry=archive_entry=gather=extras=direct_check=custom_check=step_spin=key_note=system_label=startup_check=menu_check=desktop_check=external_label=progress=progress_label=start_check=nullptr;key_labels.fill(nullptr);
        f.clear();f.active(std::min(page,4));u.gtk_widget_set_visible(f.back,page>0&&page<5&&!keys_only);u.gtk_widget_set_sensitive(f.next,1);u.gtk_widget_set_sensitive(f.cancel,1);
        u.gtk_button_set_label(f.next,keys_only?"Salvar configurações":page==4?"Instalar":page==6?"Concluir":"Continuar");
        if(page==0){
            f.heading("Bem-vindo ao Áudio Painel RC","Escolha onde guardar o aplicativo. Executável, configurações, instalador e registros ficarão juntos sempre que possível.");
            auto card=f.card();f.pack(card,f.label("PASTA DE INSTALAÇÃO","caption"));path_entry=u.gtk_entry_new();u.gtk_entry_set_text(path_entry,options.destination.c_str());f.pack(card,path_entry);
            auto row=f.box(false,8);f.pack(card,row);f.pack(row,f.button("Procurar pasta",[this](){auto p=f.choose("Escolha a pasta do Áudio Painel RC",true);if(!p.empty())u.gtk_entry_set_text(path_entry,p.c_str());}));f.pack(row,f.button("Usar esta pasta",[this](){u.gtk_entry_set_text(path_entry,folder().c_str());}));
            gather=f.check("Recolher os arquivos da origem depois de verificar a cópia",options.gather_source);f.pack(f.content,gather);
            extras=f.check("Recolher arquivos adicionais; preservá-los na desinstalação",options.gather_extras);f.pack(f.content,extras);
            auto archive=f.card();f.pack(archive,f.label("ZIP DE DOWNLOAD · SERÁ PRESERVADO AO DESINSTALAR","caption"));auto ar=f.box(false,8);f.pack(archive,ar);archive_entry=u.gtk_entry_new();u.gtk_entry_set_text(archive_entry,options.archive.c_str());f.pack(ar,archive_entry,true);
            u.connect(archive_entry,"changed",+[](void*,void* p){auto* s=static_cast<Setup*>(p);if(!s->updating_archive)s->archive_edited=true;},this);
            f.pack(ar,f.button("Selecionar ZIP",[this](){auto p=f.choose("Selecione o ZIP que deseja guardar dentro do aplicativo",false);if(!p.empty())u.gtk_entry_set_text(archive_entry,p.c_str());}));f.pack(ar,f.button("Limpar",[this](){archive_edited=true;u.gtk_entry_set_text(archive_entry,"");}));
            f.pack(f.content,f.label("O instalador procura um ZIP desta mesma versão ao lado da pasta extraída. Você também pode selecioná-lo. A origem só é recolhida após a verificação SHA-256.","sub",true));
        }else if(page==1){
            f.heading("Seu sistema, reconhecido","O assistente verifica o ambiente e explica o que está disponível nesta sessão.");auto card=f.card();system_label=f.label(detecting?"Verificando o sistema e a sessão de áudio…":environment.text(),nullptr,true);u.gtk_label_set_selectable(system_label,1);f.pack(card,system_label);
            if(!device_text.empty())f.pack(f.content,f.label(device_text,"sub",true));
            f.pack(f.content,f.label("PipeWire e WirePlumber são necessários para os controles de áudio desta versão. PulseAudio independente ainda não possui backend. Você pode concluir a instalação e resolver os requisitos depois.","sub",true));
            f.pack(f.content,f.label("Bandeja, posicionamento e teclas globais dependem do desktop. O suporte completo foi desenvolvido para X11; Wayland usa compatibilidade e pode precisar da central aberta.","warning",true));
            f.pack(f.content,f.button("Verificar novamente",[this](){if(!detecting){detect();u.gtk_label_set_text(system_label,"Verificando…");}}));
        }else if(page==2){
            f.heading("Teclas de volume","Teste as funções do seu teclado. Você pode continuar com uma, duas ou nenhuma tecla confirmada.");
            for(size_t n=0;n<3;++n){auto card=f.card();auto row=f.box(false,10);f.pack(card,row);auto titles=f.box(true,4);f.pack(row,titles,true);f.pack(titles,f.label(action_name(n),"caption"));key_labels[n]=f.label("","sub",true);f.pack(titles,key_labels[n]);f.pack(row,f.button("Testar",[this,n](){start_test(n);}));}
            update_key_labels();key_note=f.label("Compatibilidade: mantenha as teclas do desktop e acompanhe o volume no painel. O controle direto é opcional.","sub",true);f.pack(f.content,key_note);
            auto advanced=u.gtk_expander_new("Controle direto e outras combinações");auto controls=f.box(true,10);u.gtk_container_set_border_width(controls,8);u.gtk_container_add(advanced,controls);u.gtk_expander_set_expanded(advanced,direct);f.pack(f.content,advanced);
            custom_check=f.check("Ao testar, escolher outra combinação de teclas",false);f.pack(controls,custom_check);
            direct_check=f.check("Permitir controle direto pelo Áudio RC no X11",direct);f.pack(controls,direct_check);u.gtk_widget_set_sensitive(direct_check,!environment.wayland);
            auto r=f.box(false,8);f.pack(controls,r);f.pack(r,f.label("Variação por toque (%)","sub"),true);step_spin=u.gtk_spin_button_new_with_range(1,15,1);u.gtk_spin_button_set_value(step_spin,step);f.pack(r,step_spin);
            auto buttons=f.box(false,8);f.pack(controls,buttons);f.pack(buttons,f.button("Verificar disponibilidade",[this](){verify();}));f.pack(buttons,f.button("Atalhos do desktop",[this](){auto cmd=keyboard_settings_command();if(cmd.empty()){u.gtk_label_set_text(key_note,"Não há uma ferramenta de atalhos reconhecida nesta sessão. Configure pelo painel do seu desktop e use Verificar disponibilidade.");return;}auto exe=cmd[0];cmd.erase(cmd.begin());launch_detached(exe,cmd);u.gtk_label_set_text(key_note,"A ferramenta do desktop foi aberta. Se desejar controle direto, libere somente as combinações escolhidas e verifique novamente. O Áudio RC não altera esses atalhos.");}));
            f.pack(f.content,f.button("Não tenho essas teclas",[this](){stop_test();for(auto& b:keys)b.confirmed=false;direct=false;options.keyboard_changed=true;u.gtk_toggle_button_set_active(direct_check,0);update_key_labels();u.gtk_label_set_text(key_note,"Sem teclas multimídia: os controles do painel continuam disponíveis. Você pode configurar combinações alternativas depois.");}));
            if(keys_only)f.pack(f.content,f.button("Salvar diagnóstico nesta pasta",[this](){auto text=environment.text()+"\n\n"+key_config().dump()+"\n\n"+external_report();atomic_write(folder()/"RELATORIO-TECLADO.txt",text);u.gtk_label_set_text(key_note,"Diagnóstico salvo em RELATORIO-TECLADO.txt, dentro da pasta do aplicativo.");}));
        }else if(page==3){
            f.heading("Integração com o desktop","Escolha os atalhos que deseja criar. Todos são opcionais; os caminhos aparecem abaixo.");
            startup_check=f.check("Iniciar junto com a sessão",options.startup);menu_check=f.check("Adicionar ao menu de aplicativos",options.menu);desktop_check=f.check("Criar atalho na área de trabalho",options.desktop);
            for(void* w:{startup_check,menu_check,desktop_check}){f.pack(f.content,w);u.connect(w,"toggled",+[](void*,void* d){try{static_cast<Setup*>(d)->preview_external();}catch(const std::exception& e){auto* s=static_cast<Setup*>(d);s->u.gtk_label_set_text(s->f.status,e.what());}},this);}
            auto card=f.card();external_label=f.label("","path",true);u.gtk_label_set_selectable(external_label,1);f.pack(card,external_label);preview_external();
            f.pack(f.content,f.label("Esses arquivos precisam ficar nas pastas lidas pelo desktop. Para remover o app, use o desinstalador antes de apagar sua pasta, para que os atalhos também sejam retirados.","sub",true));
        }else if(page==4){
            f.heading("Pronto para instalar","Confira a pasta, o recolhimento da origem e os arquivos externos antes de aplicar.");auto l=f.label(plan->summary(),nullptr,true);u.gtk_label_set_selectable(l,1);f.pack(f.content,l);
        }else if(page==5){
            f.heading("Instalando o Áudio Painel RC","Os arquivos são copiados, verificados e confirmados antes de recolher a origem.");auto card=f.card();progress_label=f.label("Preparando…",nullptr,true);f.pack(card,progress_label);progress=u.gtk_progress_bar_new();f.pack(card,progress);f.pack(f.content,f.label("Os arquivos temporários desta instalação ficam na própria pasta de destino. Em caso de falha, o instalador tenta desfazer as alterações e mantém o registro para recuperação.","sub",true));u.gtk_widget_set_sensitive(f.next,0);u.gtk_widget_set_visible(f.back,0);
        }else if(page==6){
            f.heading("Tudo pronto","O Áudio Painel RC foi instalado e os arquivos da instalação foram registrados.");auto c=f.card();f.pack(c,f.label(installed.root.string(),"caption",true));f.pack(c,f.label(installed.warnings.empty()?"✓ Cópia verificada. Origem recolhida conforme a escolha.":"Há arquivos preservados fora da pasta. Confira o relatório abaixo.",installed.warnings.empty()?"success":"warning",true));
            auto l=f.label(installed.report,"sub",true);u.gtk_label_set_selectable(l,1);f.pack(f.content,l);start_check=f.check("Abrir o Áudio Painel RC ao concluir",true);f.pack(f.content,start_check);u.gtk_button_set_label(f.cancel,"Fechar");u.gtk_widget_set_visible(f.back,0);
        }
        u.gtk_widget_show_all(f.content);u.gtk_widget_set_visible(f.back,page>0&&page<5&&!keys_only);
    }
    void advance(){
        try{
            if(keys_only){snapshot();if(demo){u.gtk_label_set_text(f.status,"A prévia não salva preferências.");return;}if(direct){auto selected=keys;selected.erase(std::remove_if(selected.begin(),selected.end(),[](const auto& b){return !b.confirmed;}),selected.end());if(selected.empty())throw std::runtime_error("Confirme pelo menos uma função antes de ativar o controle direto.");auto probe=probe_shortcuts(selected);if(!probe.available)throw std::runtime_error(probe.message);}
                preference("keyboard",key_config());u.gtk_main_quit();return;}
            if(page==0){snapshot();if(options.destination.empty()||!options.destination.is_absolute())throw std::runtime_error("Escolha um caminho absoluto para a instalação.");
                if(!options.keyboard_changed){auto cfg=Config::load().original["keyboard"];auto root=options.destination;
                    if(root!=folder()&&fs::exists(root/"IDENTIDADE-RC")&&fs::exists(root/"preferencias.json")){reject_links(root/"IDENTIDADE-RC");reject_links(root/"preferencias.json");if(read_file(root/"IDENTIDADE-RC",128)==std::string(APP_ID)+"\n")cfg=Json::parse(read_file(root/"preferencias.json",65536))["keyboard"];}
                    keys=key_bindings(cfg);direct=cfg["mode"].text()=="direct";step=std::clamp(cfg["step"].integer(5),1,15);initial_keyboard=key_config();}
            }
            if(page==2){snapshot();if(direct){std::vector<KeyBinding> confirmed;for(const auto& b:keys)if(b.confirmed)confirmed.push_back(b);if(confirmed.empty())throw std::runtime_error("Confirme pelo menos uma tecla ou use o modo compatibilidade.");auto probe=probe_shortcuts(confirmed);if(!probe.available)throw std::runtime_error(probe.message);}}
            if(page==4){if(demo){u.gtk_label_set_text(f.status,"Prévia do instalador: nenhum arquivo foi copiado ou removido.");return;}if(detecting)throw std::runtime_error("Aguarde a verificação do sistema terminar.");go(5);running=true;worker_done=false;cancelled=false;
                auto selected=*plan;worker=std::thread([this,selected](){try{auto result=perform_install(selected,cancelled,[this](double n,const std::string& text){std::lock_guard<std::mutex> lock(mutex);fraction=n;progress_text=text;});std::lock_guard<std::mutex> lock(mutex);installed=std::move(result);}
                    catch(const std::exception& e){std::lock_guard<std::mutex> lock(mutex);error=e.what();}std::lock_guard<std::mutex> lock(mutex);worker_done=true;});return;}
            if(page==6){if(start_check&&u.gtk_toggle_button_get_active(start_check))launch_detached(installed.root/"audio-painel-rc",{"--janela"});u.gtk_main_quit();return;}
            go(page+1);
        }catch(const std::exception& e){u.gtk_label_set_text(f.status,e.what());}
    }
    void close(){if(running){closing=true;cancelled=true;u.gtk_button_set_label(f.cancel,"Cancelando…");u.gtk_widget_set_sensitive(f.cancel,0);}else u.gtk_main_quit();}
    bool tick(){
        bool finished;std::string text,err;double n;
        {std::lock_guard<std::mutex> lock(mutex);finished=worker_done;text=progress_text;err=error;n=fraction;}
        if(detecting&&finished){join();detecting=false;{std::lock_guard<std::mutex> lock(mutex);worker_done=false;environment=detected_environment;device_text=detected_devices;if(options.archive.empty())options.archive=detected_archive;}
            if(page==0&&archive_entry&&!archive_edited&&std::string(u.gtk_entry_get_text(archive_entry)).empty()){updating_archive=true;u.gtk_entry_set_text(archive_entry,options.archive.c_str());updating_archive=false;}
            if(page==1)go(1);
        }else if(running){if(progress)u.gtk_progress_bar_set_fraction(progress,n);if(progress_label)u.gtk_label_set_text(progress_label,text.c_str());if(n>=.90)u.gtk_widget_set_sensitive(f.cancel,0);
            if(finished){join();running=false;if(!err.empty()){u.gtk_label_set_text(f.status,err.c_str());u.gtk_widget_set_sensitive(f.cancel,1);u.gtk_button_set_label(f.cancel,"Fechar");if(closing)u.gtk_main_quit();}
                else{complete=true;go(6);if(closing)u.gtk_main_quit();}}
        }
        if(keys_only&&!demo&&std::chrono::steady_clock::now()-heartbeat>std::chrono::seconds(60)){heartbeat=std::chrono::steady_clock::now();try{ipc("tray","keys-pause");}catch(...){} }
        return true;
    }
public:
    Setup(GtkApi& api,bool only,bool demonstration):f(api,only?"Áudio RC — Teclado e teclas Fn":"Instalar Áudio Painel RC",{"01   Sua pasta","02   Seu sistema","03   Teclas de volume","04   Integração","05   Revisão e instalação"}),u(api),keys_only(only),demo(demonstration){
        options.source=folder();options.destination=fs::exists(folder()/"instalacao.json")?folder():fs::path(env_value("HOME"))/"Apps/Audio-Painel-RC";auto cfg=Config::load().original["keyboard"];keys=key_bindings(cfg);direct=cfg["mode"].text()=="direct";step=std::clamp(cfg["step"].integer(5),1,15);initial_keyboard=key_config();
        u.clicked(f.next,[this](){advance();});u.clicked(f.back,[this](){go(page-1);});u.clicked(f.cancel,[this](){close();});
        u.connect(f.window,"delete-event",+[](void*,void*,void* data)->int{static_cast<Setup*>(data)->close();return 1;},this);
        u.connect(f.window,"key-press-event",+[](void*,void* event,void* data)->int{return static_cast<Setup*>(data)->key_event(event);},this);
        environment=detect_environment(false);go(keys_only?2:0);f.show();u.gtk_widget_set_visible(f.back,0);
        if(keys_only&&!demo)try{ipc("tray","keys-pause");}catch(...){}
        detect();timer=u.g_timeout_add(60,+[](void* data)->int{return static_cast<Setup*>(data)->tick();},this);
    }
    ~Setup(){cancelled=true;u.cancel(timer);stop_test();join();if(keys_only&&!demo)try{ipc("tray","keys-resume");}catch(...){};}
    int run(){u.gtk_main();return 0;}
};
class Remover {
    Frame f;GtkApi& u;std::thread worker;std::atomic<bool> done{false};unsigned timer=0;bool running=false,finished=false,demonstration=false;std::string report,error;void* text=nullptr;
    void remove(){
        if(demonstration){u.gtk_label_set_text(f.status,"Prévia do desinstalador: nenhum arquivo será removido.");return;}
        if(finished){u.gtk_main_quit();return;}if(running)return;running=true;u.gtk_widget_set_sensitive(f.next,0);u.gtk_widget_set_sensitive(f.cancel,0);u.gtk_label_set_text(f.status,"Encerrando as instâncias desta pasta e removendo as integrações registradas…");
        worker=std::thread([this](){std::ostringstream stream;try{uninstall(true,&stream);}catch(const std::exception& e){error=e.what();}report=stream.str();done=true;});
    }
public:
    Remover(GtkApi& api,bool preview):f(api,"Desinstalar Áudio Painel RC",{"01   Conferir instalação","02   Restaurar integrações","03   Remover aplicativo"}),u(api),demonstration(preview){
        f.clear();f.active(0);f.heading("Desinstalar Áudio Painel RC","O aplicativo removerá seus arquivos e as integrações registradas nesta pasta. ZIPs e arquivos pessoais serão preservados.");
        auto c=f.card();f.pack(c,f.label(folder().string(),"caption",true));auto l=f.label(external_report(),"sub",true);u.gtk_label_set_selectable(l,1);f.pack(f.content,l);
        text=f.label("Se um atalho ou uma configuração tiver sido alterado depois, o desinstalador interrompe a remoção e preserva o registro para revisão.","sub",true);f.pack(f.content,text);
        u.gtk_button_set_label(f.next,"Desinstalar");u.clicked(f.next,[this](){remove();});u.clicked(f.cancel,[this](){if(!running)u.gtk_main_quit();});u.connect(f.window,"delete-event",+[](void*,void*,void* data)->int{auto* s=static_cast<Remover*>(data);if(!s->running)s->u.gtk_main_quit();return 1;},this);
        f.show();u.gtk_widget_set_visible(f.back,0);timer=u.g_timeout_add(100,+[](void* data)->int{auto* s=static_cast<Remover*>(data);if(!s->done)return 1;s->timer=0;s->worker.join();s->running=false;s->finished=true;s->f.active(2);
            s->f.heading(s->error.empty()?"Aplicativo removido":"A remoção precisa de revisão",s->error.empty()?"Os arquivos próprios foram removidos. ZIPs e arquivos pessoais continuam preservados.":"Os arquivos necessários para revisar e repetir a remoção foram preservados.");
            s->u.gtk_label_set_text(s->text,(s->report+(s->error.empty()?"":"\n"+s->error)).c_str());s->u.gtk_label_set_text(s->f.status,"");s->u.gtk_button_set_label(s->f.next,"Fechar");s->u.gtk_widget_set_sensitive(s->f.next,1);s->u.gtk_widget_set_sensitive(s->f.cancel,1);return 0;
        },this);
    }
    ~Remover(){u.cancel(timer);if(worker.joinable())worker.join();}
    int run(){u.gtk_main();return 0;}
};
}
int setup_window(GtkApi& ui,bool keyboard_only,bool demonstration){Setup setup(ui,keyboard_only,demonstration);return setup.run();}
int uninstall_window(GtkApi& ui,bool demonstration){Remover remover(ui,demonstration);return remover.run();}
}

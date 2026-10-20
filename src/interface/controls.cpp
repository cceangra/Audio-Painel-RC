#include "ui.hpp"
#include <sstream>
namespace rc {
VolumeControl::VolumeControl(App& a,const std::string& name,bool mic,bool inline_row):app_(a),mic_(mic),inline_(inline_row){
    auto& u=a.ui;widget=a.box(!inline_,inline_?8:4);
    title=a.label(name,inline_?"app-name":"eyebrow");
    u.gtk_label_set_ellipsize(title,3);u.gtk_label_set_max_width_chars(title,inline_?16:24);
    if(inline_){u.gtk_widget_set_size_request(title,106,-1);a.pack(widget,title);}
    else a.pack(widget,title);
    void* line=inline_?widget:a.box(false,8);if(!inline_)a.pack(widget,line);
    scale=u.gtk_scale_new_with_range(0,0,100,1);u.gtk_scale_set_draw_value(scale,0);u.gtk_widget_set_hexpand(scale,1);
    u.gtk_widget_set_tooltip_text(scale,((name)+" · até 100%; ajustar não desativa o mudo").c_str());
    a.pack(line,scale,true);
    percent=a.label("—","percent");u.gtk_label_set_width_chars(percent,4);u.gtk_label_set_xalign(percent,1);a.pack(line,percent);
    image=u.gtk_image_new_from_icon_name(mic_?"microphone-sensitivity-high-symbolic":"audio-volume-high-symbolic",1);
    mute_button=u.gtk_button_new();u.gtk_button_set_image(mute_button,image);u.cls(mute_button,"mute");
    u.gtk_widget_set_tooltip_text(mute_button,("Silenciar ou ativar "+name).c_str());a.pack(line,mute_button);
    u.clicked(mute_button,[this](){mute();});
    u.connect(scale,"value-changed",+[](void*,void* p){static_cast<VolumeControl*>(p)->change();},this);
    u.connect(scale,"button-press-event",+[](void*,void*,void* p)->int{static_cast<VolumeControl*>(p)->drag_=true;return 0;},this);
    u.connect(scale,"button-release-event",+[](void*,void*,void* p)->int{
        auto* v=static_cast<VolumeControl*>(p);v->drag_=false;
        if(v->pending_){v->app_.ui.cancel(v->pending_);v->app_.act(v->delayed_);}return 0;
    },this);
}
VolumeControl::~VolumeControl(){app_.ui.cancel(pending_);if(widget)app_.ui.gtk_widget_destroy(widget);}
void VolumeControl::change(){
    if(sync_||nodes_.empty())return;auto& u=app_.ui;
    double value=u.gtk_range_get_value(scale);u.gtk_label_set_text(percent,(std::to_string(int(std::lround(value)))+"%").c_str());
    delayed_=Action{};delayed_.kind=Action::Volume;delayed_.value=value;for(const auto& n:nodes_)delayed_.refs.push_back(n.ref);
    u.cancel(pending_);pending_=u.later([this](){pending_=0;app_.act(delayed_);},140);
}
void VolumeControl::mute(){
    if(nodes_.empty())return;Action a;a.kind=Action::Mute;a.muted=!std::all_of(nodes_.begin(),nodes_.end(),[](const Node& n){return n.muted;});for(const auto& n:nodes_)a.refs.push_back(n.ref);app_.act(a);
}
void VolumeControl::update(const std::vector<Node>& nodes){
    nodes_=nodes;auto& u=app_.ui;u.gtk_widget_set_sensitive(widget,!nodes.empty());
    if(nodes.empty()){u.gtk_label_set_text(percent,"—");return;}
    double vol=0;bool muted=true;for(const auto& n:nodes){vol=std::max(vol,n.volume*100);muted&=n.muted;}
    if(!drag_&&!pending_){sync_=true;u.gtk_range_set_value(scale,std::min(100.0,vol));u.gtk_label_set_text(percent,(std::to_string(int(std::lround(vol)))+"%").c_str());sync_=false;}
    if(last_mute_!=int(muted)){
        last_mute_=int(muted);const char* icon=mic_?(muted?"microphone-sensitivity-muted-symbolic":"microphone-sensitivity-high-symbolic"):(muted?"audio-volume-muted-symbolic":"audio-volume-high-symbolic");
        u.gtk_image_set_from_icon_name(image,icon,1);u.gtk_widget_set_tooltip_text(mute_button,muted?"Ativar som deste controle":"Silenciar este controle");
        if(muted)u.cls(mute_button,"muted");else u.uncls(mute_button,"muted");
    }
}
Selector::Selector(App& a,std::function<void(const Choice&)> callback):app_(a),callback_(std::move(callback)){
    auto& u=a.ui;
    // G_TYPE_STRING=64 e G_TYPE_BOOLEAN=20, constantes ABI do GObject.
    model_=u.gtk_list_store_new(2,static_cast<unsigned long>(64),static_cast<unsigned long>(20));
    widget=u.gtk_combo_box_new_with_model(model_);u.g_object_unref(model_);
    void* text=u.gtk_cell_renderer_text_new();u.g_object_set(text,"ellipsize",3,"max-width-chars",34,nullptr);
    u.gtk_cell_layout_pack_start(widget,text,1);u.gtk_cell_layout_add_attribute(widget,text,"text",0);u.gtk_cell_layout_add_attribute(widget,text,"sensitive",1);
    u.gtk_widget_set_hexpand(widget,1);u.cls(widget,"selector");a.selectors.insert(this);
    u.connect(widget,"changed",+[](void*,void* p){
        auto* s=static_cast<Selector*>(p);int i=s->app_.ui.gtk_combo_box_get_active(s->widget);
        if(s->syncing_||i<0||size_t(i)>=s->choices_.size())return;
        const Choice c=s->choices_[size_t(i)];if(!c.available){s->app_.message("Essa porta está desconectada.");return;}
        s->callback_(c);
    },this);
}
Selector::~Selector(){app_.selectors.erase(this);app_.ui.gtk_widget_destroy(widget);}
void Selector::update(std::vector<Choice> choices,const std::string& target){
    std::string signature;for(const auto& c:choices)signature+=c.key+'\n'+c.label+'\n'+(c.available?"1":"0");
    auto& u=app_.ui;syncing_=true;choices_=std::move(choices);
    if(signature!=signature_){
        u.gtk_list_store_clear(model_);for(const auto& c:choices_){Iter it{};u.gtk_list_store_append(model_,&it);u.gtk_list_store_set(model_,&it,0,c.label.c_str(),1,int(c.available),-1);}signature_=signature;
    }
    int active=-1;for(size_t i=0;i<choices_.size();++i)if(choices_[i].node.name==target){if(active<0||choices_[i].selected)active=int(i);if(choices_[i].selected)break;}
    u.gtk_combo_box_set_active(widget,active);u.gtk_widget_set_sensitive(widget,!choices_.empty());
    if(active>=0)u.gtk_widget_set_tooltip_text(widget,choices_[size_t(active)].label.c_str());syncing_=false;
}
static std::vector<Choice> app_outputs(const State& s){
    std::vector<Choice> out;std::set<std::string> seen;
    for(const auto& c:s.outputs)if(c.selected&&seen.insert(c.node.name).second){Choice p=c;p.device.reset();p.route=-1;p.route_name.clear();p.key=p.node.name;out.push_back(p);}return out;
}
AppsView::AppsView(App& a,bool advanced):app_(a),advanced_(advanced){
    widget=a.box(true,2);empty_=a.label("Os aplicativos aparecem aqui quando reproduzem áudio.","empty",true);a.pack(widget,empty_);a.ui.gtk_widget_set_no_show_all(empty_,1);
}
AppsView::~AppsView(){
    for(auto& kv:rows_){auto& r=kv.second;r->output.reset();r->volume.reset();app_.ui.gtk_widget_destroy(r->widget);}rows_.clear();app_.ui.gtk_widget_destroy(widget);
}
void AppsView::app_menu(Row* row){
    auto& u=app_.ui;app_.close_menu();void* menu=u.gtk_menu_new();u.cls(menu,"rc-menu");
    auto append=[&](const std::string& text,std::function<void()> callback,bool enabled=true){void* item=u.gtk_menu_item_new_with_label(text.c_str());u.gtk_menu_shell_append(menu,item);u.gtk_widget_set_sensitive(item,enabled);
        auto* f=new std::function<void()>(std::move(callback));u.g_signal_connect_data(item,"activate",reinterpret_cast<void(*)()>(+[](void*,void* data){try{(*static_cast<std::function<void()>*>(data))();}catch(const std::exception& e){std::cerr<<e.what()<<'\n';}}),f,+[](void* data,void*){delete static_cast<std::function<void()>*>(data);},0);};
    std::vector<Ref> refs;for(const auto& n:row->group.nodes)refs.push_back(n.ref);
    append(row->group.label+" · saída de áudio",[](){},false);
    append("Acompanhar saída padrão",[this,refs](){Action a;a.kind=Action::FollowDefault;a.refs=refs;app_.act(a);});
    for(const auto& c:app_outputs(app_.state()))append(c.label,[this,refs,c](){Action a;a.kind=Action::Choose;a.choice=c;a.refs=refs;app_.act(a);},c.available);
    append(std::to_string(refs.size())+" fluxo(s) neste grupo",[](){},false);app_.show_menu(menu);
}
void AppsView::update(const State& state){
    auto groups=state.apps();std::set<std::string> keys;for(const auto& g:groups)keys.insert(g.key);
    auto& u=app_.ui;
    for(auto it=rows_.begin();it!=rows_.end();)if(!keys.count(it->first)){
        it->second->output.reset();it->second->volume.reset();u.gtk_widget_destroy(it->second->widget);it=rows_.erase(it);
    }else ++it;
    u.gtk_widget_set_visible(empty_,groups.empty());int order=1;
    for(const auto& group:groups){
        if(!rows_.count(group.key)){
            auto row=std::make_unique<Row>();Row* r=row.get();r->widget=app_.box(true,4);u.cls(r->widget,"app-row");void* line=app_.box(false,8);app_.pack(r->widget,line);
            std::string icon=group.icon;
            if(icon=="application-x-executable-symbolic"){
                if(group.label=="Firefox")icon="firefox";else if(group.label=="Telegram")icon="telegram";else if(group.label=="VLC")icon="vlc";else if(group.label=="Spotify")icon="spotify";
            }
            r->image=u.gtk_image_new_from_icon_name(icon.c_str(),1);
            if(!u.gtk_icon_theme_has_icon(u.gtk_icon_theme_get_default(),icon.c_str()))u.gtk_image_set_from_file(r->image,(folder()/"icones/aplicativo.svg").c_str());
            u.gtk_image_set_pixel_size(r->image,20);app_.pack(line,r->image);
            r->volume=std::make_unique<VolumeControl>(app_,group.label,false,true);app_.pack(line,r->volume->widget,true);
            r->more=app_.text_button("⋮",[this,r](){app_menu(r);},"more");u.gtk_widget_set_tooltip_text(r->more,"Escolher saída deste aplicativo");app_.pack(line,r->more);
            if(advanced_){
                r->output=std::make_unique<Selector>(app_,[this,r](const Choice& c){Action a;a.kind=Action::Choose;a.choice=c;for(const auto& n:r->group.nodes)a.refs.push_back(n.ref);app_.act(a);});
                app_.pack(r->widget,r->output->widget);
            }
            app_.pack(widget,r->widget);u.gtk_widget_show_all(r->widget);rows_[group.key]=std::move(row);
        }
        Row& r=*rows_.at(group.key);r.group=group;r.volume->update(group.nodes);u.gtk_box_reorder_child(widget,r.widget,order++);
        std::string tip;for(const auto& n:group.nodes){if(!tip.empty())tip+='\n';tip+=prop(n.props,"media.name",n.label);}
        u.gtk_widget_set_tooltip_text(r.widget,tip.c_str());
        if(r.output)r.output->update(app_outputs(state),group.nodes[0].output.empty()?prop(group.nodes[0].props,"target.object"):group.nodes[0].output);
    }
}
}

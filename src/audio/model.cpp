#include "model.hpp"
#include <iomanip>
#include <sstream>

namespace rc {
static bool ends(const std::string& s,const std::string& suffix){return s.size()>=suffix.size()&&s.compare(s.size()-suffix.size(),suffix.size(),suffix)==0;}
static const std::set<std::string> kept={
    "object.serial","media.class","node.name","node.description","node.nick",
    "application.name","application.process.id","application.icon-name","client.id","media.name",
    "device.id","device.name","device.api","device.description","device.profile.name",
    "card.profile.device","target.object","target.node","node.target","node.passive"
};
static void update_props(Props& out,const Json& j){
    for(const auto& kv:j.fields())if(kept.count(kv.first)){
        if(kv.second.null())out.erase(kv.first);else out[kv.first]=kv.second.text();
    }
}
static Ref reference(int id,const Props& p){return {id,prop(p,"object.serial"),prop(p,"node.name",prop(p,"device.name")),prop(p,"media.class")};}
static int as_int(const Props& p,const std::string& key){return Json(prop(p,key)).integer();}
void Level::update(const Json& j){
    if(j.has("volume"))volume=j["volume"].null()?std::nullopt:std::optional<double>(j["volume"].number(1));
    if(j.has("mute"))mute=j["mute"].null()?std::nullopt:std::optional<bool>(j["mute"].boolean());
    if(j.has("channelVolumes")){
        if(j["channelVolumes"].array()){
            double max=0;for(const auto& c:j["channelVolumes"].items())max=std::max(max,c.number());channels=max;
        }else channels.reset();
    }
}
static std::vector<Route> read_routes(const Json& j){
    std::vector<Route> routes;
    for(const auto& v:j.items())if(v.object()&&v["index"].integer()>=0){
        Route r;r.index=v["index"].integer();r.device=v["device"].integer();
        r.name=v["name"].text();r.description=v["description"].text(r.name);
        r.direction=v["direction"].text();r.available=v["available"].text();
        std::transform(r.direction.begin(),r.direction.end(),r.direction.begin(),[](unsigned char c){return char(std::tolower(c));});
        for(const auto& x:v["profiles"].items())r.profiles.push_back(x.integer());
        for(const auto& x:v["devices"].items())r.devices.push_back(x.integer());
        r.level.update(v["props"]);routes.push_back(std::move(r));
    }return routes;
}
bool Graph::update(const Json& e){
    int id=e["id"].integer();if(id<0)return false;
    auto old=types_.find(id);std::string type=e["type"].text(old==types_.end()?"":old->second);
    auto erase=[&](){bool existed=types_.erase(id)>0;nodes.erase(id);devices.erase(id);links.erase(id);metadata_.erase(id);if(existed)++version;return existed;};
    if((e.has("info")&&e["info"].null())||(e.object()&&e.fields().size()==1))return erase();
    const Json& info=e["info"];const Json& props=info["props"];
    if(ends(type,":Node")){
        auto i=nodes.find(id);
        std::string media=props["media.class"].text(i==nodes.end()?"":i->second.ref.kind);
        if(media!="Audio/Sink"&&media!="Audio/Source"&&media!="Audio/Source/Virtual"&&media!="Stream/Output/Audio"&&media!="Stream/Input/Audio")return erase();
        Node before=nodes.count(id)?nodes.at(id):Node{};Node& n=nodes[id];
        if(props.has("object.serial")&&!n.ref.serial.empty()&&props["object.serial"].text()!=n.ref.serial)n=Node{};
        update_props(n.props,props);n.ref=reference(id,n.props);n.device=as_int(n.props,"device.id");n.route_device=as_int(n.props,"card.profile.device");
        n.label=prop(n.props,"node.description",prop(n.props,"node.nick",prop(n.props,"application.name",n.ref.name)));
        if(info.has("state"))n.state=info["state"].text();
        if(info["params"].has("Props")){
            n.level=Level{};for(const auto& j:info["params"]["Props"].items())n.level.update(j);
        }
        types_[id]=type;if(before.ref==n.ref&&before.props==n.props&&before.level==n.level&&before.state==n.state)return false;++version;return true;
    }
    if(ends(type,":Device")){
        auto i=devices.find(id);std::string media=props["media.class"].text(i==devices.end()?"":i->second.ref.kind);
        if(media.find("Audio")==std::string::npos)return erase();
        Device before=devices.count(id)?devices.at(id):Device{};Device& d=devices[id];
        if(props.has("object.serial")&&!d.ref.serial.empty()&&props["object.serial"].text()!=d.ref.serial)d=Device{};
        update_props(d.props,props);d.ref=reference(id,d.props);const Json& p=info["params"];
        if(p.has("Profile")){d.profile=-1;if(!p["Profile"].items().empty())d.profile=p["Profile"].items()[0]["index"].integer();}
        if(p.has("EnumRoute"))d.routes=read_routes(p["EnumRoute"]);
        if(p.has("Route"))d.current=read_routes(p["Route"]);
        types_[id]=type;if(before.ref==d.ref&&before.props==d.props&&before.profile==d.profile&&before.routes==d.routes&&before.current==d.current)return false;++version;return true;
    }
    if(ends(type,":Link")){
        bool existed=links.count(id);Link before=existed?links.at(id):Link{};Link& l=links[id];
        if(info.has("output-node-id"))l.output=info["output-node-id"].integer();
        if(info.has("input-node-id"))l.input=info["input-node-id"].integer();
        if(info.has("state"))l.state=info["state"].text();
        types_[id]=type;if(existed&&before.output==l.output&&before.input==l.input&&before.state==l.state)return false;++version;return true;
    }
    if(ends(type,":Metadata")){
        std::string name=e["props"]["metadata.name"].text(metadata_.count(id)?"default":"");
        if(name!="default")return erase();
        auto& m=metadata_[id];bool changed=old==types_.end();
        for(const auto& j:e["metadata"].items()){
            int subject=j["subject"].integer();if(subject<0)continue;
            if(j["key"].null()){
                for(auto it=m.begin();it!=m.end();)if(it->first.first==subject){it=m.erase(it);changed=true;}else++it;
                continue;
            }
            std::string k=j["key"].text();
            if(k!="default.audio.sink"&&k!="default.audio.source"&&k!="target.object"&&k!="target.node")continue;
            auto key=std::make_pair(subject,k);
            if(j["value"].null()){changed|=m.erase(key)>0;continue;}
            Json v=j["value"];
            if(std::holds_alternative<std::string>(v.v)){
                const auto& s=std::get<std::string>(v.v);
                if(!s.empty()&&s.front()=='{')try{v=Json::parse(s);}catch(const std::exception&){}
            }
            if(!m.count(key)||m[key]!=v){m[key]=std::move(v);changed=true;}
        }
        types_[id]=type;if(changed)++version;return changed;
    }
    return erase();
}
bool Graph::valid(const Ref& r)const {
    if(r.serial.empty())return false;
    auto n=nodes.find(r.id);if(n!=nodes.end())return n->second.ref==r;
    auto d=devices.find(r.id);return d!=devices.end()&&d->second.ref==r;
}
void Graph::validate(const Ref& r)const{if(!valid(r))throw std::runtime_error("O dispositivo ou aplicativo mudou. Tente novamente.");}
State Graph::state()const {
    State s;std::map<std::string,Json> defaults;
    std::map<int,std::string> targets;
    for(const auto& m:metadata_)for(const auto& kv:m.second){
        if(kv.first.first==0)defaults[kv.first.second]=kv.second;
        else if(kv.first.second=="target.object")targets[kv.first.first]=kv.second.text();
    }
    for(const auto& kv:nodes){
        Node n=kv.second;Level l=n.level;
        auto d=devices.find(n.device);
        if(d!=devices.end()&&n.route_device>=0)for(const auto& r:d->second.current)if(r.device==n.route_device){l.overlay(r.level);break;}
        n.volume=l.ui();n.muted=l.mute.value_or(false);
        if(targets.count(n.ref.id))n.props["target.object"]=targets[n.ref.id];
        for(const auto& link:links)if(link.second.output==n.ref.id&&link.second.state!="error"){
            auto sink=nodes.find(link.second.input);
            if(sink!=nodes.end()&&sink->second.ref.kind=="Audio/Sink"){n.output=sink->second.ref.name;break;}
        }
        s.nodes.push_back(std::move(n));
    }
    std::sort(s.nodes.begin(),s.nodes.end(),[](const Node& a,const Node& b){return std::make_pair(a.label,a.ref.serial)<std::make_pair(b.label,b.ref.serial);});
    const std::string default_sink=defaults["default.audio.sink"]["name"].text(),default_source=defaults["default.audio.source"]["name"].text();
    for(const auto& n:s.nodes){
        if(!default_sink.empty()&&n.ref.name==default_sink&&n.ref.kind=="Audio/Sink")s.sink=n;
        if(!default_source.empty()&&n.ref.name==default_source&&n.ref.kind.find("Audio/Source")==0)s.source=n;
        bool output=n.ref.kind=="Audio/Sink";
        if(!output&&n.ref.kind!="Audio/Source"&&n.ref.kind!="Audio/Source/Virtual")continue;
        std::vector<Choice> choices;auto d=devices.find(n.device);
        if(d!=devices.end()){
            const auto& routes=d->second.routes.empty()?d->second.current:d->second.routes;
            for(const auto& r:routes){
                if(r.direction!=(output?"output":"input"))continue;
                if(!r.devices.empty()&&std::find(r.devices.begin(),r.devices.end(),n.route_device)==r.devices.end())continue;
                if(d->second.profile>=0&&!r.profiles.empty()&&std::find(r.profiles.begin(),r.profiles.end(),d->second.profile)==r.profiles.end())continue;
                std::string label=r.description;
                if(r.name.find("headphones")!=std::string::npos)label="Fones P2";
                else if(r.name=="analog-output-speaker")label="Alto-falantes";
                else if(r.name=="analog-input-internal-mic")label="Microfone interno";
                else if(r.name=="analog-input-mic")label="Microfone P2";
                else if(r.name.find("hdmi")==0)label=prop(n.props,"node.nick",r.description)+" · HDMI";
                bool available=r.available!="no",active=false;
                for(const auto& a:d->second.current)if(a.index==r.index&&a.device==n.route_device)active=true;
                if(!available)label+=" (desconectado)";
                choices.push_back({n.ref.name+"|"+r.name,label,r.name,n.ref,d->second.ref,r.index,available,active});
            }
        }
        if(choices.empty())choices.push_back({n.ref.name,n.label,"",n.ref,std::nullopt,-1,true,true});
        auto& into=output?s.outputs:s.inputs;into.insert(into.end(),choices.begin(),choices.end());
    }
    if(s.source){
        for(const auto& n:s.nodes)if(n.ref.kind=="Stream/Input/Audio"&&n.ref.name=="capture."+s.source->ref.name){
            s.filtered=true;std::string target=prop(n.props,"target.object");
            for(const auto& raw:s.nodes)if(raw.ref.kind.find("Audio/Source")==0&&(raw.ref.name==target||raw.ref.serial==target))s.physical=raw;
            if(!s.physical)for(const auto& link:links)if(link.second.input==n.ref.id){
                for(const auto& raw:s.nodes)if(raw.ref.id==link.second.output&&raw.ref.kind.find("Audio/Source")==0)s.physical=raw;
            }
            break;
        }
        if(!s.filtered&&s.source->device>=0)s.physical=s.source;
    }
    return s;
}
std::vector<AppGroup> State::apps()const {
    std::map<std::pair<std::string,std::string>,AppGroup> groups;
    for(const auto& n:nodes)if(n.ref.kind=="Stream/Output/Audio"){
        std::string name=prop(n.props,"application.name",prop(n.props,"node.nick",n.label));
        std::string pid=prop(n.props,"application.process.id",prop(n.props,"client.id",n.ref.serial));
        auto& a=groups[{name,pid}];a.key=name+"\x1f"+pid;a.label=name;
        a.icon=prop(n.props,"application.icon-name","application-x-executable-symbolic");a.nodes.push_back(n);
    }
    std::vector<AppGroup> out;for(auto& g:groups)out.push_back(std::move(g.second));return out;
}
std::string Action::key()const {
    std::string k=std::to_string(int(kind));for(const auto& r:refs)k+=':'+std::to_string(r.id)+':'+r.serial;return k;
}
std::vector<Command> plan(const Graph& g,const Action& a){
    for(const auto& r:a.refs)g.validate(r);
    std::vector<Command> out;
    auto cmd=[&](std::vector<std::string> args,std::vector<Ref> guards){out.push_back({std::move(args),std::move(guards)});};
    if(a.kind==Action::Volume||a.kind==Action::Mute){
        if(!std::isfinite(a.value))throw std::runtime_error("Volume inválido");
        std::ostringstream v;v<<std::fixed<<std::setprecision(2)<<std::clamp(a.value,0.0,100.0)<<'%';
        for(const auto& r:a.refs){
            if(a.kind==Action::Volume)cmd({"wpctl","set-volume",std::to_string(r.id),v.str(),"--limit","1.0"},{r});
            else cmd({"wpctl","set-mute",std::to_string(r.id),a.muted?"1":"0"},{r});
        }return out;
    }
    if(a.kind==Action::FollowDefault){
        for(const auto& r:a.refs)cmd({"pw-metadata","-n","default","-d",std::to_string(r.id),"target.object"},{r});return out;
    }
    const auto& c=a.choice;g.validate(c.node);
    if(!c.available)throw std::runtime_error("Essa porta está desconectada.");
    if(c.device&&c.route>=0){
        g.validate(*c.device);const auto& d=g.devices.at(c.device->id);
        const auto& rs=d.routes.empty()?d.current:d.routes;
        auto r=std::find_if(rs.begin(),rs.end(),[&](const Route& x){return x.index==c.route&&x.name==c.route_name;});
        if(r==rs.end()||r->available=="no")throw std::runtime_error("A porta deixou de estar disponível.");
        bool active=false;for(const auto& current:d.current)if(current.index==c.route&&current.device==g.nodes.at(c.node.id).route_device)active=true;
        if(!active)cmd({"wpctl","set-route",std::to_string(c.device->id),std::to_string(c.route)},{*c.device,c.node});
    }
    if(!a.refs.empty()){
        for(const auto& r:a.refs)cmd({"pw-metadata","-n","default",std::to_string(r.id),"target.object",c.node.name,"Spa:String"},{r,c.node});
    }else if(!a.port_only){
        cmd({"wpctl","set-default",std::to_string(c.node.id)},{c.node});
        if(c.node.kind=="Audio/Sink")for(const auto& kv:g.nodes)if(kv.second.ref.kind=="Stream/Output/Audio"){
            const auto& r=kv.second.ref;g.validate(r);
            cmd({"pw-metadata","-n","default",std::to_string(r.id),"target.object",c.node.name,"Spa:String"},{r,c.node});
        }
    }return out;
}
Graph read_graph(const std::string& text){Graph g;JsonStream f;f.feed(text,[&](const Json& e){g.update(e);});if(!f.empty()||!f.batches)throw std::runtime_error("Dump PipeWire incompleto");return g;}
std::string connection_signature(const State& s){
    std::string stamp;
    for(const auto* node:{&s.sink,&s.source,&s.physical}){
        if(!*node){stamp+="|none";continue;}const auto& r=(*node)->ref;
        stamp+='|'+std::to_string(r.id)+':'+r.serial+':'+r.name+':'+r.kind;
    }
    for(const auto& c:s.outputs)if(s.sink&&c.node==s.sink->ref&&c.selected)stamp+="|out:"+c.key+(c.available?":on":":off");
    const auto& input=s.filtered?s.physical:s.source;
    for(const auto& c:s.inputs)if(input&&c.node==input->ref&&c.selected)stamp+="|in:"+c.key+(c.available?":on":":off");
    return stamp;
}
std::string demo_json(){
    return R"JSON([
      {"id":2,"type":"PipeWire:Interface:Node","info":{"props":{"object.serial":1002,"node.name":"demo-output","node.description":"Fones P2","media.class":"Audio/Sink"},"params":{"Props":[{"channelVolumes":[0.421875],"mute":false}]}}},
      {"id":3,"type":"PipeWire:Interface:Node","info":{"props":{"object.serial":1003,"node.name":"demo-hdmi","node.description":"Monitor · HDMI","media.class":"Audio/Sink"},"params":{"Props":[{"channelVolumes":[0.125],"mute":false}]}}},
      {"id":4,"type":"PipeWire:Interface:Node","info":{"props":{"object.serial":1004,"node.name":"demo-filter","node.description":"Filtro de ruído","media.class":"Audio/Source"},"params":{"Props":[{"channelVolumes":[0.216],"mute":false}]}}},
      {"id":5,"type":"PipeWire:Interface:Node","info":{"props":{"object.serial":1005,"node.name":"demo-mic","node.description":"Microfone interno","media.class":"Audio/Source","device.id":50,"card.profile.device":0},"params":{"Props":[{"channelVolumes":[1],"mute":false}]}}},
      {"id":50,"type":"PipeWire:Interface:Device","info":{"props":{"object.serial":1050,"device.name":"demo-device","media.class":"Audio/Device"},"params":{"Route":[{"index":0,"device":0,"direction":"Input","name":"analog-input-internal-mic","description":"Microfone interno","props":{"channelVolumes":[1],"mute":false}}],"EnumRoute":[{"index":0,"direction":"Input","name":"analog-input-internal-mic","description":"Microfone interno","devices":[0]}]}}},
      {"id":10,"type":"PipeWire:Interface:Node","info":{"props":{"object.serial":1010,"node.name":"capture.demo-filter","target.object":"demo-mic","media.class":"Stream/Input/Audio"}}},
      {"id":6,"type":"PipeWire:Interface:Node","info":{"props":{"object.serial":1006,"node.name":"firefox","application.name":"Firefox","application.process.id":200,"application.icon-name":"firefox","media.class":"Stream/Output/Audio"},"params":{"Props":[{"channelVolumes":[0.512]}]}}},
      {"id":7,"type":"PipeWire:Interface:Node","info":{"props":{"object.serial":1007,"node.name":"telegram","application.name":"Telegram","application.process.id":201,"application.icon-name":"telegram","media.class":"Stream/Output/Audio"},"params":{"Props":[{"channelVolumes":[0.216]}]}}},
      {"id":8,"type":"PipeWire:Interface:Node","info":{"props":{"object.serial":1008,"node.name":"vlc","application.name":"VLC","application.process.id":202,"application.icon-name":"vlc","media.class":"Stream/Output/Audio"},"params":{"Props":[{"channelVolumes":[0.064]}]}}},
      {"id":9,"type":"PipeWire:Interface:Node","info":{"props":{"object.serial":1009,"node.name":"spotify","application.name":"Spotify","application.process.id":203,"application.icon-name":"spotify","media.class":"Stream/Output/Audio"},"params":{"Props":[{"channelVolumes":[0.729]}]}}},
      {"id":44,"type":"PipeWire:Interface:Metadata","props":{"metadata.name":"default"},"metadata":[{"subject":0,"key":"default.audio.sink","value":{"name":"demo-output"}},{"subject":0,"key":"default.audio.source","value":{"name":"demo-filter"}}]}
    ])JSON";
}
}

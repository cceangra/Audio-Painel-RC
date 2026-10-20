#pragma once
#include "json.hpp"
#include <optional>
#include <set>

namespace rc {
using Props=std::map<std::string,std::string>;
inline std::string prop(const Props& p,const std::string& k,const std::string& fallback=""){
    auto i=p.find(k);return i==p.end()?fallback:i->second;
}
struct Ref {
    int id=-1;std::string serial,name,kind;
    bool operator==(const Ref& x)const{return id==x.id&&serial==x.serial&&name==x.name&&kind==x.kind;}
    bool operator!=(const Ref& x)const{return !(*this==x);}
};
struct Level {
    std::optional<double> volume,channels;
    std::optional<bool> mute;
    void update(const Json& j);
    void overlay(const Level& l){if(l.volume)volume=l.volume;if(l.channels)channels=l.channels;if(l.mute)mute=l.mute;}
    double ui()const{return std::cbrt(std::max(0.0,channels.value_or(volume.value_or(1.0))));}
    bool operator==(const Level& x)const{return volume==x.volume&&channels==x.channels&&mute==x.mute;}
};
struct Route {
    int index=-1,device=-1;
    std::string name,description,direction,available;
    std::vector<int> profiles,devices;
    Level level;
    bool operator==(const Route& x)const{return index==x.index&&device==x.device&&name==x.name&&description==x.description&&direction==x.direction&&available==x.available&&profiles==x.profiles&&devices==x.devices&&level==x.level;}
};
struct Node {
    Ref ref;Props props;Level level;
    std::string label,state,output;
    int device=-1,route_device=-1;
    double volume=1;bool muted=false;
};
struct Device {
    Ref ref;Props props;int profile=-1;
    std::vector<Route> routes,current;
};
struct Link {int output=-1,input=-1;std::string state;};
struct Choice {
    std::string key,label,route_name;
    Ref node;std::optional<Ref> device;
    int route=-1;bool available=true,selected=true;
};
struct AppGroup {std::string key,label,icon;std::vector<Node> nodes;};
struct State {
    std::vector<Node> nodes;
    std::vector<Choice> outputs,inputs;
    std::optional<Node> sink,source,physical;
    bool filtered=false;
    std::string error;
    std::vector<AppGroup> apps()const;
};
class Graph {
    std::map<int,std::string> types_;
    std::map<int,std::map<std::pair<int,std::string>,Json>> metadata_;
public:
    std::map<int,Node> nodes;
    std::map<int,Device> devices;
    std::map<int,Link> links;
    uint64_t version=0;
    bool update(const Json& event);
    bool valid(const Ref& ref)const;
    void validate(const Ref& ref)const;
    State state()const;
    size_t size()const{return nodes.size()+devices.size()+links.size()+metadata_.size();}
    void clear(){*this=Graph{};}
};
struct Action {
    enum Kind {Volume,Mute,Choose,FollowDefault} kind=Volume;
    std::vector<Ref> refs;
    Choice choice;
    double value=0;
    bool muted=false,port_only=false;
    std::string key()const;
};
struct Command {std::vector<std::string> args;std::vector<Ref> guards;};
std::vector<Command> plan(const Graph&,const Action&);
Graph read_graph(const std::string&);
std::string connection_signature(const State&);
std::string demo_json();
}

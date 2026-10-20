#include "process.hpp"
#include <cerrno>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
namespace rc {
Child::Child(LoopApi& l,const std::vector<std::string>& args,bool merge,unsigned timeout_ms,
    std::function<void(std::string_view)> data,std::function<void(int,const std::string&)> done,bool keep_alive):loop_(l),data_(std::move(data)),done_(std::move(done)){
    pid_=spawn_pipe(args,fd_,merge,!keep_alive);
    io_=loop_.g_unix_fd_add(fd_,1|8|16,+[](int,int condition,void* p)->int{
        auto* c=static_cast<Child*>(p);c->drain();
        if(condition&(8|16|32)){c->io_=0;return 0;}return 1;
    },this);
    watch_=loop_.g_child_watch_add(pid_,+[](int,int status,void* p){auto* c=static_cast<Child*>(p);c->watch_=0;c->ended(status);},this);
    if(timeout_ms)timeout_=loop_.g_timeout_add(timeout_ms,+[](void* p)->int{auto* c=static_cast<Child*>(p);c->timeout_=0;c->timedout_=true;c->terminate();return 0;},this);
}
void Child::drain(){
    if(fd_<0)return;char buf[32768];ssize_t n;
    while((n=read(fd_,buf,sizeof buf))>0){
        if(error_.empty())try{data_(std::string_view(buf,size_t(n)));}catch(const std::exception& e){error_=e.what();terminate();}
    }
}
void Child::ended(int status){
    pid_=-1;drain();loop_.cancel(io_);loop_.cancel(timeout_);if(fd_>=0){close(fd_);fd_=-1;}
    status_=WIFEXITED(status)?WEXITSTATUS(status):128+(WIFSIGNALED(status)?WTERMSIG(status):0);
    if(timedout_)error_="O comando excedeu o tempo limite.";
    finish_=loop_.later([this](){finish_=0;auto fn=std::move(done_);int st=status_;std::string e=error_;fn(st,e);});
}
void Child::terminate(){if(pid_>0)kill(pid_,SIGKILL);}
void Child::detach(){
    loop_.cancel(io_);loop_.cancel(watch_);loop_.cancel(timeout_);loop_.cancel(finish_);
    if(fd_>=0){close(fd_);fd_=-1;}pid_=-1;data_={};done_={};
}
Child::~Child(){
    loop_.cancel(io_);loop_.cancel(watch_);loop_.cancel(timeout_);loop_.cancel(finish_);
    if(pid_>0){kill(pid_,SIGKILL);int status;while(waitpid(pid_,&status,0)<0&&errno==EINTR){}}
    if(fd_>=0)close(fd_);
}
Backend::Backend(LoopApi& l,bool demo,std::function<void()> changed,std::function<void(const std::string&)> finished):loop_(l),demo_(demo),changed_(std::move(changed)),finished_(std::move(finished)){}
Backend::~Backend(){closing_=true;loop_.cancel(retry_);loop_.cancel(notify_);command_.reset();monitor_.reset();}
void Backend::notify(){
    if(notify_||closing_)return;
    notify_=loop_.later([this](){notify_=0;std::string e=state.error;state=graph.state();state.error=e;changed_();},80);
}
void Backend::start(){if(demo_){graph=read_graph(demo_json());state=graph.state();notify();}else start_monitor();}
void Backend::start_monitor(){
    if(closing_)return;stream_.reset();got_batch_=false;graph.clear();state=State{};state.error="Conectando ao PipeWire…";notify();
    try{
        monitor_=std::make_unique<Child>(loop_,std::vector<std::string>{"pw-dump","-m","-N"},false,0,
            [this](std::string_view chunk){uint64_t version=graph.version;size_t batches=stream_.batches;
                stream_.feed(chunk,[this](const Json& e){graph.update(e);});
                if(stream_.batches>batches){got_batch_=true;backoff_=1;bool had_error=!state.error.empty();state.error.clear();if(graph.version!=version||had_error)notify();}
                else if(graph.version!=version&&got_batch_)notify();
            },
            [this](int status,const std::string& e){
                monitor_.reset();graph.clear();state=State{};state.error=e.empty()?"PipeWire desconectado (código "+std::to_string(status)+"). Tentando reconectar…":e;notify();
                retry_=loop_.later([this](){retry_=0;start_monitor();},unsigned(backoff_*1000));backoff_=std::min(8,backoff_*2);
            });
    }catch(const std::exception& e){state.error=e.what();notify();retry_=loop_.later([this](){retry_=0;start_monitor();},unsigned(backoff_*1000));backoff_=std::min(8,backoff_*2);}
}
void Backend::submit(Action a){
    if(closing_)return;
    if(demo_){
        try{
            plan(graph,a);
            if(a.kind==Action::Volume||a.kind==Action::Mute)for(const auto& r:a.refs){
                auto& n=graph.nodes.at(r.id);if(a.kind==Action::Volume){double v=std::clamp(a.value/100.0,0.0,1.0);n.level.channels=v*v*v;}
                else n.level.mute=a.muted;
                auto d=graph.devices.find(n.device);if(d!=graph.devices.end())for(auto& route:d->second.current)if(route.device==n.route_device){if(a.kind==Action::Volume)route.level.channels=n.level.channels;else route.level.mute=a.muted;}
            }
            else if(a.kind==Action::Choose&&!a.port_only&&a.refs.empty()){
                Json e=Json::parse("{\"id\":44,\"metadata\":[]}");Json j=Json::Object{{"subject",0},{"key",a.choice.node.kind=="Audio/Sink"?"default.audio.sink":"default.audio.source"},{"value",Json::Object{{"name",a.choice.node.name}}}};
                e["metadata"]=Json::Array{j};graph.update(e);
            }else if(a.kind==Action::Choose)for(const auto& r:a.refs)graph.nodes[r.id].props["target.object"]=a.choice.node.name;
            ++graph.version;state=graph.state();notify();finished_("");
        }catch(const std::exception& e){finished_(e.what());}return;
    }
    if(a.kind==Action::Volume){auto key=a.key();for(auto it=queue_.begin();it!=queue_.end();)if(it->kind==Action::Volume&&it->key()==key)it=queue_.erase(it);else ++it;}
    if(queue_.size()>=32){finished_("Aguarde os comandos pendentes.");return;}queue_.push_back(std::move(a));next_action();
}
void Backend::next_action(){
    if(busy_||queue_.empty()||closing_)return;busy_=true;active_=std::move(queue_.front());queue_.pop_front();fresh_.clear();fresh_stream_.reset();command_output_.clear();
    try{
        command_=std::make_unique<Child>(loop_,std::vector<std::string>{"pw-dump","-N"},false,5000,
            [this](std::string_view s){fresh_stream_.feed(s,[this](const Json& e){fresh_.update(e);});},
            [this](int status,const std::string& err){
                command_.reset();
                if(status||!err.empty()||!fresh_stream_.empty()||!fresh_stream_.batches){finish_action(err.empty()?"Não foi possível validar o estado atual do PipeWire.":err);return;}
                try{auto p=plan(fresh_,active_);commands_=std::deque<Command>(std::make_move_iterator(p.begin()),std::make_move_iterator(p.end()));next_command();}
                catch(const std::exception& e){finish_action(e.what());}
            });
    }catch(const std::exception& e){finish_action(e.what());}
}
void Backend::next_command(){
    if(commands_.empty()){finish_action("");return;}
    Command c=std::move(commands_.front());commands_.pop_front();
    try{
        // Não reaproveitar IDs removidos/recriados enquanto o usuário arrasta
        // um controle ou enquanto um grupo de comandos aguarda na fila.
        for(const auto& ref:c.guards){fresh_.validate(ref);graph.validate(ref);}
        command_output_.clear();
        command_=std::make_unique<Child>(loop_,c.args,true,5000,[this](std::string_view s){if(command_output_.size()+s.size()>65536)throw std::runtime_error("Comando retornou saída excessiva");command_output_.append(s);},
            [this](int status,const std::string& e){command_.reset();if(status||!e.empty())finish_action(e.empty()?"Comando de áudio falhou: "+command_output_:e);else next_command();});
    }catch(const std::exception& e){finish_action(e.what());}
}
void Backend::finish_action(const std::string& e){
    commands_.clear();fresh_.clear();fresh_stream_.reset();command_output_.clear();busy_=false;finished_(e);next_action();
}
}

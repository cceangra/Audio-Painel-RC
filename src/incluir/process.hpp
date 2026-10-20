#pragma once
#include "gtk_api.hpp"
#include <memory>
namespace rc {
class Child {
    LoopApi& loop_;int fd_=-1;pid_t pid_=-1;
    unsigned io_=0,watch_=0,timeout_=0,finish_=0;
    bool timedout_=false;std::string error_;int status_=-1;
    std::function<void(std::string_view)> data_;
    std::function<void(int,const std::string&)> done_;
    void drain();void ended(int);
public:
    Child(LoopApi&,const std::vector<std::string>&,bool merge_stderr,unsigned timeout_ms,
          std::function<void(std::string_view)>,std::function<void(int,const std::string&)>,bool keep_alive=false);
    ~Child();
    Child(const Child&)=delete;Child& operator=(const Child&)=delete;
    void terminate();
    void detach(); // A integração explícita deve concluir mesmo se a central fechar.
};
class Backend {
    LoopApi& loop_;bool demo_=false,closing_=false;
    std::unique_ptr<Child> monitor_,command_;
    JsonStream stream_,fresh_stream_;Graph fresh_;
    unsigned retry_=0,notify_=0;
    int backoff_=1;
    bool busy_=false,got_batch_=false;
    Action active_;std::deque<Action> queue_;
    std::deque<Command> commands_;
    std::string command_output_;
    std::function<void()> changed_;
    std::function<void(const std::string&)> finished_;
    void start_monitor();void notify();void next_action();void next_command();void finish_action(const std::string&);
public:
    Graph graph;State state;
    Backend(LoopApi&,bool demo,std::function<void()>,std::function<void(const std::string&)>);
    ~Backend();
    void start();void submit(Action);
    bool busy()const{return busy_;}
    bool demo()const{return demo_;}
};
}

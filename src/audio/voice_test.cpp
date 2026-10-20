#include "voice_test.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
extern char** environ;
namespace rc {
std::string VoiceDelay::process(std::string_view raw){
    std::string out(raw.size(),'\0');double max=0;
    for(size_t k=0;k<raw.size();++k){
        unsigned char c=static_cast<unsigned char>(raw[k]);out[k]=char(ring_[cursor_]);ring_[cursor_]=c;cursor_=(cursor_+1)%ring_.size();
        if(!half_){low_=c;half_=true;}else{int16_t sample=int16_t(uint16_t(low_)|(uint16_t(c)<<8));max=std::max(max,std::abs(int(sample))/32768.0);half_=false;}
    }
    peak=max;return out;
}
void VoiceTest::start(const Ref& input,const Ref& output){
    stop();source=input;sink=output;running=true;
    // A validação usa a mesma identidade (id + serial + nome + classe) dos controles.
    auto fresh=std::make_shared<Graph>();auto parser=std::make_shared<JsonStream>();
    try{validation_=std::make_unique<Child>(loop_,std::vector<std::string>{"pw-dump","-N"},false,5000,
        [fresh,parser](std::string_view s){parser->feed(s,[&](const Json& e){fresh->update(e);});},
        [this,fresh,parser](int status,const std::string& error){
            validation_.reset();
            try{if(status||!error.empty()||!parser->empty()||!parser->batches)throw std::runtime_error(error.empty()?"Não foi possível validar o áudio para o teste.":error);
                fresh->validate(source);fresh->validate(sink);launch();
            }catch(const std::exception& e){fail(e.what());}
        });
    }catch(const std::exception& e){fail(e.what());}
}
void VoiceTest::launch(){
    int p[2];if(pipe2(p,O_CLOEXEC))throw std::runtime_error("Falha no pipe do retorno de voz");
    std::vector<std::string> args={"pw-cat","--playback","--raw","--format","s16","--rate","48000","--channels","1","--latency","40ms","--target",sink.name,"-"};
    posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions,p[0],STDIN_FILENO);posix_spawn_file_actions_addclose(&actions,p[1]);posix_spawn_file_actions_addclose(&actions,p[0]);
    posix_spawn_file_actions_addopen(&actions,STDOUT_FILENO,"/dev/null",O_WRONLY,0);posix_spawn_file_actions_addopen(&actions,STDERR_FILENO,"/dev/null",O_WRONLY,0);
    auto execargs=child_arguments(args);std::vector<char*> argv;for(auto& a:execargs)argv.push_back(a.data());argv.push_back(nullptr);
    int error=posix_spawnp(&playback_,execargs[0].c_str(),&actions,nullptr,argv.data(),environ);posix_spawn_file_actions_destroy(&actions);close(p[0]);
    if(error){close(p[1]);playback_=-1;throw std::runtime_error("pw-cat não iniciou a reprodução: "+std::string(strerror(error)));}
    playback_fd_=p[1];fcntl(playback_fd_,F_SETFL,fcntl(playback_fd_,F_GETFL)|O_NONBLOCK);
    playback_watch_=loop_.g_child_watch_add(playback_,+[](int,int status,void* data){auto* v=static_cast<VoiceTest*>(data);v->playback_watch_=0;v->playback_=-1;v->fail("O retorno de voz foi encerrado (código "+std::to_string(WIFEXITED(status)?WEXITSTATUS(status):-1)+").");},this);
    capture_=std::make_unique<Child>(loop_,std::vector<std::string>{"pw-cat","--record","--raw","--format","s16","--rate","48000","--channels","1","--latency","40ms","--target",source.name,"-"},false,0,
        [this](std::string_view data){
            if(pending_.size()-pending_offset_+data.size()>96000)throw std::runtime_error("Retorno interrompido: a saída de áudio não acompanhou a captura.");
            if(pending_offset_){pending_.erase(0,pending_offset_);pending_offset_=0;}
            pending_+=delay_.process(data);write_pending();
        },
        [this](int status,const std::string& error){fail(error.empty()?"A captura do teste foi encerrada (código "+std::to_string(status)+").":error);});
    limit_=loop_.later([this](){limit_=0;fail("Teste concluído após 30 segundos.");},30000);
}
void VoiceTest::write_pending(){
    if(playback_fd_<0)return;
    while(pending_offset_<pending_.size()){
        ssize_t n=write(playback_fd_,pending_.data()+pending_offset_,pending_.size()-pending_offset_);
        if(n>0){pending_offset_+=size_t(n);continue;}if(n<0&&errno==EINTR)continue;
        if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK))break;
        fail("Não foi possível enviar a voz para a saída selecionada.");return;
    }
    if(pending_offset_==pending_.size()){std::fill(pending_.begin(),pending_.end(),0);pending_.clear();pending_offset_=0;}
    if(!pending_.empty()&&!writable_)writable_=loop_.g_unix_fd_add(playback_fd_,4|8|16,+[](int,int condition,void* data)->int{
        auto* v=static_cast<VoiceTest*>(data);if(condition&(8|16|32)){v->writable_=0;v->fail("A saída do retorno de voz foi desconectada.");return 0;}
        v->write_pending();if(v->pending_.empty()){v->writable_=0;return 0;}return 1;
    },this);
}
void VoiceTest::fail(const std::string& text){
    if(finish_||!running)return;
    // Encerrar fora da callback que está lendo a captura evita destruir a
    // instância Child enquanto ela está na pilha de execução.
    finish_=loop_.later([this,text](){finish_=0;stop();stopped_(text);});
}
void VoiceTest::stop(){
    running=false;loop_.cancel(finish_);loop_.cancel(limit_);loop_.cancel(writable_);loop_.cancel(playback_watch_);
    validation_.reset();capture_.reset();
    if(playback_fd_>=0){close(playback_fd_);playback_fd_=-1;}
    if(playback_>0){kill(playback_,SIGKILL);int status;while(waitpid(playback_,&status,0)<0&&errno==EINTR){}playback_=-1;}
    std::fill(pending_.begin(),pending_.end(),0);pending_.clear();pending_offset_=0;delay_.clear();
}
}

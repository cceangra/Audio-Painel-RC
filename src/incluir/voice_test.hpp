#pragma once
#include "process.hpp"
namespace rc {
// Buffer PCM mono 48 kHz / s16: 96000 bytes = exatamente um segundo.
// O dispositivo/servidor adiciona sua própria latência a esse atraso.
class VoiceDelay {
    std::vector<unsigned char> ring_=std::vector<unsigned char>(96000,0);
    size_t cursor_=0;
    bool half_=false;unsigned char low_=0;
public:
    double peak=0;
    std::string process(std::string_view);
    void clear(){std::fill(ring_.begin(),ring_.end(),0);cursor_=0;peak=0;half_=false;low_=0;}
    size_t bytes()const{return ring_.size();}
};
class VoiceTest {
    LoopApi& loop_;std::unique_ptr<Child> capture_,validation_;
    pid_t playback_=-1;int playback_fd_=-1;
    unsigned playback_watch_=0,writable_=0,limit_=0,finish_=0;
    std::string pending_;size_t pending_offset_=0;
    VoiceDelay delay_;
    std::function<void(const std::string&)> stopped_;
    void write_pending();void fail(const std::string&);void launch();
public:
    bool running=false;
    Ref source,sink;
    explicit VoiceTest(LoopApi& l,std::function<void(const std::string&)> cb):loop_(l),stopped_(std::move(cb)){}
    ~VoiceTest(){stop();}
    void start(const Ref& source,const Ref& sink);
    void stop();
    double peak()const{return delay_.peak;}
};
}

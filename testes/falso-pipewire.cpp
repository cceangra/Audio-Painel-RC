// Simulador para testar pipes, timeouts e PCM. Nunca acessa áudio real.
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <unistd.h>
static std::string read_file(const std::string& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){
    const char* root=getenv("RC_AUDIO_TEST_DIR");if(!root)return 2;std::string dir(root);
    std::string name=argv[0];name=name.substr(name.find_last_of('/')+1);std::string line=name;
    for(int n=1;n<argc;++n)line+=' '+std::string(argv[n]);line+='\n';
    int log=open((dir+"/comandos.txt").c_str(),O_WRONLY|O_CREAT|O_APPEND,0600);if(log>=0){ssize_t written=write(log,line.data(),line.size());close(log);if(written!=ssize_t(line.size()))return 4;}
    auto has=[&](const std::string& a){for(int n=1;n<argc;++n)if(argv[n]==a)return true;return false;};
    if(name=="pw-dump"){
        std::cout<<read_file(dir+"/snapshot.json")<<std::flush;
        if(has("-m")){std::string previous;while(true){std::string event=read_file(dir+"/evento.json");if(!event.empty()&&event!=previous){previous=event;std::cout<<event<<std::flush;}std::this_thread::sleep_for(std::chrono::milliseconds(20));}}
        return 0;
    }
    if(name=="pw-cat"){
        if(has("--record")){
            unsigned long sample=0;while(true){std::string block;block.resize(960);
                for(int n=0;n<480;++n,++sample){int16_t v=int16_t(20000*std::sin(sample*0.05));block[size_t(2*n)]=char(v&255);block[size_t(2*n+1)]=char((uint16_t(v)>>8)&255);}
                std::cout.write(block.data(),block.size());std::cout.flush();if(!std::cout)return 0;std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        if(has("--playback")){
            char buffer[4096];ssize_t n;long total=0,first=-1;
            while((n=read(0,buffer,sizeof buffer))>0){for(ssize_t k=0;k<n;++k)if(first<0&&buffer[k])first=total+k;total+=n;
                std::ofstream f(dir+"/pcm-contagem.txt");f<<total<<' '<<first<<'\n';
            }return 0;
        }
    }
    return name=="wpctl"||name=="pw-metadata"?0:3;
}

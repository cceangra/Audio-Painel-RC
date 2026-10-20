#pragma once
#include "model.hpp"
#include <filesystem>
#include <chrono>
#include <deque>
#include <iosfwd>
#include <sys/types.h>
namespace rc {
namespace fs=std::filesystem;
inline constexpr const char* VERSION="0.3.0";
inline constexpr const char* APP_ID="audio-painel-rc-cpp-v2";
fs::path folder();
int proc_self_pid();
int signal_pid(int proc_id);
std::string read_file(const fs::path&,size_t limit=16*1024*1024);
void atomic_write(const fs::path&,const std::string&,unsigned mode=0600);
struct Config {
    int layout=4,width=560,font=12,radius=14,rows=4,hide_delay=350,margin=12,gap=3,release_seconds=20;
    double opacity=0.94;
    std::string theme="dark",accent="#91a8ff",density="normal";
    bool hide_leave=true,animation=true;
    Json original=Json::Object{};
    static Config load();
    static Config from_json(const Json&);
    Json json()const;
};
void preference(const std::string&,const Json&);
struct Result {int status=-1;bool timeout=false;std::string text;};
std::vector<std::string> child_arguments(const std::vector<std::string>&,bool die_with_parent=true);
pid_t spawn_pipe(const std::vector<std::string>&,int& fd,bool merge_stderr=true,bool die_with_parent=true);
Result run_read(const std::vector<std::string>&,int timeout_ms=5000,size_t limit=16*1024*1024);
void spawn_settings(bool demo);
std::string endpoint(const std::string& role);
Json ipc(const std::string& role,const std::string& action,int timeout_ms=250);
int claim(const std::string& role);
std::string autostart_path();
bool autostart_enabled();
void autostart(bool enable);
int diagnostics();
int uninstall(bool yes,std::ostream* report=nullptr);
int memory_meter(int samples,int interval_ms,int wait_ms,const std::string& label);
struct Rect {int x=0,y=0,width=0,height=0;};
struct Point {int x=0,y=0;};
Rect anchor(Rect area,Rect icon,bool have_icon,int orientation,int w,int h,int gap);
bool in_area(Rect r,Point p,int margin=0);
bool pointer_safe(Rect panel,Rect icon,Point p,int margin,bool have_icon);
int tests();
int tests_io();
}

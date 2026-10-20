#pragma once
#include "gtk_api.hpp"
#include "environment.hpp"
#include <memory>
namespace rc {
struct KeyBinding {std::string action;unsigned keyval=0,modifiers=0;bool confirmed=false;};
std::vector<KeyBinding> key_bindings(const Json&);
Json keyboard_json(const std::vector<KeyBinding>&,const std::string& mode,int step);
bool valid_shortcut(unsigned keyval,unsigned modifiers);
struct KeyProbe {bool supported=false,available=false;std::string message;};
KeyProbe probe_shortcuts(const std::vector<KeyBinding>&);
int keyboard_x11_tests();
class KeyboardRuntime {
    struct Impl;std::unique_ptr<Impl> impl_;
public:
    KeyboardRuntime(LoopApi&,const Json&,std::function<void(const std::string&,int)>,std::function<void(const std::string&)>);
    ~KeyboardRuntime();
};
}

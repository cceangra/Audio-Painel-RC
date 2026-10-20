#pragma once
#include "platform.hpp"
namespace rc {
std::optional<std::string> root_value(const std::string&,const std::string&);
std::string change_root_value(const std::string&,const std::string&,const std::optional<std::string>&);
void integrate_kmix();
void restore_kmix(bool start=true);
void spawn_tray();
}

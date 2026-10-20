#pragma once
#include "platform.hpp"
namespace rc {
std::string env_value(const char*, const std::string& fallback="");
bool program_exists(const std::string&);
struct Environment {
    std::string distro,architecture,desktop,session,backend,audio_note;
    bool trinity=false,wayland=false,audio_ready=false;
    std::vector<std::string> volume_agents;
    std::string text()const;
};
Environment detect_environment(bool probe_audio=true);
fs::path startup_location();
fs::path menu_location();
fs::path desktop_location();
std::vector<std::string> keyboard_settings_command();
void launch_detached(const fs::path& executable,const std::vector<std::string>& args={});
}

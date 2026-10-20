#pragma once
#include "environment.hpp"
#include <atomic>
namespace rc {
std::string sha256_file(const fs::path&);
std::string sha256_text(const std::string&);
bool safe_relative_path(const fs::path&);
void reject_links(const fs::path&);
std::string shortcut_text(const fs::path& root,const std::string& kind,bool trinity);
struct ExternalSpec {fs::path path;std::string kind,content;};
struct InstallOptions {
    fs::path source,destination,archive;
    bool gather_source=true,gather_extras=true,startup=false,menu=false,desktop=false;
    Json keyboard=Json::Object{};
    bool keyboard_changed=false;
};
struct InstallPlan {
    InstallOptions options;
    std::vector<fs::path> files,extras;
    std::vector<ExternalSpec> external;
    uintmax_t bytes=0;
    bool inplace=false,update=false,pending_recovery=false;
    std::string summary()const;
};
struct InstallResult {fs::path root;std::vector<std::string> warnings;std::string report;};
InstallPlan plan_install(const InstallOptions&);
InstallResult perform_install(const InstallPlan&,std::atomic<bool>& cancel,const std::function<void(double,const std::string&)>& progress={});
void recover_installation(const fs::path&);
bool installed_here(const fs::path& root=folder());
std::string external_report(const fs::path& root=folder());
void write_external_shortcut(const fs::path& root,const ExternalSpec&);
void remove_external_shortcut(const fs::path& root,const std::string& kind);
void replace_registered_external(const fs::path& path,const fs::path& temporary,const std::string& text,unsigned mode);
void clear_registered_temporaries(const fs::path& path,const Json& record);
// Recusa apagar integrações editadas: o registro e os backups são preservados.
void remove_recorded_external(const fs::path& root,std::ostream&);
std::vector<fs::path> removal_manifest(const fs::path& root);
fs::path find_release_archive(const fs::path& root);
int installation_tests();
}

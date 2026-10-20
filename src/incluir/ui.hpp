#pragma once
#include "process.hpp"
#include "voice_test.hpp"
#include "keyboard.hpp"
namespace rc {
class App;
class VolumeControl {
    App& app_;bool mic_,inline_,sync_=false,drag_=false;
    unsigned pending_=0;
    std::vector<Node> nodes_;
    Action delayed_;
    int last_mute_=-1;
    void change();void mute();
public:
    void* widget=nullptr;void* title=nullptr;void* scale=nullptr;void* percent=nullptr;void* mute_button=nullptr;void* image=nullptr;
    VolumeControl(App&,const std::string&,bool mic=false,bool inline_row=false);
    ~VolumeControl();
    void update(const std::vector<Node>&);
    bool dragging()const{return drag_;}
};
class Selector {
    App& app_;void* model_;bool syncing_=false;
    std::vector<Choice> choices_;std::string signature_;
    std::function<void(const Choice&)> callback_;
public:
    void* widget;
    Selector(App&,std::function<void(const Choice&)>);
    ~Selector();
    void update(std::vector<Choice>,const std::string& target);
};
class AppsView {
    struct Row {void* widget=nullptr;void* image=nullptr;void* more=nullptr;std::unique_ptr<VolumeControl> volume;std::unique_ptr<Selector> output;AppGroup group;};
    App& app_;bool advanced_;
    void* empty_;std::map<std::string,std::unique_ptr<Row>> rows_;
    void app_menu(Row*);
public:
    void* widget;
    AppsView(App&,bool advanced=false);~AppsView();
    void update(const State&);
};
class Popup {
    App& app_;
    std::unique_ptr<VolumeControl> master_,mic_;
    std::unique_ptr<Selector> output_,input_;
    std::unique_ptr<AppsView> apps_;
    void* mic_note_=nullptr;void* enable_=nullptr;void* error_=nullptr;
    unsigned pointer_=0,animation_=0;
    std::chrono::steady_clock::time_point opened_,outside_;
    bool outside_pending_=false;
    Rect icon_,area_,panel_;bool have_icon_=false;int orientation_=0;
    void build();void begin_pointer();bool watch_pointer();
public:
    void* window=nullptr;
    explicit Popup(App&);~Popup();
    void show();void hide();bool visible()const;void position();void update(const State&);
};
class SettingsView {
    App& app_;
    void* stack_=nullptr;void* error_=nullptr;void* diagnostics_=nullptr;void* info_=nullptr;
    std::map<std::string,void*> pages_;std::set<std::string> built_;
    std::unique_ptr<VolumeControl> master_,mic_,physical_;
    std::unique_ptr<Selector> output_,input_,source_;
    std::unique_ptr<AppsView> apps_;
    std::vector<std::unique_ptr<Selector>> preferences_;
    void* mic_note_=nullptr;void* enable_=nullptr;
    void* voice_start_=nullptr;void* voice_stop_=nullptr;void* voice_status_=nullptr;void* voice_meter_=nullptr;
    unsigned voice_timer_=0,opacity_save_=0;
    std::unique_ptr<VoiceTest> voice_;
    std::string voice_routes_;
    std::unique_ptr<Child> diagnosis_;
    std::unique_ptr<Child> integration_;
    void* integration_note_=nullptr;
    void* autostart_toggle_=nullptr;bool syncing_startup_=false;
    std::string diagnosis_text_;
    void changed_page();void build_page(const std::string&,void*);
    void appearance(void*);void start_voice();void stop_voice();void update_voice();
    void run_diagnosis();void integration(bool replace);std::string diagnostic_text()const;
public:
    void* window=nullptr;
    explicit SettingsView(App&);~SettingsView();
    void present();void update(const State&);
};
class App {
    unsigned server_watch_=0,config_watch_=0,signal1_=0,signal2_=0,release_=0,tray_check_=0,key_resume_=0;
    int server_=-1,config_fd_=-1;
    std::map<int,unsigned> clients_;
    std::map<int,std::string> messages_;
    std::unique_ptr<Popup> popup_;std::unique_ptr<SettingsView> settings_;
    std::unique_ptr<Backend> backend_;
    std::unique_ptr<KeyboardRuntime> keyboard_;
    Json keyboard_config_;bool keys_paused_=false,free_window_=false;
    void* provider_=nullptr;void* tray_=nullptr;
    std::string last_icon_,last_tip_,last_error_;
    bool closing_=false,standalone_=false,original_dark_=false;
    void serve();bool receive(int);void close_client(int);
    void style();void monitor_config();void configure_keyboard();
public:
    GtkApi& ui;Config config;bool demo=false,settings_mode=false;
    void* menu=nullptr;
    App(GtkApi&,bool settings,bool demonstration,bool standalone,int server);
    ~App();
    int run(bool show);
    const State& state()const{return backend_->state;}
    bool busy()const{return backend_->busy();}
    bool composited()const{return ui.gdk_screen_is_composited(ui.gdk_screen_get_default());}
    void act(Action);void message(const std::string&);const std::string& error()const{return last_error_;}
    void refresh();void show_popup(bool toggle=false);void hidden();void open_settings();void quit();void reload();
    void set(const std::string&,const Json&);
    void show_menu(void*,unsigned button=0,unsigned time=0,bool by_tray=false);void close_menu();
    void* label(const std::string&,const char* cls=nullptr,bool wrap=false);
    void* box(bool vertical=true,int gap=8);void pack(void*,void*,bool expand=false);
    void* button(const char* icon,const std::string& tip,std::function<void()>,const char* cls=nullptr);
    void* text_button(const std::string&,std::function<void()>,const char* cls=nullptr);
    void* scroll(void*,int height=220);void* section(void*,const std::string&);
    bool dragging()const;
    bool any_combo_popup()const;
    std::set<Selector*> selectors;
    void* tray()const{return tray_;}
    bool free_window()const{return free_window_;}
};
}

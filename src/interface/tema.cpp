#include "ui.hpp"
#include "tema.hpp"
#include <iomanip>
#include <sstream>

namespace rc {
std::string gerar_css_tema(const Config& c,bool composite){
    bool light=c.theme=="light";
    std::string fg=light?"#202633":"#ecedf4",dim=light?"#636b7d":"#9da5b7",line=light?"#dce0e9":"#303642";
    std::string bg=light?"246,247,251":"22,25,32",card=light?"#ffffff":"#20242e",button=light?"#edf0f8":"#2b303c",hover=light?"#e1e6f5":"#363e50";
    std::ostringstream s;s<<std::fixed<<std::setprecision(3);
    s<<".rc-root,.rc-menu {font-family: Sans; font-size:"<<c.font<<"px; color:"<<fg<<";}\n"
     <<"window.rc-popup {background: transparent;}\n"
     <<".rc-surface {background:rgba("<<bg<<','<<(composite?c.opacity:1.0)<<"); border:1px solid "<<line<<"; border-radius:"<<c.radius<<"px; padding:"<<(c.density=="compact"?12:16)<<"px;}\n"
     <<"window.rc-settings {background:rgb("<<bg<<"); color:"<<fg<<";}\n"
     <<".rc-root label,.rc-menu label {color:inherit;}\n"
     <<".rc-root .title {font-size:"<<c.font+5<<"px;font-weight:600;letter-spacing:0.1px;}\n"
     <<".rc-root .eyebrow {font-size:"<<c.font-1<<"px;font-weight:600;letter-spacing:1px;color:"<<dim<<";}\n"
     <<".rc-root .section {font-size:"<<c.font-1<<"px;font-weight:600;color:"<<dim<<";}\n"
     <<".rc-root .dim,.rc-root .empty {color:"<<dim<<";font-size:"<<c.font-1<<"px;}\n"
     <<".rc-root .empty {padding:16px 8px;}\n"
     <<".rc-root .percent {font-feature-settings:'tnum';font-size:"<<c.font-1<<"px;color:"<<dim<<";}\n"
     <<".rc-root .app-name {font-weight:500;}\n"
     <<".rc-root .card {background:"<<card<<";border:1px solid "<<line<<";border-radius:"<<std::max(0,c.radius-4)<<"px;padding:"<<(c.density=="compact"?10:12)<<"px;}\n"
     <<".rc-root button,.rc-root combobox button {background:"<<button<<";color:"<<fg<<";border:1px solid "<<line<<";border-radius:8px;box-shadow:none;text-shadow:none;min-height:24px;min-width:20px;padding:4px 7px;}\n"
     <<".rc-root button:hover,.rc-root combobox button:hover {background:"<<hover<<";border-color:"<<c.accent<<";}\n"
     <<".rc-root button:active,.rc-root button:checked {background:"<<hover<<";border-color:"<<c.accent<<";}\n"
     <<".rc-root button.primary {background:"<<c.accent<<";color:#111724;border-color:"<<c.accent<<";font-weight:600;}\n"
     <<".rc-root button.primary label {color:#111724;}\n"
     <<".rc-root button.muted {background:#492e39;border-color:#80505c;color:#ffb5bd;}\n"
     <<".rc-root button.flat {background:transparent;border-color:transparent;}\n"
     <<".rc-root button.flat:hover {background:"<<hover<<";border-color:"<<line<<";}\n"
     <<".rc-root button.more {font-size:"<<c.font+7<<"px;padding:0 6px;min-width:12px;background:transparent;border-color:transparent;}\n"
     <<".rc-root scale {padding:9px 0;min-width:44px;}\n"
     <<".rc-root scale trough {background:"<<hover<<";border:0;min-height:4px;border-radius:3px;}\n"
     <<".rc-root scale highlight {background:"<<c.accent<<";border:0;border-radius:3px;}\n"
     <<".rc-root scale slider {background:"<<c.accent<<";border:2px solid "<<card<<";min-width:10px;min-height:10px;border-radius:50%;box-shadow:none;}\n"
     <<".rc-root scale slider:hover {background:"<<fg<<";}\n"
     <<".rc-root scale:disabled {opacity:0.45;}\n"
     <<".rc-root .app-row {padding:"<<(c.density=="compact"?4:6)<<"px 2px;border-bottom:1px solid alpha("<<line<<",0.6);}\n"
     <<".rc-root separator {background:"<<line<<";min-height:1px;min-width:1px;}\n"
     <<".rc-root viewport,.rc-root scrolledwindow,.rc-root stack {background:transparent;border:0;}\n"
     <<".rc-root scrollbar {background:transparent;}\n"
     <<".rc-root scrollbar slider {background:"<<hover<<";min-width:4px;min-height:24px;border-radius:3px;}\n"
     <<".rc-root .error {color:"<<(light?"#a02940":"#f29ea9")<<";font-size:"<<c.font-1<<"px;}\n"
     <<".rc-root .warning {color:"<<(light?"#8b5a18":"#dfbd85")<<";font-size:"<<c.font-1<<"px;}\n"
     <<".rc-root notebook {border:1px solid "<<line<<";border-radius:8px;}\n"
     <<".rc-root notebook header {background:"<<card<<";border-color:"<<line<<";}\n"
     <<".rc-root notebook tab {color:"<<dim<<";padding:7px;}\n"
     <<".rc-root notebook tab:checked {color:"<<fg<<";box-shadow:inset 0 -2px "<<c.accent<<";}\n"
     <<".rc-root stacksidebar list {background:transparent;}\n"
     <<".rc-root stacksidebar row {padding:10px 12px;border-radius:8px;color:"<<dim<<";}\n"
     <<".rc-root stacksidebar row:selected {background:"<<hover<<";color:"<<fg<<";}\n"
     <<".rc-root entry,.rc-root spinbutton {background:"<<card<<";color:"<<fg<<";border-color:"<<line<<";}\n"
     <<".rc-root check {background:"<<button<<";border:1px solid "<<line<<";}\n"
     <<".rc-root check:checked {background:"<<c.accent<<";color:#111724;}\n"
     <<".rc-root progressbar trough {background:"<<button<<";border-radius:3px;min-height:4px;}\n"
     <<".rc-root progressbar progress {background:"<<c.accent<<";border:0;min-height:4px;}\n"
     <<"menu.rc-menu {background:"<<card<<";color:"<<fg<<";border:1px solid "<<line<<";padding:5px;}\n"
     <<".rc-menu menuitem {padding:8px 12px;}\n.rc-menu menuitem:hover {background:"<<hover<<";}\n"
     <<".rc-menu menuitem:disabled {color:"<<dim<<";}\n";
    if(c.theme=="system")s<<"window.rc-settings,.rc-surface {background-color:@theme_bg_color;color:@theme_fg_color;}\n";
    return s.str();
}
void App::style(){
    if(provider_){ui.gtk_style_context_remove_provider_for_screen(ui.gdk_screen_get_default(),provider_);ui.g_object_unref(provider_);}
    provider_=ui.gtk_css_provider_new();Config effective=config;if(effective.theme=="system")effective.theme=original_dark_?"dark":"light";std::string css=gerar_css_tema(effective,composited());Error* error=nullptr;
    if(!ui.gtk_css_provider_load_from_data(provider_,css.c_str(),long(css.size()),&error)){
        std::string m=error?error->message:"CSS inválido";if(error)ui.g_error_free(error);throw std::runtime_error(m);
    }
    ui.gtk_style_context_add_provider_for_screen(ui.gdk_screen_get_default(),provider_,800);
    ui.g_object_set(ui.gtk_settings_get_default(),"gtk-application-prefer-dark-theme",int(config.theme=="system"?original_dark_:config.theme=="dark"),nullptr);
}
} // namespace rc

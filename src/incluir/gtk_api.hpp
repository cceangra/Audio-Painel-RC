#pragma once
// ABI pública GTK3/GLib. Não requer headers -dev para compilar com o G++ portátil.
// Propriedade de strings e assinaturas seguem a documentação GTK 3.24.
#include <dlfcn.h>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include "platform.hpp"
namespace rc {
struct Error {unsigned domain;int code;char* message;};
struct Iter {int stamp;void* a;void* b;void* c;};
struct Rgba {double red,green,blue,alpha;};
struct Requisition {int width,height;};
class LoopApi {
    void* lib_;
protected:
    template<class F> static F symbol(void* lib,const char* name){void* p=dlsym(lib,name);if(!p)throw std::runtime_error(std::string("Símbolo indisponível: ")+name);return reinterpret_cast<F>(p);}
public:
    unsigned (*g_timeout_add)(unsigned,int(*)(void*),void*);
    unsigned (*g_idle_add)(int(*)(void*),void*);
    unsigned (*g_timeout_add_full)(int,unsigned,int(*)(void*),void*,void(*)(void*));
    unsigned (*g_idle_add_full)(int,int(*)(void*),void*,void(*)(void*));
    int (*g_source_remove)(unsigned);
    unsigned (*g_unix_fd_add)(int,int,int(*)(int,int,void*),void*);
    unsigned (*g_child_watch_add)(int,void(*)(int,int,void*),void*);
    unsigned (*g_unix_signal_add)(int,int(*)(void*),void*);
    void* (*g_main_loop_new)(void*,int);
    void (*g_main_loop_run)(void*);
    void (*g_main_loop_quit)(void*);
    void (*g_main_loop_unref)(void*);
    void (*g_free)(void*);
    void (*g_error_free)(Error*);
    LoopApi(){
        lib_=dlopen("libglib-2.0.so.0",RTLD_NOW|RTLD_LOCAL);if(!lib_)throw std::runtime_error("GLib indisponível");
#define LOAD_G(n) n=symbol<decltype(n)>(lib_,#n)
        LOAD_G(g_timeout_add);LOAD_G(g_idle_add);LOAD_G(g_timeout_add_full);LOAD_G(g_idle_add_full);LOAD_G(g_source_remove);LOAD_G(g_unix_fd_add);
        LOAD_G(g_child_watch_add);LOAD_G(g_unix_signal_add);LOAD_G(g_main_loop_new);LOAD_G(g_main_loop_run);
        LOAD_G(g_main_loop_quit);LOAD_G(g_main_loop_unref);LOAD_G(g_free);LOAD_G(g_error_free);
#undef LOAD_G
    }
    void cancel(unsigned& id){if(id){g_source_remove(id);id=0;}}
    // Sem threads ou timers permanentes para despachar trabalho na interface.
    unsigned later(std::function<void()> cb,unsigned ms=0){
        auto* f=new std::function<void()>(std::move(cb));
        auto invoke=[](void* d)->int{try{(*static_cast<std::function<void()>*>(d))();}catch(const std::exception& e){std::cerr<<e.what()<<'\n';}return 0;};
        auto destroy=[](void* d){delete static_cast<std::function<void()>*>(d);};
        return ms?g_timeout_add_full(0,ms,invoke,f,destroy):g_idle_add_full(200,invoke,f,destroy);
    }
};
class GtkApi:public LoopApi {
    void* lib_;
public:
#define FN(ret,name,...) ret (*name)(__VA_ARGS__)
    FN(int,gtk_init_check,int*,char***);FN(void,gtk_main);FN(void,gtk_main_quit);
    FN(void*,gtk_window_new,int);FN(void,gtk_window_set_title,void*,const char*);
    FN(void,gtk_window_set_default_size,void*,int,int);FN(void,gtk_window_set_resizable,void*,int);
    FN(void,gtk_window_set_decorated,void*,int);FN(void,gtk_window_set_skip_taskbar_hint,void*,int);
    FN(void,gtk_window_set_skip_pager_hint,void*,int);FN(void,gtk_window_set_keep_above,void*,int);
    FN(void,gtk_window_set_type_hint,void*,int);FN(void,gtk_window_set_position,void*,int);
    FN(void,gtk_window_set_icon_name,void*,const char*);FN(void,gtk_window_set_wmclass,void*,const char*,const char*);
    FN(void,gtk_window_present,void*);FN(void,gtk_window_move,void*,int,int);FN(void,gtk_window_resize,void*,int,int);
    FN(void,gtk_window_get_size,void*,int*,int*);FN(void,gtk_window_get_position,void*,int*,int*);
    FN(int,gtk_window_is_active,void*);
    FN(void,gtk_window_set_modal,void*,int);FN(void,gtk_window_set_transient_for,void*,void*);
    FN(void*,gtk_widget_get_style_context,void*);FN(void,gtk_style_context_add_class,void*,const char*);
    FN(void,gtk_style_context_remove_class,void*,const char*);FN(void*,gtk_css_provider_new);
    FN(int,gtk_css_provider_load_from_data,void*,const char*,long,Error**);
    FN(void,gtk_style_context_add_provider_for_screen,void*,void*,unsigned);
    FN(void,gtk_style_context_remove_provider_for_screen,void*,void*);
    FN(void,gtk_widget_show,void*);FN(void,gtk_widget_show_all,void*);FN(void,gtk_widget_hide,void*);
    FN(void,gtk_widget_set_visible,void*,int);FN(void,gtk_widget_destroy,void*);
    FN(int,gtk_widget_get_visible,void*);FN(int,gtk_widget_get_mapped,void*);
    FN(void,gtk_widget_set_sensitive,void*,int);FN(void,gtk_widget_set_tooltip_text,void*,const char*);
    FN(void,gtk_widget_set_size_request,void*,int,int);FN(void,gtk_widget_set_hexpand,void*,int);
    FN(void,gtk_widget_set_vexpand,void*,int);FN(void,gtk_widget_set_halign,void*,int);FN(void,gtk_widget_set_valign,void*,int);
    FN(void,gtk_widget_set_margin_start,void*,int);FN(void,gtk_widget_set_margin_end,void*,int);
    FN(void,gtk_widget_set_margin_top,void*,int);FN(void,gtk_widget_set_margin_bottom,void*,int);
    FN(void,gtk_widget_set_name,void*,const char*);FN(void,gtk_widget_set_no_show_all,void*,int);
    FN(void,gtk_widget_set_visual,void*,void*);FN(void,gtk_widget_set_app_paintable,void*,int);
    FN(void,gtk_widget_queue_resize,void*);FN(void,gtk_widget_queue_draw,void*);
    FN(void*,gtk_widget_get_window,void*);FN(void*,gtk_widget_get_screen,void*);
    FN(void,gtk_widget_get_preferred_size,void*,Requisition*,Requisition*);
    FN(void*,gtk_grab_get_current);
    FN(void*,gtk_box_new,int,int);FN(void,gtk_box_pack_start,void*,void*,int,int,unsigned);
    FN(void,gtk_box_pack_end,void*,void*,int,int,unsigned);FN(void,gtk_box_reorder_child,void*,void*,int);
    FN(void,gtk_container_add,void*,void*);FN(void,gtk_container_remove,void*,void*);FN(void,gtk_container_set_border_width,void*,unsigned);
    FN(void*,gtk_label_new,const char*);FN(void,gtk_label_set_text,void*,const char*);FN(void,gtk_label_set_xalign,void*,float);
    FN(void,gtk_label_set_ellipsize,void*,int);FN(void,gtk_label_set_max_width_chars,void*,int);
    FN(void,gtk_label_set_width_chars,void*,int);FN(void,gtk_label_set_line_wrap,void*,int);
    FN(void,gtk_label_set_lines,void*,int);
    FN(void,gtk_label_set_line_wrap_mode,void*,int);FN(void,gtk_label_set_selectable,void*,int);
    FN(void*,gtk_button_new);FN(void*,gtk_button_new_with_label,const char*);
    FN(void*,gtk_button_new_from_icon_name,const char*,int);FN(void,gtk_button_set_label,void*,const char*);
    FN(void,gtk_button_set_image,void*,void*);FN(void,gtk_button_set_always_show_image,void*,int);
    FN(void*,gtk_image_new_from_icon_name,const char*,int);FN(void,gtk_image_set_from_icon_name,void*,const char*,int);
    FN(void*,gtk_icon_theme_get_default);FN(int,gtk_icon_theme_has_icon,void*,const char*);FN(void,gtk_image_set_from_file,void*,const char*);
    FN(void,gtk_image_set_pixel_size,void*,int);
    FN(void*,gtk_color_button_new_with_rgba,const Rgba*);FN(void,gtk_color_chooser_get_rgba,void*,Rgba*);
    FN(void,gtk_color_button_set_title,void*,const char*);
    FN(void*,gtk_scale_new_with_range,int,double,double,double);FN(void,gtk_scale_set_draw_value,void*,int);
    FN(void,gtk_range_set_value,void*,double);FN(double,gtk_range_get_value,void*);
    FN(void*,gtk_spin_button_new_with_range,double,double,double);FN(double,gtk_spin_button_get_value,void*);
    FN(void,gtk_spin_button_set_value,void*,double);
    FN(void*,gtk_check_button_new_with_label,const char*);FN(void,gtk_toggle_button_set_active,void*,int);FN(int,gtk_toggle_button_get_active,void*);
    FN(void*,gtk_entry_new);FN(void,gtk_entry_set_text,void*,const char*);FN(const char*,gtk_entry_get_text,void*);
    FN(void*,gtk_file_chooser_dialog_new,const char*,void*,int,const char*,...);
    FN(int,gtk_file_chooser_set_current_folder,void*,const char*);FN(void,gtk_file_chooser_set_local_only,void*,int);
    FN(char*,gtk_file_chooser_get_filename,void*);FN(int,gtk_dialog_run,void*);
    FN(void,gtk_widget_grab_focus,void*);
    FN(void*,gtk_list_store_new,int,...);FN(void,gtk_list_store_clear,void*);FN(void,gtk_list_store_append,void*,Iter*);
    FN(void,gtk_list_store_set,void*,Iter*,...);FN(void*,gtk_combo_box_new_with_model,void*);
    FN(int,gtk_combo_box_get_active,void*);FN(void,gtk_combo_box_set_active,void*,int);
    FN(void*,gtk_cell_renderer_text_new);FN(void,gtk_cell_layout_pack_start,void*,void*,int);
    FN(void,gtk_cell_layout_add_attribute,void*,void*,const char*,int);
    FN(void*,gtk_scrolled_window_new,void*,void*);FN(void,gtk_scrolled_window_set_policy,void*,int,int);
    FN(void,gtk_scrolled_window_set_min_content_height,void*,int);FN(void,gtk_scrolled_window_set_max_content_height,void*,int);
    FN(void,gtk_scrolled_window_set_propagate_natural_height,void*,int);
    FN(void*,gtk_notebook_new);FN(int,gtk_notebook_append_page,void*,void*,void*);
    FN(int,gtk_notebook_get_current_page,void*);
    FN(void*,gtk_expander_new,const char*);FN(int,gtk_expander_get_expanded,void*);
    FN(void,gtk_expander_set_expanded,void*,int);
    FN(void*,gtk_stack_new);FN(void,gtk_stack_add_titled,void*,void*,const char*,const char*);
    FN(const char*,gtk_stack_get_visible_child_name,void*);FN(void,gtk_stack_set_visible_child_name,void*,const char*);
    FN(void,gtk_stack_set_transition_type,void*,int);FN(void*,gtk_stack_sidebar_new);FN(void,gtk_stack_sidebar_set_stack,void*,void*);
    FN(void*,gtk_separator_new,int);
    FN(void*,gtk_progress_bar_new);FN(void,gtk_progress_bar_set_fraction,void*,double);
    FN(void*,gtk_drawing_area_new);FN(int,gtk_widget_get_allocated_width,void*);FN(int,gtk_widget_get_allocated_height,void*);
    FN(void*,gtk_menu_new);FN(void*,gtk_menu_item_new_with_label,const char*);
    FN(unsigned,gtk_get_current_event_time);
    FN(void,gtk_menu_shell_append,void*,void*);
    FN(void,gtk_menu_popup,void*,void*,void*,void(*)(void*,int*,int*,int*,void*),void*,unsigned,unsigned);
    FN(void,gtk_menu_popdown,void*);FN(void,gtk_status_icon_position_menu,void*,int*,int*,int*,void*);
    FN(void*,gtk_status_icon_new_from_file,const char*);FN(void,gtk_status_icon_set_from_file,void*,const char*);
    FN(void,gtk_status_icon_set_tooltip_text,void*,const char*);FN(void,gtk_status_icon_set_visible,void*,int);
    FN(void,gtk_status_icon_set_name,void*,const char*);FN(void,gtk_status_icon_set_title,void*,const char*);
    FN(int,gtk_status_icon_get_geometry,void*,void**,Rect*,int*);FN(int,gtk_status_icon_is_embedded,void*);
    FN(void*,gtk_clipboard_get,void*);FN(void,gtk_clipboard_set_text,void*,const char*,int);
    FN(void*,gtk_settings_get_default);
    FN(void*,gdk_screen_get_default);FN(void*,gdk_screen_get_rgba_visual,void*);FN(int,gdk_screen_is_composited,void*);
    FN(void*,gdk_display_get_default);FN(void*,gdk_display_get_primary_monitor,void*);FN(void*,gdk_display_get_monitor,void*,int);
    FN(void*,gdk_display_get_monitor_at_point,void*,int,int);FN(void,gdk_monitor_get_workarea,void*,Rect*);
    FN(void*,gdk_display_get_default_seat,void*);FN(void*,gdk_seat_get_pointer,void*);
    FN(void,gdk_device_get_position,void*,void**,int*,int*);FN(int,gdk_window_get_origin,void*,int*,int*);
    FN(unsigned,gdk_window_get_state,void*);FN(int,gdk_event_get_keyval,void*,unsigned*);
    FN(int,gdk_event_get_scroll_direction,void*,int*);FN(int,gdk_event_get_scroll_deltas,void*,double*,double*);
    FN(void*,gdk_atom_intern_static_string,const char*);
    FN(unsigned long,g_signal_connect_data,void*,const char*,void(*)(),void*,void(*)(void*,void*),unsigned);
    FN(int,gdk_event_get_state,void*,unsigned*);FN(const char*,gdk_keyval_name,unsigned);
    FN(char*,gtk_accelerator_get_label,unsigned,unsigned);
    FN(void,g_object_set,void*,const char*,...);FN(void,g_object_get,void*,const char*,...);FN(void,g_object_unref,void*);FN(void*,g_object_ref_sink,void*);
    FN(void,cairo_save,void*);FN(void,cairo_restore,void*);FN(void,cairo_set_operator,void*,int);
    FN(void,cairo_set_source_rgba,void*,double,double,double,double);FN(void,cairo_paint,void*);
    FN(void,cairo_rectangle,void*,double,double,double,double);FN(void,cairo_fill,void*);
#undef FN
    GtkApi();
    template<class F> void connect(void* obj,const char* sig,F cb,void* data){g_signal_connect_data(obj,sig,reinterpret_cast<void(*)()>(cb),data,nullptr,0);}
    void clicked(void* obj,std::function<void()> cb){
        auto* fn=new std::function<void()>(std::move(cb));
        g_signal_connect_data(obj,"clicked",reinterpret_cast<void(*)()>(+[](void*,void* p){try{(*static_cast<std::function<void()>*>(p))();}catch(const std::exception& e){std::cerr<<e.what()<<'\n';}}),fn,+[](void* p,void*){delete static_cast<std::function<void()>*>(p);},0);
    }
    void cls(void* w,const char* c){gtk_style_context_add_class(gtk_widget_get_style_context(w),c);}
    void uncls(void* w,const char* c){gtk_style_context_remove_class(gtk_widget_get_style_context(w),c);}
};
}

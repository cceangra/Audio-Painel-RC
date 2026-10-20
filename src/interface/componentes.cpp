#include "ui.hpp"

namespace rc {
void* App::box(bool vertical,int gap){return ui.gtk_box_new(vertical?1:0,gap);}
void App::pack(void* parent,void* child,bool expand){ui.gtk_box_pack_start(parent,child,expand,expand,0);}
void* App::label(const std::string& text,const char* style,bool wrap){
    void* l=ui.gtk_label_new(text.c_str());ui.gtk_label_set_xalign(l,0);if(style)ui.cls(l,style);
    if(wrap){ui.gtk_label_set_line_wrap(l,1);ui.gtk_label_set_line_wrap_mode(l,2);ui.gtk_label_set_max_width_chars(l,60);}return l;
}
void* App::button(const char* icon,const std::string& tip,std::function<void()> callback,const char* cls){
    void* b=ui.gtk_button_new_from_icon_name(icon,1);ui.gtk_widget_set_tooltip_text(b,tip.c_str());if(cls)ui.cls(b,cls);ui.clicked(b,std::move(callback));return b;
}
void* App::text_button(const std::string& text,std::function<void()> callback,const char* cls){
    void* b=ui.gtk_button_new_with_label(text.c_str());if(cls)ui.cls(b,cls);ui.clicked(b,std::move(callback));return b;
}
void* App::scroll(void* child,int height){
    void* w=ui.gtk_scrolled_window_new(nullptr,nullptr);ui.gtk_scrolled_window_set_policy(w,2,1);
    ui.gtk_scrolled_window_set_min_content_height(w,50);ui.gtk_scrolled_window_set_max_content_height(w,height);
    ui.gtk_scrolled_window_set_propagate_natural_height(w,1);ui.gtk_container_add(w,child);return w;
}
void* App::section(void* parent,const std::string& title){
    void* row=box(false,10);pack(row,label(title,"section"));void* sep=ui.gtk_separator_new(0);ui.gtk_widget_set_valign(sep,3);ui.gtk_widget_set_size_request(sep,-1,1);pack(row,sep,true);pack(parent,row);return row;
}
} // namespace rc

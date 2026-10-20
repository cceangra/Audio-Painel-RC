#include "gtk_api.hpp"
namespace rc {
GtkApi::GtkApi(){
    lib_=dlopen("libgtk-3.so.0",RTLD_NOW|RTLD_LOCAL);if(!lib_)throw std::runtime_error(std::string("GTK3 indisponível: ")+dlerror());
#define L(n) n=symbol<decltype(n)>(lib_,#n)
    L(gtk_init_check);L(gtk_main);L(gtk_main_quit);L(gtk_window_new);L(gtk_window_set_title);L(gtk_window_set_default_size);
    L(gtk_window_set_resizable);L(gtk_window_set_decorated);L(gtk_window_set_skip_taskbar_hint);L(gtk_window_set_skip_pager_hint);
    L(gtk_window_set_keep_above);L(gtk_window_set_type_hint);L(gtk_window_set_position);L(gtk_window_set_icon_name);L(gtk_window_set_wmclass);
    L(gtk_window_present);L(gtk_window_move);L(gtk_window_resize);L(gtk_window_get_size);L(gtk_window_get_position);L(gtk_window_is_active);
    L(gtk_window_set_modal);L(gtk_window_set_transient_for);L(gtk_widget_grab_focus);
    L(gtk_widget_get_style_context);L(gtk_style_context_add_class);L(gtk_style_context_remove_class);L(gtk_css_provider_new);
    L(gtk_css_provider_load_from_data);L(gtk_style_context_add_provider_for_screen);L(gtk_style_context_remove_provider_for_screen);
    L(gtk_widget_show);L(gtk_widget_show_all);L(gtk_widget_hide);L(gtk_widget_set_visible);L(gtk_widget_destroy);L(gtk_widget_get_visible);L(gtk_widget_get_mapped);
    L(gtk_widget_set_sensitive);L(gtk_widget_set_tooltip_text);L(gtk_widget_set_size_request);L(gtk_widget_set_hexpand);L(gtk_widget_set_vexpand);
    L(gtk_widget_set_halign);L(gtk_widget_set_valign);L(gtk_widget_set_margin_start);L(gtk_widget_set_margin_end);L(gtk_widget_set_margin_top);L(gtk_widget_set_margin_bottom);
    L(gtk_widget_set_name);L(gtk_widget_set_no_show_all);L(gtk_widget_set_visual);L(gtk_widget_set_app_paintable);L(gtk_widget_queue_resize);L(gtk_widget_queue_draw);
    L(gtk_widget_get_window);L(gtk_widget_get_screen);L(gtk_widget_get_preferred_size);L(gtk_grab_get_current);
    L(gtk_box_new);L(gtk_box_pack_start);L(gtk_box_pack_end);L(gtk_box_reorder_child);L(gtk_container_add);L(gtk_container_remove);L(gtk_container_set_border_width);
    L(gtk_label_new);L(gtk_label_set_text);L(gtk_label_set_xalign);L(gtk_label_set_ellipsize);L(gtk_label_set_max_width_chars);L(gtk_label_set_width_chars);
    L(gtk_label_set_line_wrap);L(gtk_label_set_line_wrap_mode);L(gtk_label_set_selectable);
    L(gtk_label_set_lines);
    L(gtk_button_new);L(gtk_button_new_with_label);L(gtk_button_new_from_icon_name);L(gtk_button_set_label);L(gtk_button_set_image);L(gtk_button_set_always_show_image);
    L(gtk_image_new_from_icon_name);L(gtk_image_set_from_icon_name);L(gtk_image_set_pixel_size);
    L(gtk_icon_theme_get_default);L(gtk_icon_theme_has_icon);L(gtk_image_set_from_file);
    L(gtk_color_button_new_with_rgba);L(gtk_color_chooser_get_rgba);L(gtk_color_button_set_title);
    L(gtk_scale_new_with_range);L(gtk_scale_set_draw_value);L(gtk_range_set_value);L(gtk_range_get_value);
    L(gtk_spin_button_new_with_range);L(gtk_spin_button_get_value);L(gtk_spin_button_set_value);
    L(gtk_check_button_new_with_label);L(gtk_toggle_button_set_active);L(gtk_toggle_button_get_active);
    L(gtk_entry_new);L(gtk_entry_set_text);L(gtk_entry_get_text);L(gtk_file_chooser_dialog_new);L(gtk_file_chooser_set_current_folder);L(gtk_file_chooser_set_local_only);L(gtk_file_chooser_get_filename);L(gtk_dialog_run);
    L(gtk_list_store_new);L(gtk_list_store_clear);L(gtk_list_store_append);L(gtk_list_store_set);L(gtk_combo_box_new_with_model);
    L(gtk_combo_box_get_active);L(gtk_combo_box_set_active);L(gtk_cell_renderer_text_new);L(gtk_cell_layout_pack_start);L(gtk_cell_layout_add_attribute);
    L(gtk_scrolled_window_new);L(gtk_scrolled_window_set_policy);L(gtk_scrolled_window_set_min_content_height);L(gtk_scrolled_window_set_max_content_height);L(gtk_scrolled_window_set_propagate_natural_height);
    L(gtk_notebook_new);L(gtk_notebook_append_page);L(gtk_notebook_get_current_page);L(gtk_expander_new);L(gtk_expander_get_expanded);L(gtk_expander_set_expanded);
    L(gtk_stack_new);L(gtk_stack_add_titled);L(gtk_stack_get_visible_child_name);L(gtk_stack_set_visible_child_name);L(gtk_stack_set_transition_type);
    L(gtk_stack_sidebar_new);L(gtk_stack_sidebar_set_stack);L(gtk_separator_new);L(gtk_progress_bar_new);L(gtk_progress_bar_set_fraction);
    L(gtk_drawing_area_new);L(gtk_widget_get_allocated_width);L(gtk_widget_get_allocated_height);
    L(gtk_menu_new);L(gtk_menu_item_new_with_label);L(gtk_menu_shell_append);L(gtk_menu_popup);L(gtk_menu_popdown);L(gtk_status_icon_position_menu);
    L(gtk_get_current_event_time);
    L(gtk_status_icon_new_from_file);L(gtk_status_icon_set_from_file);L(gtk_status_icon_set_tooltip_text);L(gtk_status_icon_set_visible);L(gtk_status_icon_set_name);L(gtk_status_icon_set_title);
    L(gtk_status_icon_get_geometry);L(gtk_status_icon_is_embedded);L(gtk_clipboard_get);L(gtk_clipboard_set_text);L(gtk_settings_get_default);
    L(gdk_screen_get_default);L(gdk_screen_get_rgba_visual);L(gdk_screen_is_composited);L(gdk_display_get_default);L(gdk_display_get_primary_monitor);L(gdk_display_get_monitor);L(gdk_display_get_monitor_at_point);L(gdk_monitor_get_workarea);
    L(gdk_display_get_default_seat);L(gdk_seat_get_pointer);L(gdk_device_get_position);L(gdk_window_get_origin);L(gdk_window_get_state);
    L(gdk_event_get_keyval);L(gdk_event_get_scroll_direction);L(gdk_event_get_scroll_deltas);L(gdk_atom_intern_static_string);
    L(g_signal_connect_data);L(g_object_set);L(g_object_get);L(g_object_unref);L(g_object_ref_sink);
    L(gdk_event_get_state);L(gdk_keyval_name);L(gtk_accelerator_get_label);
    L(cairo_save);L(cairo_restore);L(cairo_set_operator);L(cairo_set_source_rgba);L(cairo_paint);
    L(cairo_rectangle);L(cairo_fill);
#undef L
}
}

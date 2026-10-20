#pragma once
#include "gtk_api.hpp"
namespace rc {
int setup_window(GtkApi&,bool keyboard_only=false,bool demonstration=false);
int uninstall_window(GtkApi&,bool demonstration=false);
}

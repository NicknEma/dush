#ifndef DUSH_H
#define DUSH_H

#define HELP_TEXT \
"The dush shell has a minimal set of commands:\n" \
"  cd  \tPrints or sets the current directory\n" \
"  exit\tExits the shell\n" \
"  help\tPrints this text\n" \
"  pwd \tPrints the current directory\n"

static void init_ctrl_c_handler(void);

static String get_line(Arena *arena);
static String get_current_directory(Arena *arena);
static String get_system_path(Arena *arena);

static void   set_current_directory(String dir);

#endif

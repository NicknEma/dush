#ifndef DUSH_H
#define DUSH_H

static void init_ctrl_c_handler(void);

static String get_line(Arena *arena);
static String get_current_directory(Arena *arena);
static String get_system_path(Arena *arena);

#endif

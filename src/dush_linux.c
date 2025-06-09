#ifndef DUSH_LINUX_C
#define DUSH_LINUX_C

////////////////////////////////
//~ Signal handling

#include <signal.h>

static void
INT_handler(int sig) {
	(void)sig;
}

static void
init_ctrl_c_handler(void) {
	signal(SIGINT, INT_handler);
}

////////////////////////////////
//~ Queries

static String
get_current_directory(Arena *arena) {
	String result = {0};
	
	errno = 0;
	i64 initial = pathconf(".", _PC_PATH_MAX);
	if (initial < 0) {
		if (errno != 0) {
# if HAS_INCLUDE(<libexplain/pathconf.h>)
			fprintf(errno, "%s\n", explain_pathconf(".", _PC_PATH_MAX));
#endif
			
#if AGGRESSIVE_ASSERTS
			panic();
#endif
		}
	}
	
	// Note: PATH_MAX is not actually the max size of the path that getcwd() can return;
	// This means we can't just allocate PATH_MAX bytes, but we have to try until it works.
	//
	// Also, getcwd() is not going to fill the buffer with part of the path up until it fits,
	// it's all or nothing; so a path longer than PATH_MAX requires this re-try approach.
	
	i64   buffer_cap = clamp(1024, initial, 10240);
	char *buffer     = push_zero(arena, buffer_cap);
	
	if (buffer != NULL) {
		while (true) {
			char *ptr = getcwd(buffer, buffer_cap);
			if (ptr != NULL) {
				break; // Success!
			}
			
			if (errno == ERANGE) {
				// Buffer was too small, reallocate and try again.
				pop_amount(arena, buffer_cap);
				
				buffer_cap *= 2;
				buffer      = push_zero(arena, buffer_cap);
				if (buffer == NULL) {
					buffer_cap = 0;
					break;
				}
			} else {
				assert(errno != EINVAL); // Only happens if buffer_cap is 0, but we made sure it is at least 1024.
				
#if AGGRESSIVE_ASSERTS
				panic("Permission denied or something.");
#endif
				
				break;
			}
		}
		
		i64 buffer_len = strnlen(buffer, buffer_cap);
		pop_amount(arena, buffer_cap - buffer_len);
		
		result = string(cast(u8 *) buffer, buffer_len);
	}
	
	return result;
}

static String
get_system_path(Arena *arena) {
	unimplemented();
	
	(void)arena;
	return string_from_lit("");
}

////////////////////////////////
//~ Other

static void
set_current_directory(String dir) {
	Scratch scratch = scratch_begin(0, 0);
	
	char *dir_nt = cstring_from_string(scratch.arena, dir);
	
	if (chdir(dir_nt) < 0) {
		// TODO: Read 'path_resolution(7) - Linux man page' to know more
		// about what can go wrong.
		fprintf(stderr, "Could not change directory to '%s': %s.\n",
				dir_nt, strerror(errno));
	}
	
	scratch_end(scratch);
}

#endif

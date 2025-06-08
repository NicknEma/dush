#ifndef DUSH_OS_LINUX_C
#define DUSH_OS_LINUX_C

////////////////////////////////
//~ Console IO

static i64
print_unbuffered(String s) {
	i64 written = 0;
	
	if (s.len > 0) {
		int nwrite = write(STDOUT_FILENO, s.data, s.len);
		if (nwrite != -1) {
			written += nwrite;
			if (nwrite < s.len) {
				nwrite = write(STDOUT_FILENO, s.data + nwrite, s.len - nwrite);
				if (nwrite == -1) {
#if AGGRESSIVE_ASSERTS
					panic(errno);
#endif
				} else {
					written += nwrite;
				}
			}
		} else {
#if AGGRESSIVE_ASSERTS
			panic(errno);
#endif
		}
	} else {
		// If writing with a length of 0 to something other than a regular file
		// the result of write() is unspecified, so we better not do that.
	}
	
	return written;
}

////////////////////////////////
//~ Path manipulation

static String
get_separator(void) {
	return string_from_lit("/");
}

static bool
is_separator(u8 c) {
	return c == '/';
}

////////////////////////////////
//~ File system introspection

static File_Iterator *
file_iterator_begin(Arena *arena, String path) {
	unimplemented();
	
	(void)arena;
	(void)path;
	return 0;
}

static bool
file_iterator_next(Arena *arena, File_Iterator *iterator, File_Info *info) {
	unimplemented();
	
	(void)arena;
	(void)iterator;
	(void)info;
	return 0;
}

static void
file_iterator_end(File_Iterator *iterator) {
	unimplemented();
	
	(void)iterator;
}

////////////////////////////////
//~ Process creation

//- Process creation functions

static bool
start_process_sync(String command_line, String working_dir) {
	unimplemented();
	
	(void)command_line;
	(void)working_dir;
	return 0;
}

#endif

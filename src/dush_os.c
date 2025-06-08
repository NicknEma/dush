#ifndef DUSH_OS_C
#define DUSH_OS_C

#if OS_WINDOWS
# include "dush_os_windows.c"
#elif OS_LINUX
# include "dush_os_linux.c"
#endif

////////////////////////////////
//~ Path manipulation

static bool
is_slash(u8 c) {
	return c == '\\' || c == '/';
}

static i64
path_volume_name_len(String path) {
	i64 len = 0;
#if OS_WINDOWS
	
	// See:
	// https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-volume
	
	if (path.len >= 2) {
		if (isalpha(path.data[0]) && path.data[1] == ':') {
			// Regular volume, such as "C:\"
			len = 2;
		} else if (path.len >= 5 && is_slash(path.data[0]) && is_slash(path.data[1]) &&
				   !is_slash(path.data[2]) && path.data[2] != '.' &&
				   is_slash(path.data[3]) && path.data[4] != '.') {
			// Volume GUID path, such as "\\?\Volume{26a21bda-a627-11d7-9931-806e6f6e6963}\"
			i64 n;
			for (n = 4; n < path.len; n += 1) {
				if (is_slash(path.data[n])) {
					break;
				}
			}
			
			len = n;
		}
	}
	
#endif
	return len;
}

static String
path_volume_name(String path) {
	return string_stop(path, path_volume_name_len(path));
}

static String
path_skip_volume_name(String path) {
	return string_skip(path, path_volume_name_len(path));
}

static i64
path_last_separator(String path) {
	i64 result = -1;
	for (i64 i = path.len - 1; i > -1; i -= 1) {
		if (is_separator(path.data[i])) {
			result = i + 1;
			break;
		}
	}
	return result;
}

static String
path_base(String path) {
	i64 slash_index = path_last_separator(path);
	if (slash_index < 0) slash_index = 0;
	
	String base = string_skip(path, slash_index);
	return base;
}

////////////////////////////////
//~ Basic file management

static Read_File_Result
read_file(Arena *arena, String file_name) {
	last_file_error = File_Error_NONE;
	
	Read_File_Result result = {0};
	
	Scratch scratch = scratch_begin(&arena, 1);
	char *file_name_null_terminated = cstring_from_string(scratch.arena, file_name);
	
	if (file_name_null_terminated) {
		FILE *handle = fopen(file_name_null_terminated, "rb");
		if (handle) {
			errno = 0;
			size_t size = fsize(handle);
			if (errno == 0) {
				result.contents.len  = size;
				result.contents.data = push_nozero(arena, size * sizeof(u8));
				if (size == 0 || result.contents.data) {
					i64 read_amount  = fread(result.contents.data,
											 sizeof(u8),
											 result.contents.len,
											 handle);
					if (read_amount == result.contents.len || feof(handle)) {
						result.ok = true;
					} else {
						last_file_error = File_Error_READ_FAILED;
					}
				}
			} else {
				last_file_error = File_Error_SEEK_FAILED;
			}
			fclose(handle);
		} else {
			switch (errno) {
				case ENOENT: last_file_error = File_Error_NOT_EXISTS; break;
				case EISDIR: last_file_error = File_Error_IS_DIRECTORY; break;
				default: last_file_error = File_Error_OTHER; break;
			}
		}
	} else {
		assert(last_alloc_error);
	}
	
	scratch_end(scratch);
	
	return result;
}

static String
last_file_error_string(void) {
	read_only static String strings[] = {
		string_from_lit_const(""),
		string_from_lit_const("The file already exists."),
		string_from_lit_const("The file does not exist."),
	};
	
	String result = string_from_lit("(unknown)");
	if (last_file_error >= 0 && last_file_error < array_count(strings)) {
		result = strings[last_file_error];
	}
	return result;
}

////////////////////////////////
//~ File system introspection

static void
file_info_list_push(Arena *arena, File_Info_List *list, File_Info info) {
	File_Info_Node *node = push_type(arena, File_Info_Node);
	node->info = info;
	
	queue_push(list->first, list->last, node);
	list->count += 1;
}

// TODO(ema): What is the path exactly? A directory? What can and can't be in the path?
// Probabily decide after doing more than 1 OS.
static File_Info_List
file_info_list_from_path(Arena *arena, String path) {
	File_Info_List list = {0};
	
	// The scratch arena is used for the iteration (for example, on Windows, to store the utf16 path),
	// while the param arena is used for the list.
	Scratch scratch = scratch_begin(&arena, 1);
	{
		File_Iterator *iterator = file_iterator_begin(scratch.arena, path); // This resets the last error to 0.
#if 1
		File_Info info = {0};
		for (bool ok = file_iterator_next(arena, iterator, &info); ok; ok = file_iterator_next(arena, iterator, &info)) {
			file_info_list_push(arena, &list, info);
		}
#endif
		file_iterator_end(iterator);
	}
	scratch_end(scratch);
	
	return list;
}

////////////////////////////////
//~ Process creation

//- Process creation functions

static String
last_process_error_string(void) {
	read_only static String strings[] = {
		string_from_lit_const(""),
		string_from_lit_const("The system cannot find the file specified."),
		string_from_lit_const("One of the parameters is incorrect."),
		string_from_lit_const("The file is not a valid executable."),
		string_from_lit_const("The process cannot be started for an unspecified reason."),
	};
	
	String result = string_from_lit("(unknown)");
	if (last_process_error >= 0 && last_process_error < array_count(strings)) {
		result = strings[last_process_error];
	}
	return result;
}

#endif

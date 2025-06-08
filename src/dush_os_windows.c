#ifndef DUSH_OS_WINDOWS_C
#define DUSH_OS_WINDOWS_C

////////////////////////////////
//~ Console IO

static i64
print_unbuffered(String s) {
	i64 written = 0;
	
	HANDLE hstdout = GetStdHandle(STD_OUTPUT_HANDLE);
	while (written < s.len) {
		i64  desired_nwrite = s.len - written;
		u32  attempt_nwrite = cast(DWORD) desired_nwrite;
		DWORD actual_nwrite = 0;
		
		// Note: Call WriteFile instead of WriteConsole because the latter prevents
		// output redirection.
		if (!WriteFile(hstdout, s.data + written, attempt_nwrite, &actual_nwrite, NULL)) {
			int n = GetLastError();
			(void)n;
			
#if AGGRESSIVE_ASSERTS
			panic();
#endif
		}
		
		written += cast(i64) actual_nwrite;
	}
	
	return written;
}

////////////////////////////////
//~ Path navigation

static String
get_separator(void) {
	return string_from_lit("\\");
}

static bool
is_separator(u8 c) {
	return c == '\\' || c == '/';
}

static bool
path_is_reserved_name(String path) {
	read_only static String reserved_names[] = {
		string_from_lit_const("CON"),  string_from_lit_const("PRN"),  string_from_lit_const("AUX"),  string_from_lit_const("NUL"),
		string_from_lit_const("COM1"), string_from_lit_const("COM2"), string_from_lit_const("COM3"), string_from_lit_const("COM4"),
		string_from_lit_const("COM5"), string_from_lit_const("COM6"), string_from_lit_const("COM7"), string_from_lit_const("COM8"),
		string_from_lit_const("COM9"), string_from_lit_const("LPT1"), string_from_lit_const("LPT2"), string_from_lit_const("LPT3"),
		string_from_lit_const("LPT4"), string_from_lit_const("LPT5"), string_from_lit_const("LPT6"), string_from_lit_const("LPT7"),
		string_from_lit_const("LPT8"), string_from_lit_const("LPT9"),
	};
	
	bool result = false;
	if (path.len > 0) {
		for (i64 i = 0; i < array_count(reserved_names); i += 1) {
			if (string_equals_case_insensitive(path, reserved_names[i])) {
				result = true;
				break;
			}
		}
	}
	return result;
}

static bool
path_is_abs(String path) {
	bool result = false;
	
	if (path_is_reserved_name(path)) {
		result = true;
	} else {
		i64 len = path_volume_name_len(path);
		if (len > 0) {
			result = path.len > len && is_separator(path.data[len]);
		}
	}
	
	return result;
}

////////////////////////////////
//~ Basic file management

////////////////////////////////
//~ File system introspection

#define WIN32_USE_WIDE_STRINGS 0

#if WIN32_USE_WIDE_STRINGS
# error Unimplemented.
#endif

//- File system introspection types

typedef struct Win32_File_Find_Data Win32_File_Find_Data;
struct Win32_File_Find_Data {
	HANDLE handle;
#if WIN32_USE_WIDE_STRINGS
	WIN32_FIND_DATAW find_data;
#else
	WIN32_FIND_DATAA find_data;
#endif
	int  last_error;
	bool first_iteration_done;
};

// #assert(size_of(Win32_File_Find_Data) <= size_of(File_Iterator), "os_w32_file_find_data_size");

//- File system introspection functions

// Converts attributes queried through `GetFileAttributesEx` into `File_Flags`.
static File_Flags
_file_flags_from_attributes(u32 attributes) {
	File_Flags flags = 0;
	if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
		flags &= File_Flag_IS_DIRECTORY;
	}
	return flags;
}

// Converts attributes queried through `GetFileAttributesEx` into `Access_Flags`.
static Access_Flags
_file_access_flags_from_attributes(u32 attributes) {
	Access_Flags flags = Access_Flag_READ;
	if ((attributes & FILE_ATTRIBUTE_READONLY) == 0) {
		flags |= Access_Flag_WRITE;
	}
	return flags;
}

static File_Iterator *
file_iterator_begin(Arena *arena, String path) {
	last_alloc_error = Alloc_Error_NONE;
	last_file_error = File_Error_NONE;
	
	Win32_File_Find_Data *find_data = push_type(arena, Win32_File_Find_Data);
	File_Iterator *iterator = cast(File_Iterator *) find_data;
	
	if (iterator != NULL) {
		path = string_skip_chop_whitespace(path);
		path = string_chop_past_last_slash(path);
		if (path.len > 0) {
			Scratch scratch = scratch_begin(&arena, 1);
			String path_star = push_stringf(scratch.arena, "%.*s*", string_expand(path)); // Append '*' at the end
			
#if WIN32_USE_WIDE_STRINGS
			wchar_t *path16 = cstring16_from_string8(scratch.arena, path_star);
			if (path16 != NULL) find_data->handle = FindFirstFileW(path16, &find_data->find_data);
#else
			char *path8 = cstring_from_string(scratch.arena, path_star);
			if (path8  != NULL) find_data->handle = FindFirstFileA(path8,  &find_data->find_data);
#endif
			
			if (find_data->handle == INVALID_HANDLE_VALUE) {
				// For now simply store the error. Later, in file_iterator_next(), the global error
				// variable is set.
				find_data->last_error = GetLastError();
			}
			
			scratch_end(scratch);
		}
	} else {
		assert(last_alloc_error);
	}
	
	return iterator;
}

static bool
file_iterator_next(Arena *arena, File_Iterator *iterator, File_Info *info) {
	last_file_error = File_Error_NONE;
	bool success = true;
	
	// Grab find data
	Win32_File_Find_Data *file_find_data = cast(Win32_File_Find_Data *) iterator;
	
#if WIN32_USE_WIDE_STRINGS
	WIN32_FIND_DATAW find_data = {0};
#else
	WIN32_FIND_DATAA find_data = {0};
#endif
	
	while (true) {
		// Check whether we already returned the file we got from FindFirstFile
		bool first_file_returned = false;
		if (!file_find_data->first_iteration_done) {
			// This is the first iteration (of the file iteration, not the loop iteration):
			// Return the data we already got from FindFirstFile
			
			find_data = file_find_data->find_data;
			
			if (file_find_data->handle == NULL || file_find_data->handle == INVALID_HANDLE_VALUE) {
				// If last_error is ERROR_FILE_NOT_FOUND, it simply means there are no more files
				// in the directory. Leave last_file_error as NONE.
				if (file_find_data->last_error != ERROR_FILE_NOT_FOUND) {
					// last_file_error = ...;
					
					success = false;
				} else {
					success = true;
				}
			}
			
			file_find_data->first_iteration_done = true;
			first_file_returned = true;
		}
		
		// If we didn't return the first, OR the first was not good, then proceed to
		// FindNextFile
		if (!first_file_returned) {
#if WIN32_USE_WIDE_STRINGS
			success = FindNextFileW(file_find_data->handle, &find_data);
#else
			success = FindNextFileA(file_find_data->handle, &find_data);
#endif
		}
		
		if (!success) {
			break;
		}
		
		// Skip . and .. which are the current and parent directories.
		if (find_data.cFileName[0] == '.' && (find_data.cFileName[1] == 0 ||
											  find_data.cFileName[1] == '.')) {
			// Skip this file, which means continue the loop.
			continue;
		} else {
			break;
		}
	}
	
	// Fill output
	if (success) {
		i64 name_len = 0;
		// This is basically a strlen(), just bounded by MAX_PATH
		for (i64 i = 0; i < MAX_PATH; i += 1) {
			if (find_data.cFileName[i] == 0) {
				break;
			}
			name_len += 1;
		}
		
#if WIN32_USE_WIDE_STRINGS
		name16 := transmute([]u16)base.Raw_Slice{
			data = raw_data(find_data.cFileName[:]),
			len  = name_len,
		};
		
		info->name = string8_from_string16(arena, name16);
#else
		info->name = string_clone(arena, string(cast(u8 *) find_data.cFileName, name_len));
#endif
		
		info->attributes.flags         = _file_flags_from_attributes(find_data.dwFileAttributes);
		info->attributes.access        = _file_access_flags_from_attributes(find_data.dwFileAttributes);
		info->attributes.size          = (cast(u64) find_data.nFileSizeHigh << 32) | cast(u64) find_data.nFileSizeLow;
		// info->attributes.created       = packed_time_from_file_time(find_data.ftCreationTime);
		// info->attributes.last_modified = packed_time_from_file_time(find_data.ftLastWriteTime);
		
	}
	
	return success;
}

static void
file_iterator_end(File_Iterator *iterator) {
	Win32_File_Find_Data *find_data = cast(Win32_File_Find_Data *) iterator;
	FindClose(find_data->handle);
}

////////////////////////////////
//~ Process creation

static bool
start_process_sync(String command_line, String working_dir) {
	last_process_error = Process_Error_NONE;
	
	bool success = false;
	Scratch scratch = scratch_begin(0, 0);
	
	char *command_line_nt = cstring_from_string(scratch.arena, command_line);
	char *working_dir_nt  = cstring_from_string(scratch.arena, working_dir);
	if (command_line_nt && working_dir_nt) {
		STARTUPINFO si = {0};
		PROCESS_INFORMATION pi = {0};
		if (CreateProcessA(NULL, command_line_nt, NULL, NULL, FALSE, 0, NULL, working_dir_nt, &si, &pi)) {
			bool  terminated  = false;
			DWORD wait_status = WaitForSingleObject(pi.hProcess, INFINITE);
			if (wait_status == WAIT_OBJECT_0) {
				terminated = true;
			} else if (wait_status == WAIT_FAILED) {
				int last_error = GetLastError();
				(void)last_error;
				
#if AGGRESSIVE_ASSERTS
				panic();
#endif
			} else {
				panic();
			}
			
			do {
				DWORD exit_code = 0;
				if (GetExitCodeProcess(pi.hProcess, &exit_code)) {
					terminated = exit_code != STILL_ACTIVE;
					success    = true;
				} else {
					int last_error = GetLastError();
					(void)last_error;
					
#if AGGRESSIVE_ASSERTS
					panic();
#endif
				}
			} while (!terminated);
			
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		} else {
			int last_error = GetLastError();
			
			if (last_error == ERROR_INVALID_PARAMETER) {
				last_process_error = Process_Error_INVALID_PARAM;
				
#if AGGRESSIVE_ASSERTS
				panic();
#endif
			} else if (last_error == ERROR_FILE_NOT_FOUND) {
				last_process_error = Process_Error_FILE_NOT_FOUND;
			} else if (last_error == ERROR_BAD_EXE_FORMAT) {
				last_process_error = Process_Error_BAD_EXE_FORMAT;
			} else {
				// The system could fail to start a process for many reasons outside our control,
				// so it doesn't make sense to assert() that there were no errors and kill
				// the shell even when the failure is the user's or the os's fault.
				
				last_process_error = Process_Error_OTHER;
			}
		}
	}
	
	scratch_end(scratch);
	return success;
}

#endif

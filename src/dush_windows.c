#ifndef DUSH_WINDOWS_C
#define DUSH_WINDOWS_C

////////////////////////////////
//~ Error handling helpers

static String
get_system_error_message_in_english(Arena *arena, int error) {
	Scratch scratch = scratch_begin(&arena, 1);
	String  result  = {0};
	
	// Note: 64 Kilobytes is the maximum size supported by FormatMessageW, as per
	// documentation.
	int  buffer16_size = cast(int) clamp_top(kilobytes(64), arena_space(*scratch.arena));  // In bytes
	int  buffer16_cap  = buffer16_size / sizeof(u16);                                      // In characters
	u16 *buffer16      = push_nozero(scratch.arena, buffer16_cap * sizeof(u16));
	
	if (buffer16 != NULL) {
		DWORD lang_default = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT); (void)lang_default;
		DWORD lang_english = 1033; // If only the documentation explained how to get this, I wouldn't need to rely on a random stackoverflow answer to know that this is English.
		
		// Note: FormatMessageW returns a DWORD, but system-messages are probabily very short, so we shouldn't need to care about big lengths.
		int  buffer16_len  = cast(int) FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
													  NULL,  // Not used with FORMAT_MESSAGE_FROM_SYSTEM
													  error, lang_english, buffer16, buffer16_cap,
													  NULL); // Not used with FORMAT_MESSAGE_FROM_SYSTEM
		
		if (buffer16_len > 0) {
			UINT  codepage = CP_UTF8;
			
			// When the codepage is CP_UTF8 this must be either 0 or WC_ERR_INVALID_CHARS; since a system function
			// such as FormatMessageW is unlikely to produce invalid Utf16 sequences, let's just ignore errors.
			DWORD convert_flags = 0;
			int   buffer8_cap   = WideCharToMultiByte(CP_UTF8, convert_flags, buffer16, buffer16_len, NULL, 0,
													  NULL, NULL); // Must be NULL when the codepage is CP_UTF8
			if (buffer8_cap > 0) {
				char *buffer8 = push_nozero(arena, buffer8_cap);
				if (buffer8 != NULL) {
					if (!WideCharToMultiByte(codepage, convert_flags, buffer16, buffer16_len, buffer8, buffer8_cap, NULL, NULL)) {
						pop_amount(arena, buffer8_cap);
						buffer8_cap = 0;
						buffer8 = NULL;
					}
					
					result = string(cast(u8 *) buffer8, buffer8_cap);
				} else {
					// No memory to convert to utf8. Oh well...
				}
			}
		}
	} else {
		// No memory to get the error string. Oh well...
	}
	
	scratch_end(scratch);
	return result;
}

////////////////////////////////
//~ Signal handling

// Note: This callback will be called by Windows from a secondary thread.
static BOOL WINAPI
ctrl_c_handler(DWORD signal) {
	BOOL handled = (signal == CTRL_C_EVENT);
	return handled;
}

static void
init_ctrl_c_handler(void) {
	// We could disable Ctrl+C entirely by passing NULL as the first argument, but
	// that would disable it for child processes as well. We don't want that, so
	// instead we just provide a handler routine that does nothing.
	//
	// https://learn.microsoft.com/en-us/windows/console/setconsolectrlhandler?redirectedfrom=MSDN#parameters
	if (!SetConsoleCtrlHandler(ctrl_c_handler, TRUE)) {
		int last_error = GetLastError();
		(void)last_error;
	}
}

////////////////////////////////
//~ Queries

static String
get_current_directory(Arena *arena) {
	DWORD required = GetCurrentDirectory(0, NULL);
	if (required == 0) {
#if AGGRESSIVE_ASSERTS
		panic();
#endif
	}
	
	// Continue even if it is 0. Worst case scenario, allocate 0 bytes.
	String result = push_string(arena, required);
	
	DWORD written = GetCurrentDirectory(cast(DWORD) result.len, cast(char *) result.data);
	if (written < cast(DWORD) (result.len - 1) && result.len > 0) {
#if AGGRESSIVE_ASSERTS
		panic();
#endif
		
		allow_break();
	}
	
	if (result.len > 1) {
		// We pop 1 because 'required' includes the null terminator and GetCurrentDirectory expects
		// a buffer long enough for the path + the null terminator, so we allocated 1 extra when
		// doing push_string().
		pop_amount(arena, 1);
		result.len -= 1;
	}
	
	return result;
}

static String
get_system_path(Arena *arena) {
	DWORD required = ExpandEnvironmentStringsA("%PATH%", NULL, 0);
	if (required == 0) {
#if AGGRESSIVE_ASSERTS
		panic();
#endif
	}
	
	// Continue even if it is 0. Worst case scenario, allocate 0 bytes.
	String result = push_string(arena, required);
	
	DWORD written = ExpandEnvironmentStringsA("%PATH%", cast(char *) result.data, cast(DWORD) result.len);
	if (written < cast(DWORD) (result.len - 1) && result.len > 0) {
#if AGGRESSIVE_ASSERTS
		panic();
#endif
		
		allow_break();
	}
	
	if (result.len > 1) {
		// See comment in get_current_directory().
		pop_amount(arena, 1);
		result.len -= 1;
	}
	
	return result;
}

////////////////////////////////
//~ Other

static void
set_current_directory(String dir) {
	Scratch scratch = scratch_begin(0, 0);
	
	// I don't fully understand the rules under which SetCurrentDirectory operates, but *sometimes* it fails
	// when the path starts with a whitespace.
	// The path received by this function should already be trimmed, but we do it again just in case.
	dir = string_skip_chop_whitespace(dir);
	
	char *dir_nt = cstring_from_string(scratch.arena, dir);
	if (dir_nt != NULL) {
		if (!SetCurrentDirectory(dir_nt)) {
			int    last_error = GetLastError();
			String message    = {0};
			
			if (last_error == ERROR_FILENAME_EXCED_RANGE) {
				String message_I_would_like_to_print = string_from_lit("Sorry, Windows doesn't allow paths longer than 260 "
																	   "characters unless you package your application "
																	   "with a manifest file. I have no intention of "
																	   "doing that, so blame Microsoft.\n");
				(void)message_I_would_like_to_print;
				
				String message_I_actually_print = string_from_lit("The path specified is too long; the full absolute path must be "
																  "strictly shorter than 260 characters.\n");
				
				// Note: There's also this line in SetCurrentDirectory's docs:
				//  Setting a current directory longer than MAX_PATH causes CreateProcessW to fail.
				
				message = message_I_actually_print;
			} else {
				message = get_system_error_message_in_english(scratch.arena, last_error);
			}
			
			fprintf(stderr, "Could not change directory to '%s': %.*s",
					dir_nt, string_expand(message));
		}
	} else {
		errno = ENOMEM;
		fprintf(stderr, "Could not change directory to '%s': %s.\n",
				dir_nt, strerror(errno));
	}
	
	scratch_end(scratch);
}

#endif

#ifndef DUSH_WINDOWS_C
#define DUSH_WINDOWS_C

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

#endif

#ifndef DUSH_BASE_WINDOWS_C
#define DUSH_BASE_WINDOWS_C

////////////////////////////////
//~ Memory

static void *
mem_reserve(u64 size) {
	void *result = NULL;
	
	// VirtualAlloc fails with ERROR_INVALID_PARAMETER if we try to reserve 0 bytes.
	// Instead, we allow an empty allocation by simply returning NULL.
	if (size > 0) {
		// No need to align the size to a page boundary, Windows will do it for us.
		result = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
		if (!result) {
			int last_error = GetLastError();
			assert(last_error != ERROR_INVALID_PARAMETER && // If any of these is returned it is our fault,
				   last_error != ERROR_INVALID_ADDRESS   && // not the caller's
				   last_error != ERROR_NOT_SUPPORTED);
			
			last_alloc_error = Alloc_Error_OUT_OF_MEMORY;
			
#if AGGRESSIVE_ASSERTS
			panic("VirtualAlloc failed!");
#endif
		}
	} else {
#if AGGRESSIVE_ASSERTS
		panic("Tried to reserve 0 bytes.");
#endif
	}
	
	return result;
}

static void *
mem_commit(void *ptr, u64 size) {
	// No need to align the size to a page boundary, Windows will do it for us.
	void *result = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
	if (!result) {
		last_alloc_error = Alloc_Error_OUT_OF_MEMORY; // TODO: GetLastError()
		
		int last_error = GetLastError();
		assert(last_error != ERROR_INVALID_PARAMETER);
		
#if AGGRESSIVE_ASSERTS
		panic("VirtualAlloc failed!");
#endif
	}
	
	return result;
}

static void *
mem_reserve_and_commit(u64 size) {
	void *result = NULL;
	
	// VirtualAlloc fails with ERROR_INVALID_PARAMETER if we try to allocate 0 bytes.
	// Instead, we allow an empty allocation by simply returning NULL.
	if (size > 0) {
		// No need to align the size to a page boundary, Windows will do it for us.
		result = VirtualAlloc(NULL, size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
		if (!result) {
			int last_error = GetLastError();
			assert(last_error != ERROR_INVALID_PARAMETER && // If any of these is returned it is our fault,
				   last_error != ERROR_INVALID_ADDRESS   && // not the caller's
				   last_error != ERROR_NOT_SUPPORTED);
			
			last_alloc_error = Alloc_Error_OUT_OF_MEMORY;
			
#if AGGRESSIVE_ASSERTS
			panic("VirtualAlloc failed!");
#endif
		}
	} else {
#if AGGRESSIVE_ASSERTS
		panic("Tried to reserve 0 bytes.");
#endif
	}
	
	return result;
}

static bool
mem_decommit(void *ptr, u64 size) {
	return VirtualFree(ptr, size, MEM_DECOMMIT);
}

static bool
mem_release(void *ptr, u64 size) {
	(void)size; // Not needed on Windows
	
	return VirtualFree(ptr, 0, MEM_RELEASE);
}

#endif

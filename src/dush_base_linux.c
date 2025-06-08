#ifndef DUSH_BASE_LINUX_C
#define DUSH_BASE_LINUX_C

////////////////////////////////
//~ Memory

static void *
mem_reserve(u64 size) {
	void *result = mmap(0, size, 0, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	assert(result != NULL);
	
	return result;
}

static void *
mem_commit(void *ptr, u64 size) {
	int r = mprotect(ptr, size, PROT_READ|PROT_WRITE);
	assert(r != -1);
	
	return ptr;
}

static void *
mem_reserve_and_commit(u64 size) {
	return mem_commit(mem_reserve(size), size);
}

static bool
mem_decommit(void *ptr, u64 size) {
	return (mprotect(ptr, size, 0) != -1 &&
			madvise(ptr, size, MADV_FREE) != -1);
}

static bool
mem_release(void *ptr, u64 size) {
	return munmap(ptr, size) != -1;
}

#endif

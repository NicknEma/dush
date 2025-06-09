#ifndef DUSH_BASE_C
#define DUSH_BASE_C

#if OS_WINDOWS
# include "dush_base_windows.c"
#elif OS_LINUX
# include "dush_base_linux.c"
#endif

/////////////////
//~ fsize

// Sets EBADF if fp is not a seekable stream
// EINVAL if fp was NULL
static size_t
fsize(FILE *fp) {
	size_t fs = 0;
	
	if (fp) {
		fseek(fp, 0L, SEEK_END);
		
		if (errno == 0) {
			fs = ftell(fp);
			
			// If fseek succeeded before, it means that fp was
			// a seekable stream, so we don't check the error again.
			
			fseek(fp, 0L, SEEK_SET);
		}
	} else {
		errno = EINVAL;
	}
	
	return fs;
}

////////////////////////////////
//~ Core

//- Integer math

static bool
is_power_of_two(u64 i) {
	return i > 0 && (i & (i-1)) == 0;
}

static u64
align_forward(u64 ptr, u64 alignment) {
	assert(is_power_of_two(alignment));
	return (ptr + alignment-1) & ~(alignment-1);
}

static u64
round_up_to_multiple_of_u64(u64 n, u64 r) {
    u64 result;
    
    result = r - 1;
    result = n + result;
    result = result / r;
    result = result * r;
    
    return result;
}

static i64
round_up_to_multiple_of_i64(i64 n, i64 r) {
    i64 result;
    
    result = r - 1;
    result = n + result;
    result = result / r;
    result = result * r;
    
    return result;
}

////////////////////////////////
//~ Memory

//- Memory procedures

static String
last_alloc_error_string(void) {
	read_only static String strings[] = {
		string_from_lit_const(""),
		string_from_lit_const("Out of memory"),
	};
	
	String result = string_from_lit("(unknown)");
	if (last_alloc_error >= 0 && last_alloc_error < array_count(strings)) {
		result = strings[last_alloc_error];
	}
	return result;
}

////////////////////////////////
//~ Arena

//- Arena operations: constructors/destructors

static bool
_arena_init(Arena *arena, Arena_Init_Params params) {
	// Trying to initialize a NULL arena is impossible, so we don't allow it.
	// Initializing an arena with a size of 0 is useless but possible.
	assert(arena != NULL);
	
	last_alloc_error = Alloc_Error_NONE;
	u8 *base = mem_reserve(params.reserve_size);
	if (base != NULL) {
		arena->ptr  = base;
		arena->cap  = params.reserve_size;
		arena->pos  = 0;
		arena->peak = 0;
		arena->commit_pos = 0;
	} else if (params.reserve_size > 0) {
		assert(last_alloc_error);
	}
	
	return last_alloc_error == Alloc_Error_NONE;
}

static bool
arena_fini(Arena *arena) {
	assert(arena != NULL);
	
	last_alloc_error = Alloc_Error_NONE;
	bool released = mem_release(arena->ptr, arena->cap);
	memset(arena, 0, sizeof(Arena));
	
	return released && last_alloc_error == Alloc_Error_NONE;
}

static void
arena_reset(Arena *arena) {
	last_alloc_error = Alloc_Error_NONE;
	pop_to(arena, 0);
}

//- Arena operations: info

static u64
arena_cap(Arena arena) {
	return arena.cap;
}

static u64
arena_pos(Arena arena) {
	return arena.pos;
}

static u64
arena_space(Arena arena) {
	return arena.cap - arena.pos;
}

//- Arena operations: push

static void *
push_nozero_aligned(Arena *arena, u64 size, u64 alignment) {
	last_alloc_error = Alloc_Error_NONE;
	
	void *result = NULL;
	
	if (size > 0) {
		u64 align_pos = align_forward(arena->pos, alignment);
		if (align_pos + size <= arena->cap) {
			arena->pos = align_pos;
			
			result = arena->ptr + arena->pos;
			arena->pos += size;
			
			if (arena->pos > arena->commit_pos) {
				u64 new_commit_pos = clamp_top(align_forward(arena->pos, ARENA_COMMIT_GRANULARITY), arena->cap);
				
				void *commit_base = arena->ptr + arena->commit_pos;
				u64   commit_size = new_commit_pos - arena->commit_pos;
				
				(void)mem_commit(commit_base, commit_size);
				arena->commit_pos = new_commit_pos;
			}
			
			arena->peak = max(arena->pos, arena->peak);
		} else {
			last_alloc_error = Alloc_Error_OUT_OF_MEMORY;
			
#if AGGRESSIVE_ASSERTS
			panic("Arena is out of memory.");
#endif
		}
	}
	
	return result;
}

static void *
push_zero_aligned(Arena *arena, u64 size, u64 alignment) {
	last_alloc_error = Alloc_Error_NONE;
	
	void *result = push_nozero_aligned(arena, size, alignment);
	if (result != NULL) {
		memset(result, 0, size);
	}
	
	return result;
}

#define push_nozero(arena, size) push_nozero_aligned(arena, size, sizeof(u8))
#define push_zero(arena, size)   push_zero_aligned(arena, size, sizeof(u8))

#define push_aligned(arena, size, alignment) push_zero_aligned(arena, size, alignment)
#define push(arena, size)                    push_zero(arena, size)

#define push_type(arena, type)       cast(type *) push_aligned(arena, sizeof(type), alignof(type))
#define push_array(arena, type, len) cast(type *) push_aligned(arena, (len)*sizeof(type), alignof(type))

//- Arena operations: pop

static void
pop_to(Arena *arena, u64 pos) {
	last_alloc_error = Alloc_Error_NONE;
	
	pos = clamp_top(pos, arena->pos); // Prevent user from going forward, only go backward.
	
#if AGGRESSIVE_MEM_ZERO
	memset(arena->ptr + pos, 0, arena->pos - pos);
#endif
	
	arena->pos = pos;
	
	u64 pos_aligned_to_commit_chunks = clamp_top(align_forward(arena->pos, ARENA_COMMIT_GRANULARITY), arena->cap);
	
	if (pos_aligned_to_commit_chunks + ARENA_DECOMMIT_THRESHOLD <= arena->commit_pos) {
		u64   decommit_size = arena->commit_pos - pos_aligned_to_commit_chunks;
		void *decommit_base = arena->ptr + pos_aligned_to_commit_chunks;
		
		mem_decommit(decommit_base, decommit_size);
		arena->commit_pos = pos_aligned_to_commit_chunks;
	}
}

static void
pop_amount(Arena *arena, u64 amount) {
	last_alloc_error = Alloc_Error_NONE;
	
	u64 amount_clamped = clamp_top(amount, arena->pos); // Prevent user from going to negative positions
	pop_to(arena, arena->pos - amount_clamped);
}

#define pop(arena, amount) pop_amount(arena, amount)

//- Arena operations: Temp

static Arena_Restore_Point
arena_begin_temp_region(Arena *arena) {
	last_alloc_error = Alloc_Error_NONE;
	
	Arena_Restore_Point point = {arena, arena->pos};
	return point;
}

static void
arena_end_temp_region(Arena_Restore_Point point) {
	pop_to(point.arena, point.pos);
}

////////////////////////////////
//~ Scratch Memory

static Scratch
scratch_begin(Arena **conflicts, i64 conflict_count) {
	last_alloc_error = Alloc_Error_NONE;
	
	Scratch scratch = {0};
	i64 index = 0;
	
#if SCRATCH_ARENA_COUNT > 0
	if (scratch_arenas[0].ptr == NULL) { // unlikely()
		for (int i = 0; i < array_count(scratch_arenas); i += 1) {
			arena_init(&scratch_arenas[i], .reserve_size = SCRATCH_ARENA_RESERVE_SIZE);
			scratch_arenas_init_errors[i] = last_alloc_error;
		}
	}
	
	for (i64 scratch_arena_index = 0; scratch_arena_index < array_count(scratch_arenas); scratch_arena_index += 1) {
		bool is_conflicting = false;
		for (i64 conflict_index = 0; conflict_index < conflict_count; conflict_index += 1) {
			if (conflicts[conflict_index] == &scratch_arenas[scratch_arena_index]) {
				is_conflicting = true;
				break;
			}
		}
		
		if (!is_conflicting && scratch_arenas[scratch_arena_index].ptr != NULL) {
			scratch = arena_begin_temp_region(&scratch_arenas[scratch_arena_index]);
			index = scratch_arena_index;
			break;
		}
	}
	
	if (scratch.arena->ptr == NULL) {
		last_alloc_error = scratch_arenas_init_errors[index];
	}
#else
	last_alloc_error = Alloc_Error_OUT_OF_MEMORY;
#endif
	
	return scratch;
}

static void
scratch_end(Scratch scratch) {
	arena_end_temp_region(scratch);
}

////////////////////////////////
//~ Strings and slices

//- Slice functions

static SliceU8
make_sliceu8(u8 *data, i64 len) {
	SliceU8 result = {
		.data = data,
		.len  = len,
	};
	return result;
}

static SliceU8
push_sliceu8(Arena *arena, i64 len) {
	SliceU8 result = {
		.data = push(arena, cast(u64) len),
		.len  = len,
	};
	
	if (!result.data) {
		result.len = 0;
	}
	
	return result;
}

static SliceU8
sliceu8_from_string(String s) {
	return make_sliceu8(s.data, s.len);
}

static SliceU8
sliceu8_clone(Arena *arena, SliceU8 s) {
	// We don't call push_sliceu8() because that clears memory to 0 and we don't need that here.
	SliceU8 result = {
		.data = push_nozero(arena, s.len),
		.len  = s.len,
	};
	
	if (result.data) {
		memcpy(result.data, s.data, s.len);
	} else {
		result.len = 0;
	}
	
	return result;
}

//- String functions

static String
string(u8 *data, i64 len) {
	String result = {
		.data = data,
		.len  = len,
	};
	return result;
}

static String
push_string(Arena *arena, i64 len) {
	String result = {
		.data = push(arena, cast(u64) len),
		.len  = len,
	};
	
	if (!result.data) {
		result.len = 0;
	}
	
	return result;
}

static String
push_stringf(Arena *arena, char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	String result = push_stringf_va_list(arena, fmt, args);
	va_end(args);
	return result;
}

static String
push_stringf_va_list(Arena *arena, char *fmt, va_list args) {
	i64 len = vsnprintf(0, 0, fmt, args);
	String result = {
		.data = push_nozero(arena, sizeof(u8) * (len + 1)), // +1 because vsnprintf always null-terminates, even at cost of truncating the string
		.len  = len,
	};
	
	if (result.data) {
		vsnprintf(cast(char *) result.data, result.len + 1, fmt, args);
	} else {
		result.len = 0;
	}
	
	return result;
}

static String
string_from_sliceu8(SliceU8 s) {
	return string(s.data, s.len);
}

static String
string_clone(Arena *arena, String s) {
	// We don't call push_string() because that clears memory to 0 and we don't need that here.
	String result = {
		.data = push_nozero(arena, s.len),
		.len  = s.len,
	};
	
	if (result.data) {
		memcpy(result.data, s.data, s.len);
	} else {
		result.len = 0;
	}
	
	return result;
}

static String
strings_concat(Arena *arena, String *strings, i64 string_count) {
	i64 total_len = 0;
	for (i64 i = 0; i < string_count; i += 1) {
		total_len += strings[i].len;
	}
	
	String result = {
		.data = push_nozero(arena, total_len),
		.len  = total_len,
	};
	
	if (result.data) {
		i64 offset = 0;
		for (i64 i = 0; i < string_count; i += 1) {
			memcpy(result.data + offset, strings[i].data, strings[i].len);
			offset += strings[i].len;
		}
	} else {
		result.len = 0;
	}
	
	return result;
}

static char *
cstring_from_string(Arena *arena, String s) {
	char *result = push_nozero(arena, (s.len + 1) * sizeof(char));
	if (result) {
		memcpy(result, s.data, s.len);
		result[s.len] = 0;
	}
	
	return result;
}

static bool
string_starts_with(String a, String b) {
	bool result = false;
	if (a.len >= b.len) {
		int memcmp_result = memcmp(a.data, b.data, b.len);
		result = memcmp_result == 0;
	}
	return result;
}

static bool
string_ends_with(String a, String b) {
	bool result = false;
	if (a.len >= b.len) {
		int memcmp_result = memcmp(a.data + (a.len - b.len), b.data, b.len);
		result = memcmp_result == 0;
	}
	return result;
}

static bool
string_equals(String a, String b) {
	return (a.len == b.len) && (memcmp(a.data, b.data, a.len) == 0);
}

static bool
string_equals_case_insensitive(String a, String b) {
	// TODO: Unicode casings
	
	bool result = false;
	if (a.len == b.len) {
		result = true;
		for (i64 i = 0; i < a.len; i += 1) {
			if (tolower(a.data[i]) != tolower(b.data[i])) {
				result = false;
				break;
			}
		}
	}
	return result;
}

static i64
string_find_first(String s, u8 c) {
	i64 result = -1;
	for (i64 i = 0; i < s.len; i += 1) {
		if (s.data[i] == c) {
			result = i;
			break;
		}
	}
	return result;
}

static i64
string_count_occurrences(String s, u8 c) {
	i64 result = 0;
	for (i64 i = 0; i < s.len; i += 1) {
		if (s.data[i] == c) {
			result += 1;
		}
	}
	return result;
}

static i64
string_contains(String s, u8 c) {
	i64 first = string_find_first(s, c);
	return first >= 0;
}

static String
string_skip(String s, i64 amount) {
	if (amount > s.len) {
		amount = s.len;
	}
	
	s.data += amount;
	s.len  -= amount;
	
	return s;
}

static String
string_chop(String s, i64 amount) {
	if (amount > s.len) {
		amount = s.len;
	}
	
	s.len -= amount;
	
	return s;
}

static String
string_stop(String s, i64 index) {
	if (index < s.len && index > -1) {
		s.len = index;
	}
	
	return s;
}

static String
string_skip_chop_whitespace(String s) {
	for (i64 i = 0; i < s.len; i += 1) {
		if (!isspace(s.data[i])) {
			s.data += i;
			s.len  -= i;
			break;
		}
	}
	
	for (i64 i = s.len - 1; i > -1; i -= 1) {
		if (!isspace(s.data[i])) {
			s.len = i + 1;
			break;
		}
	}
	
	return s;
}

static String
string_chop_past_last_slash(String s) {
	for (i64 i = s.len - 1; i > -1; i -= 1) {
		if (s.data[i] == '\\' || s.data[i] == '/') {
			s.len = i + 1;
			break;
		}
	}
	
	return s;
}

////////////////////////////////
//~ String Builder

static void
string_builder_init(String_Builder *builder, SliceU8 backing) {
	builder->data = backing.data;
	builder->cap  = backing.len;
	builder->len  = 0;
}

static i64
string_builder_append(String_Builder *builder, String s) {
	assert(builder->data); // Not initialized
	
	i64 space = builder->cap - builder->len;
	i64 to_copy = min(space, s.len);
	memcpy(builder->data + builder->len, s.data, to_copy);
	builder->len += to_copy;
	
	return to_copy;
}

static String
string_from_builder(String_Builder builder) {
	return string(builder.data, builder.len);
}

#endif

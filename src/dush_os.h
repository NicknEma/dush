#ifndef DUSH_OS_H
#define DUSH_OS_H

////////////////////////////////
//~ Console IO

//- Console platform-specific functions

// Returns how many characters were printed.
static i64 print_unbuffered(String s);

////////////////////////////////
//~ Path manipulation

//- Platform-specific functions

static String get_separator(void); // '\' on Windows, '/' on Linux
static bool   is_separator(u8 c);  // '\' and '/' on Windows, '/' on Linux

static bool   path_is_abs(String path);

//- Platform-independent functions

static bool   is_slash(u8 c);      // '\' and '/' everywhere

static i64    path_volume_name_len(String path);
static String path_volume_name(String path);
static String path_skip_volume_name(String path);

static i64    path_last_separator(String path); // Index of the last separator

static String path_base(String path);

////////////////////////////////
//~ Basic file management

//- File types

typedef enum Access_Flags {
	Access_Flag_READ   = (1<<0),
	Access_Flag_WRITE  = (1<<1),
	Access_Flag_SHARED = (1<<2),
} Access_Flags;

typedef enum File_Flags {
	File_Flag_IS_DIRECTORY = (1<<0),
} File_Flags;

typedef enum File_Error {
	File_Error_NONE = 0,
	File_Error_EXISTS,
	File_Error_NOT_EXISTS,
	File_Error_OPEN_FAILED,
	File_Error_SEEK_FAILED,
	File_Error_READ_FAILED,
	File_Error_WRITE_FAILED,
	File_Error_ACCESS_DENIED,
	File_Error_INVALID_HANDLE,
	File_Error_IS_DIRECTORY,
	File_Error_INVALID_OFFSET,
	File_Error_OTHER,
} File_Error;

typedef struct Read_File_Result Read_File_Result;
struct Read_File_Result {
	SliceU8 contents;
	bool    ok;
};

//- File global variables

per_thread File_Error last_file_error;

//- File functions

static Read_File_Result read_file(Arena *arena, String file_name);
static String last_file_error_string(void);

////////////////////////////////
//~ File system introspection

//- File system introspection types

typedef struct File_Attributes File_Attributes;
struct File_Attributes {
	File_Flags   flags;
	Access_Flags access;
	u64    size;
	time_t created;
	time_t last_modified;
};

typedef struct File_Info File_Info;
struct File_Info {
	String name;
	File_Attributes attributes;
};

typedef struct File_Info_Node File_Info_Node;
struct File_Info_Node {
	File_Info_Node *next;
	File_Info info;
};

typedef struct File_Info_List File_Info_List;
struct File_Info_List {
	File_Info_Node *first;
	File_Info_Node *last;
	u64 count;
};

typedef struct File_Iterator File_Iterator;
struct File_Iterator {
	u8 opaque[1024];
};

//- File system introspection functions

static void file_info_list_push(Arena *arena, File_Info_List *list, File_Info info);

// Note: The memory pushed onto `arena` in this procedure must stay valid througout
// the whole file iteration.
static File_Iterator *file_iterator_begin(Arena *arena, String path);
static bool file_iterator_next(Arena *arena, File_Iterator *iterator, File_Info *info);
static void file_iterator_end(File_Iterator *iterator);

////////////////////////////////
//~ Process creation

//- Process creation types

typedef enum Process_Error {
	Process_Error_NONE = 0,
	Process_Error_FILE_NOT_FOUND,
	Process_Error_INVALID_PARAM,
	Process_Error_BAD_EXE_FORMAT,
	Process_Error_OTHER,
	Process_Error_COUNT,
} Process_Error;

//- Process creation global variables

per_thread Process_Error last_process_error;

//- Process creation functions

static bool   start_process_sync(String command_line, String working_dir);
static String last_process_error_string(void);

#endif

#include "dush_ctx_crack.h"
#include "dush_base.h"
#include "dush_os.h"

#include "dush_base.c"
#include "dush_os.c"

#include "dush.h"
#if OS_WINDOWS
# include "dush_windows.c"
#elif OS_LINUX
# include "dush_linux.c"
#endif

static String
get_line(Arena *arena) {
	String result = {0};
	
	// TODO: Handle tabs
	
	Scratch scratch = scratch_begin(&arena, 1);
	
	i64 line_buffer_cap = 256;
	i64 line_buffer_len = 0;
	u8 *line_buffer = push_nozero(scratch.arena, line_buffer_cap);
	
	if (line_buffer) {
		while (true) {
			int c = fgetc(stdin);
			if (c == '\r' || c == '\n' || c == EOF) {
				if (c == EOF) {
					print_unbuffered(string_from_lit("\n"));
				}
				break;
			}
			
			if (line_buffer_len == line_buffer_cap) {
				i64 new_line_buffer_cap = line_buffer_cap * 2;
				u8 *new_line_buffer = push_nozero(scratch.arena, new_line_buffer_cap);
				
				while (!new_line_buffer) {
					new_line_buffer_cap = new_line_buffer_cap / 2 + line_buffer_cap / 2;
					if (new_line_buffer_cap == line_buffer_cap) {
						goto main_loop_end;
					}
					
					new_line_buffer = push_nozero(scratch.arena, new_line_buffer_cap);
				}
				
				memcpy(new_line_buffer, line_buffer, line_buffer_len);
				line_buffer_cap = new_line_buffer_cap;
				line_buffer = new_line_buffer;
			}
			
			line_buffer[line_buffer_len] = cast(u8) c;
			line_buffer_len += 1;
		}
		
		main_loop_end:;
		
		result = string_clone(arena, string(line_buffer, line_buffer_len));
	}
	
	scratch_end(scratch);
	
	return result;
}

int
main(void) {
	
	init_ctrl_c_handler();
	
	bool should_exit = false;
	while (!should_exit) {
		Scratch scratch = scratch_begin(0, 0);
		
		// Print prompt
		String current_dir = get_current_directory(scratch.arena);
		printf("%.*s>", string_expand(current_dir));
		
		// Process command
		// TODO: Evaluate variables, e.g. %PATH%
		String line = get_line(scratch.arena);
		line = string_skip_chop_whitespace(line);
		
		i64 space_index = string_find_first(line, ' ');
		if (space_index < 0) space_index = line.len;
		
		String command = string_stop(line, space_index);
		String args    = string_skip(line, space_index+1);
		
		if (command.len == 0) {
		} else if (string_equals(command, string_from_lit("exit"))) {
			should_exit = true;
		} else if (string_equals(command, string_from_lit("help"))) {
			printf("This is the help text.\n");
		} else if (string_equals(command, string_from_lit("pwd"))) {
			printf("%.*s\n", string_expand(current_dir));
		} else if (string_starts_with(command, string_from_lit("cd"))) {
			if (args.len == 0) {
				printf("%.*s\n", string_expand(current_dir));
			} else {
				set_current_directory(args);
			}
		} else {
			// Try to start a process or run a script
			
			if (!start_process_sync(line, current_dir)) {
				
				// Don't treat FILE_NOT_FOUND and BAD_EXE_FORMAT as errors.
				// If the file wasn't found, print the specialized error message later;
				// If the file is not a valid executable, it probabily is some other kind of
				// file which will be interpreted according to its extension.
				//
				// TODO: Is this the expected behaviour on Linux?
				if (last_process_error != Process_Error_FILE_NOT_FOUND &&
					last_process_error != Process_Error_BAD_EXE_FORMAT) {
					
					// Note: On Windows, 'command' might not match the exact executable that CreateProcessA
					// tried to spawn (for example, the command might be 'dush' but the chosen executable
					// is 'dush.exe').
					//
					// There is not an easy way to know which file was chosen as the executable; the only way
					// is to simply replicate all the steps the OS did, as written in the documentation
					// for CreateProcessA.
					//
					// It's not super important, so we just use 'command'.
					fprintf(stderr, "Error starting process '%.*s': %.*s\n\n", string_expand(command), string_expand(last_process_error_string()));
				} else {
					// Find a file in this folder with the .dush extension
					// If nothing is found, search in the path
					
					// If the command has no extension, add a '.dush' extension,
					// otherwise leave it as it is.
					String extension = string_from_lit(".dush");
					String file_name = command;
					{
						String base = path_base(command);
						if (!string_contains(base, '.')) {
							String temp[] = {command, extension};
							file_name = strings_concat(scratch.arena, temp, array_count(temp));
						}
					}
					
					if (string_ends_with(file_name, extension)) {
						// Simply read the whole file. If it doesn't fit in memory, don't run it at all.
						Read_File_Result script_read = read_file(scratch.arena, file_name);
						if (!script_read.ok && last_file_error == File_Error_NOT_EXISTS) {
							// Search in the PATH if the file name does not contain a path,
							// e.g. "build.dush" (with nothing before).
							
							String base = path_base(file_name);
							if (base.len == file_name.len) {
								String system_path = get_system_path(scratch.arena);
								
								for (;;) {
									i64 split_index = string_find_first(system_path, ';');
									if (split_index < 0) break;
									
									String loc  = string_stop(system_path, split_index);
									system_path = string_skip(system_path, split_index + 1);
									
									String full_path = {0};
									{
										String temp[] = {loc, get_separator(), file_name};
										full_path = strings_concat(scratch.arena, temp, array_count(temp));
									}
									
									script_read = read_file(scratch.arena, full_path);
									if (script_read.ok || last_file_error != File_Error_NOT_EXISTS) {
										// Either we loaded the file correctly, or we found it but couldn't
										// read it for some other reason: stop searching.
										break;
									}
								}
								
								allow_break();
							}
						}
						
						if (script_read.ok) {
							printf("Running script '%.*s'...\n\n", string_expand(file_name));
						} else {
							if (last_file_error != File_Error_NOT_EXISTS) {
								fprintf(stderr, "Error running script '%.*s': %.*s\n\n", string_expand(file_name), string_expand(last_file_error_string()));
							} else {
								fprintf(stderr, "'%.*s' is not a known command, executable file or dush script in the current directory or in the path.\n\n", string_expand(command));
							}
						}
					} else if (string_ends_with(file_name, string_from_lit(".txt"))) {
						// This is an example of how the if-else chain can be continued.
						// Associate each extension with the selected program that opens it.
						//
						// For now, do nothing.
						
						allow_break();
					}
				}
			}
			
			allow_break();
		}
		
		printf("\n");
		
		scratch_end(scratch);
		allow_break();
	}
	
	return 0;
}

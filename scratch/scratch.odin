package scratch

import "core:fmt"
import "core:path/filepath"

main :: proc() {
	s: string;
	
	s = `C:\path\file`;
	fmt.printf("%v -> %v\n", s, filepath.volume_name_len(s));
	
	s = `\\server\share\path\file`;
	fmt.printf("%v -> %v\n", s, filepath.volume_name_len(s));
	
	s = `\\a\\server\share\path\file`;
	fmt.printf("%v -> %v\n", s, filepath.volume_name_len(s));
	
	s = `\\.server`;
	fmt.printf("%v -> %v\n", s, filepath.volume_name_len(s));
	
	s = `\\?\Volume{26a21bda-a627-11d7-9931-806e6f6e6963}\path\file`;
	fmt.printf("%v -> %v\n", s, filepath.volume_name_len(s));
	
	s = `\\?\.aaaaaaaaaaaaaaaaaaaaaaa`;
	fmt.printf("%v -> %v\n", s, filepath.volume_name_len(s));
	
	s = `\\?\aaaaaaaaaaaaaaaaaaaaaaaa`;
	fmt.printf("%v -> %v\n", s, filepath.volume_name_len(s));
	
	s = `\\?\a`;
	fmt.printf("%v -> %v\n", s, filepath.volume_name_len(s));
	
	s = `\\?\a\`;
	fmt.printf("%v -> %v\n", s, filepath.volume_name_len(s));
	
	s = `\\?\\\\\\\`;
	fmt.printf("%v -> %v\n", s, filepath.volume_name_len(s));
	
	s = `C:\MountD\path\file`;
	fmt.printf("%v -> %v\n", s, filepath.volume_name_len(s));
	
	return;
}

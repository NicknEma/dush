#include <stdio.h>

int
main(void) {
	char buf[1024] = {0};
	int buf_len = 0;
	
	while (buf_len < sizeof(buf)) {
		char c = getchar();
		if (c == '\r' || c == '\n' || c == EOF) break;
		
		buf[buf_len] = c;
		buf_len += 1;
	}
	
	printf("Hello, %s!\n", buf);
	
	return 0;
}

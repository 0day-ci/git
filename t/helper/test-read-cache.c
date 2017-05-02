#include "cache.h"

int cmd_main(int argc, const char **argv)
{
	int i, cnt = 1;
	if (argc == 2)
		cnt = strtol(argv[1], NULL, 0);
	setup_git_directory();
	for (i = 0; i < cnt; i++) {
		read_index(&the_index);
		discard_index(&the_index);
	}
	return 0;
}

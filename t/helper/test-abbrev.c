#include "cache.h"
#include <stdio.h>

int cmd_main(int ac, const char **av)
{
	struct object_id oid;
	char hex[GIT_MAX_HEXSZ + 2];
	const char *end;

	setup_git_directory();

	while (fgets(hex, GIT_MAX_HEXSZ + 2, stdin)) {
		hex[GIT_MAX_HEXSZ] = 0;
		if (!parse_oid_hex(hex, &oid, &end))
			find_unique_abbrev(oid.hash, MINIMUM_ABBREV);
	}

	exit(0);
}

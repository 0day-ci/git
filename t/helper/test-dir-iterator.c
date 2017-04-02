#include "git-compat-util.h"
#include "strbuf.h"
#include "iterator.h"
#include "dir-iterator.h"

int cmd_main(int argc, const char **argv) {
	struct strbuf path = STRBUF_INIT;
	struct dir_iterator *diter;

	if (argc < 2) {
		return 1;
	}

	strbuf_add(&path, argv[1], strlen(argv[1]));

	diter = dir_iterator_begin(path.buf);

	while (dir_iterator_advance(diter) == ITER_OK) {
		if (S_ISDIR(diter->st.st_mode))
			printf("[d] ");
		else
			printf("[f] ");

		printf("(%s) %s\n", diter->relative_path, diter->path.buf);
	}

	return 0;
}

#include "git-compat-util.h"
#include "strbuf.h"
#include "iterator.h"
#include "dir-iterator.h"

int cmd_main(int argc, const char **argv) {
	struct strbuf path = STRBUF_INIT;
	struct dir_iterator *diter;
	unsigned flag = DIR_ITERATOR_PRE_ORDER_TRAVERSAL;

	if (argc < 2) {
		return 1;
	}

	strbuf_add(&path, argv[1], strlen(argv[1]));

	if (argc == 3)
		flag = atoi(argv[2]);

	diter = dir_iterator_begin(path.buf, flag);

	while (dir_iterator_advance(diter) == ITER_OK) {
		if (S_ISDIR(diter->st.st_mode))
			printf("[d] ");
		else
			printf("[f] ");

		printf("(%s) %s\n", diter->relative_path, diter->path.buf);
	}

	return 0;
}

#include "cache.h"
#include "parse-options.h"

uint64_t time_runs(int count)
{
	uint64_t t0, t1;
	uint64_t sum = 0;
	uint64_t avg;
	int i;

	for (i = 0; i < count; i++) {
		t0 = getnanotime();
		read_cache();
		t1 = getnanotime();

		sum += (t1 - t0);

		printf("%f %d [cache_nr %d]\n",
			   ((double)(t1 - t0))/1000000000,
			   skip_verify_index,
			   the_index.cache_nr);
		fflush(stdout);

		discard_cache();
	}

	avg = sum / count;
	if (count > 1) {
		printf("%f %d avg\n",
			   (double)avg/1000000000,
			   skip_verify_index);
		fflush(stdout);
	}

	return avg;
}

int cmd_main(int argc, const char **argv)
{
	int count = 1;
	const char *usage[] = {
		"test-core-verify-index",
		NULL
	};
	struct option options[] = {
		OPT_INTEGER('c', "count", &count, "number of passes"),
		OPT_END(),
	};
	const char *prefix;
	uint64_t avg_no_skip, avg_skip;

	prefix = setup_git_directory();
	argc = parse_options(argc, argv, prefix, options, usage, 0);

	if (count < 1)
		die("count must greater than zero");

	/* throw away call to get the index in the disk cache */
	read_cache();
	discard_cache();

	/* disable index SHA verification */
	skip_verify_index = 0;
	avg_no_skip = time_runs(count);

	/* enable index SHA verification */
	skip_verify_index = 1;
	avg_skip = time_runs(count);

	if (avg_skip > avg_no_skip)
		die("skipping index SHA verification did not help");
	return 0;
}

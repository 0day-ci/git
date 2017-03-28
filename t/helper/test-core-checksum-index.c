#include "cache.h"
#include "parse-options.h"

uint64_t time_runs(int do_checksum, int count)
{
	uint64_t t0, t1;
	uint64_t sum = 0;
	uint64_t avg;
	int i;

	git_config_set_gently("core.checksumindex",
		(do_checksum ? "true" : "false"));

	for (i = 0; i < count; i++) {
		t0 = getnanotime();
		read_cache();
		t1 = getnanotime();

		sum += (t1 - t0);

		printf("%f %d [cache_nr %d]\n",
			   ((double)(t1 - t0))/1000000000,
			   do_checksum,
			   the_index.cache_nr);
		fflush(stdout);

		discard_cache();
	}

	avg = sum / count;
	if (count > 1) {
		printf("%f %d avg\n",
			   (double)avg/1000000000,
			   do_checksum);
		fflush(stdout);
	}

	return avg;
}

int cmd_main(int argc, const char **argv)
{
	int original_do_checksum;
	int count = 1;
	const char *usage[] = {
		"test-core-checksum-index",
		NULL
	};
	struct option options[] = {
		OPT_INTEGER('c', "count", &count, "number of passes"),
		OPT_END(),
	};
	const char *prefix;
	uint64_t avg_0, avg_1;

	prefix = setup_git_directory();
	argc = parse_options(argc, argv, prefix, options, usage, 0);

	if (count < 1)
		die("count must greater than zero");

	/* throw away call to get the index in the disk cache */
	read_cache();
	discard_cache();

	git_config_get_bool("core.checksumindex", &original_do_checksum);

	avg_1 = time_runs(1, count);
	avg_0 = time_runs(0, count);

	git_config_set_gently("core.checksumindex",
		((original_do_checksum) ? "true" : "false"));

	if (avg_0 >= avg_1)
		die("skipping index checksum verification did not help");
	return 0;
}


#include "cache.h"
#include "exec_cmd.h"
#include "parse-options.h"
#include "pkt-line.h"

static const char * const upload_file_usage[] = {
	N_("git upload-file [<options>] <dir>"),
	NULL
};


static void upload_file(void)
{
	for (;;) {
		char *line = packet_read_line(0, NULL);
		const char *arg;
		if (!line)
			break;

		if (skip_prefix(line, "info ", &arg)) {
			unsigned char sha1[20];
			void *buffer;
			enum object_type type;
			unsigned long size;

			if (get_sha1_hex(arg, sha1))
				die("invalid sha: %s", arg);

			buffer = read_sha1_file(sha1, &type, &size);
			if (buffer) {
				packet_write_fmt(1, "found %s %d %ld\n", sha1_to_hex(sha1), type, size);
				free(buffer);
			} else {
				packet_write_fmt(1, "missing %s\n", sha1_to_hex(sha1));
			}
		}

		if (skip_prefix(line, "get ", &arg)) {
			unsigned char sha1[20];
			void *buffer;
			enum object_type type;
			unsigned long size;

			if (get_sha1_hex(arg, sha1))
				die("invalid sha: %s", arg);

			buffer = read_sha1_file(sha1, &type, &size);
			if (buffer) {
				packet_write_fmt(1, "found %s %d %ld\n", sha1_to_hex(sha1), type, size);
				write_or_die(1, buffer, size);
				free(buffer);
			} else {
				packet_write_fmt(1, "missing %s\n", sha1_to_hex(sha1));
			}
			
		}

		if (!strcmp(line, "end"))
			break;
	}
}

int cmd_main(int argc, const char **argv)
{
	const char *dir;
	struct option options[] = {
		OPT_END()
	};

	packet_trace_identity("upload-file");

	argc = parse_options(argc, argv, NULL, options, upload_file_usage, 0);

	if (argc != 1)
		usage_with_options(upload_file_usage, options);

	setup_path();

	dir = argv[0];

	if (!enter_repo(dir, 0))
		die("'%s' does not appear to be a git repository", dir);

	upload_file();
	return 0;
}

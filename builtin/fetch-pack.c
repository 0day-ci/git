#include "builtin.h"
#include "pkt-line.h"
#include "fetch-pack.h"
#include "remote.h"
#include "connect.h"
#include "sha1-array.h"
#include "refs.h"

static const char fetch_pack_usage[] =
"git fetch-pack [--all] [--stdin] [--quiet | -q] [--keep | -k] [--thin] "
"[--include-tag] [--upload-pack=<git-upload-pack>] [--depth=<n>] "
"[--no-progress] [--diag-url] [-v] [<host>:]<directory> [<refs>...]";

static void add_sought_entry(struct refspec **sought, int *nr, int *alloc,
			     const char *name)
{
	(*nr)++;
	ALLOC_GROW(*sought, *nr, *alloc);
	parse_ref_or_pattern(&(*sought)[*nr - 1], name);
}

int cmd_fetch_pack(int argc, const char **argv, const char *prefix)
{
	int i, ret;
	struct ref *ref = NULL;
	const char *dest = NULL;
	struct refspec *sought = NULL;
	int nr_sought = 0, alloc_sought = 0;
	const struct ref **sought_refs;
	int nr_sought_refs;
	int fd[2];
	char *pack_lockfile = NULL;
	char **pack_lockfile_ptr = NULL;
	struct child_process *conn;
	struct fetch_pack_args args;
	struct sha1_array shallow = SHA1_ARRAY_INIT;
	struct string_list deepen_not = STRING_LIST_INIT_DUP;
	int always_print_refs = 0;

	packet_trace_identity("fetch-pack");

	memset(&args, 0, sizeof(args));
	args.uploadpack = "git-upload-pack";

	for (i = 1; i < argc && *argv[i] == '-'; i++) {
		const char *arg = argv[i];

		if (skip_prefix(arg, "--upload-pack=", &arg)) {
			args.uploadpack = arg;
			continue;
		}
		if (skip_prefix(arg, "--exec=", &arg)) {
			args.uploadpack = arg;
			continue;
		}
		if (!strcmp("--quiet", arg) || !strcmp("-q", arg)) {
			args.quiet = 1;
			continue;
		}
		if (!strcmp("--keep", arg) || !strcmp("-k", arg)) {
			args.lock_pack = args.keep_pack;
			args.keep_pack = 1;
			continue;
		}
		if (!strcmp("--thin", arg)) {
			args.use_thin_pack = 1;
			continue;
		}
		if (!strcmp("--include-tag", arg)) {
			args.include_tag = 1;
			continue;
		}
		if (!strcmp("--all", arg)) {
			args.fetch_all = 1;
			continue;
		}
		if (!strcmp("--stdin", arg)) {
			args.stdin_refs = 1;
			continue;
		}
		if (!strcmp("--diag-url", arg)) {
			args.diag_url = 1;
			continue;
		}
		if (!strcmp("-v", arg)) {
			args.verbose = 1;
			continue;
		}
		if (skip_prefix(arg, "--depth=", &arg)) {
			args.depth = strtol(arg, NULL, 0);
			continue;
		}
		if (skip_prefix(arg, "--shallow-since=", &arg)) {
			args.deepen_since = xstrdup(arg);
			continue;
		}
		if (skip_prefix(arg, "--shallow-exclude=", &arg)) {
			string_list_append(&deepen_not, arg);
			continue;
		}
		if (!strcmp(arg, "--deepen-relative")) {
			args.deepen_relative = 1;
			continue;
		}
		if (!strcmp("--no-progress", arg)) {
			args.no_progress = 1;
			continue;
		}
		if (!strcmp("--stateless-rpc", arg)) {
			args.stateless_rpc = 1;
			continue;
		}
		if (!strcmp("--lock-pack", arg)) {
			args.lock_pack = 1;
			pack_lockfile_ptr = &pack_lockfile;
			continue;
		}
		if (!strcmp("--check-self-contained-and-connected", arg)) {
			args.check_self_contained_and_connected = 1;
			continue;
		}
		if (!strcmp("--cloning", arg)) {
			args.cloning = 1;
			continue;
		}
		if (!strcmp("--update-shallow", arg)) {
			args.update_shallow = 1;
			continue;
		}
		if (!strcmp("--always-print-refs", arg)) {
			always_print_refs = 1;
			continue;
		}
		usage(fetch_pack_usage);
	}
	if (deepen_not.nr)
		args.deepen_not = &deepen_not;

	if (i < argc)
		dest = argv[i++];
	else
		usage(fetch_pack_usage);

	/*
	 * Copy refs from cmdline to growable list, then append any
	 * refs from the standard input:
	 */
	for (; i < argc; i++)
		add_sought_entry(&sought, &nr_sought, &alloc_sought, argv[i]);
	if (args.stdin_refs) {
		if (args.stateless_rpc) {
			/* in stateless RPC mode we use pkt-line to read
			 * from stdin, until we get a flush packet
			 */
			for (;;) {
				char *line = packet_read_line(0, NULL);
				if (!line)
					break;
				add_sought_entry(&sought, &nr_sought,  &alloc_sought, line);
			}
		}
		else {
			/* read from stdin one ref per line, until EOF */
			struct strbuf line = STRBUF_INIT;
			while (strbuf_getline_lf(&line, stdin) != EOF)
				add_sought_entry(&sought, &nr_sought, &alloc_sought, line.buf);
			strbuf_release(&line);
		}
	}

	if (args.stateless_rpc) {
		conn = NULL;
		fd[0] = 0;
		fd[1] = 1;
	} else {
		int flags = args.verbose ? CONNECT_VERBOSE : 0;
		if (args.diag_url)
			flags |= CONNECT_DIAG_URL;
		conn = git_connect(fd, dest, args.uploadpack,
				   flags);
		if (!conn)
			return args.diag_url ? 0 : 1;
	}
	get_remote_heads(fd[0], NULL, 0, &ref, 0, NULL, &shallow);
	get_ref_array(&sought_refs, &nr_sought_refs, ref, sought, nr_sought);

	ref = fetch_pack(&args, fd, conn, ref, dest, sought, nr_sought,
			 sought_refs, nr_sought_refs, &shallow,
			 pack_lockfile_ptr);
	if (pack_lockfile) {
		printf("lock %s\n", pack_lockfile);
		fflush(stdout);
	}
	if (args.check_self_contained_and_connected &&
	    args.self_contained_and_connected) {
		printf("connectivity-ok\n");
		fflush(stdout);
	}
	if (finish_connect(conn)) {
		ret = 1;
		goto cleanup;
	}

	ret = !ref;

	/*
	 * If the heads to pull were given, we should have consumed
	 * all of them by matching the remote.  Otherwise, 'git fetch
	 * remote no-such-ref' would silently succeed without issuing
	 * an error.
	 */
	for (i = 0; i < nr_sought; i++) {
		struct ref *r;
		if (sought[i].pattern)
			continue; /* patterns do not need to match anything */
		for (r = ref; r; r = r->next) {
			if (refname_match(sought[i].src, r->name))
				break;
		}
		if (r)
			continue;
		error("no such remote ref %s", sought[i].src);
		ret = 1;
	}

	if (args.stateless_rpc && !always_print_refs)
		goto cleanup;

	while (ref) {
		printf("%s %s\n",
		       oid_to_hex(&ref->old_oid), ref->name);
		ref = ref->next;
	}
	fflush(stdout);

cleanup:
	close(fd[0]);
	close(fd[1]);
	return ret;
}

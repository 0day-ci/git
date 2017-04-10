#include "cache.h"
#include "pkt-line.h"
#include "refs.h"
#include "revision.h"
#include "run-command.h"

static const char * const server_endpoint_usage[] = {
	N_("git server-endpoint [<options>] <dir>"),
	NULL
};

static const char *capabilities = "multi_ack_detailed side-band-64k shallow";

struct handle_want_data {
	int upload_pack_in_fd;
	int capabilities_sent;
	struct string_list sent_namespaced_names;
};

static int send_want(const char *namespaced_name, const struct object_id *oid,
		     int flags, void *handle_want_data)
{
	struct handle_want_data *data = handle_want_data;

	if (ref_is_hidden(strip_namespace(namespaced_name), namespaced_name))
		return 0;
	if (string_list_lookup(&data->sent_namespaced_names, namespaced_name))
		return 0;

	string_list_insert(&data->sent_namespaced_names, namespaced_name);

	if (data->capabilities_sent) {
		packet_write_fmt(data->upload_pack_in_fd, "want %s\n",
				 oid_to_hex(oid));
	} else {
		packet_write_fmt(data->upload_pack_in_fd, "want %s%s\n",
				 oid_to_hex(oid), capabilities);
		data->capabilities_sent = 1;
	}

	return 0;
}

static void handle_want(const char *arg, struct handle_want_data *data) {
	char *namespaced_name = xstrfmt("%s%s", get_git_namespace(), arg);
	if (has_glob_specials(arg)) {
		for_each_glob_ref(send_want, namespaced_name, data);
	} else {
		struct object_id oid;
		if (!read_ref(namespaced_name, oid.hash))
			send_want(namespaced_name, &oid, 0, data);
	}
	free(namespaced_name);
}

static int fetch_ref(int stateless_rpc)
{
	struct child_process cmd = CHILD_PROCESS_INIT;
	static const char *argv[] = {
		"upload-pack", ".", NULL, NULL
	};
	struct handle_want_data handle_want_data = {0, 0, STRING_LIST_INIT_DUP};

	char *line;
	int size;

	int upload_pack_will_respond = 0;
	int wanted_refs_sent = 0;

	if (stateless_rpc)
		argv[2] = "--stateless-rpc";
	cmd.argv = argv;
	cmd.git_cmd = 1;
	cmd.in = -1;
	cmd.out = -1;

	if (start_command(&cmd))
		goto error;

	handle_want_data.upload_pack_in_fd = cmd.in;

	if (!stateless_rpc) {
		/* Drain the initial ref advertisement (until flush-pkt). */
		while (packet_read_line(cmd.out, NULL))
			;
	}

	/* Send the wants. Upload-pack will not respond to this unless a depth
	 * request is made. */
	while ((line = packet_read_line(0, NULL))) {
		const char *arg;
		if (skip_prefix(line, "want ", &arg)) {
			handle_want(arg, &handle_want_data);
		} else if (starts_with(line, "shallow ")) {
			packet_write_fmt(cmd.in, "%s", line);
		} else if (starts_with(line, "deepen ") ||
			   starts_with(line, "deepen-since ") ||
			   starts_with(line, "deepen-not ")) {
			packet_write_fmt(cmd.in, "%s", line);
			upload_pack_will_respond = 1;
		}
	}
	packet_flush(cmd.in);

	if (upload_pack_will_respond) {
		while ((line = packet_read_line(cmd.out, NULL))) {
			packet_write_fmt(1, "%s", line);
		}
		packet_flush(1);
	}

	/* Continue to copy the conversation. */
	do {
		char buffer[LARGE_PACKET_DATA_MAX];
		char size_buffer[5]; /* 4 bytes + NUL */
		int done_received = 0;
		int ready_received = 0;
		int options = PACKET_READ_CHOMP_NEWLINE;

		while ((line = packet_read_line(0, NULL))) {
			packet_write_fmt(cmd.in, "%s", line);
			if (!strcmp(line, "done")) {
				done_received = 1;
				/* "done" also marks the end of the request. */
				goto after_flush;
			}
		}
		packet_flush(cmd.in);
after_flush:
		while ((size = packet_read(cmd.out, NULL, NULL, buffer,
					   sizeof(buffer), options))) {
			int send_wanted_refs = 0;
			if (!wanted_refs_sent) {
				if ((done_received || ready_received) &&
				    size == strlen("ACK ") + GIT_SHA1_HEXSZ &&
				    starts_with(buffer, "ACK "))
					send_wanted_refs = 1;
				else if (done_received && !strcmp(buffer, "NAK"))
					send_wanted_refs = 1;
				else if (size == strlen("ACK  ready") + GIT_SHA1_HEXSZ &&
					 starts_with(buffer, "ACK ") &&
					 !strcmp(buffer + strlen("ACK  ") + GIT_SHA1_HEXSZ, "ready"))
					ready_received = 1;
			}
			if (send_wanted_refs) {
				struct string_list_item *item;
				for_each_string_list_item(item,
							  &handle_want_data.sent_namespaced_names) {
					struct object_id oid;
					if (read_ref(item->string, oid.hash))
						die("something happened");
					packet_write_fmt(1, "wanted %s %s",
							 oid_to_hex(&oid),
							 strip_namespace(item->string));
				}
				wanted_refs_sent = 1;
				/* Do not chomp any more characters because
				 * binary data (packfile) is about to be sent.
				 */
				options = 0;
			}
			sprintf(size_buffer, "%04x", size + 4);
			write_or_die(1, size_buffer, 4);
			write_or_die(1, buffer, size);
			if (!wanted_refs_sent && !strcmp(buffer, "NAK")) {
				/* NAK before we send wanted refs marks the end
				 * of the response. */
				goto after_flush_2;
			}
		}
		packet_flush(1);
after_flush_2:
		;
	} while (!stateless_rpc && !wanted_refs_sent);

	close(cmd.in);
	cmd.in = -1;
	close(cmd.out);
	cmd.out = -1;

	if (finish_command(&cmd))
		return -1;

	return 0;

error:

	if (cmd.in >= 0)
		close(cmd.in);
	if (cmd.out >= 0)
		close(cmd.out);
	return -1;
}

static int server_endpoint_config(const char *var, const char *value, void *unused)
{
	return parse_hide_refs_config(var, value, "uploadpack");
}

int cmd_main(int argc, const char **argv)
{
	int stateless_rpc = 0;

	struct option options[] = {
		OPT_BOOL(0, "stateless-rpc", &stateless_rpc,
			 N_("quit after a single request/response exchange")),
		OPT_END()
	};

	char *line;

	packet_trace_identity("server-endpoint");
	check_replace_refs = 0;

	argc = parse_options(argc, argv, NULL, options, server_endpoint_usage, 0);

	if (argc != 1)
		die("must have 1 arg");

	if (!enter_repo(argv[0], 0))
		die("does not appear to be a git repository");
	git_config(server_endpoint_config, NULL);

	line = packet_read_line(0, NULL);
	if (!strcmp(line, "fetch-refs"))
		return fetch_ref(stateless_rpc);
	die("only fetch-refs is supported");
}

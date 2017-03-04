#include "transport.h"
#include "pkt-line.h"
#include "remote.h"
#include "commit.h"

static struct remote *remote = NULL;
static struct transport *transport = NULL;
static int fd[2];
static int connected;

struct trace_key trace_on_demand = TRACE_KEY_INIT(ON_DEMAND);

static void on_demand_cleanup(void)
{
	if (connected) {
		packet_write_fmt(fd[1], "end\n");
		transport_disconnect(transport);
		connected = 0;
	}
}

static int on_demand_connect(void)
{
	if (!connected) {
		if (!remote)
			remote = remote_get(NULL);
		if (remote && !transport)
			transport = transport_get(remote, NULL);
		if (!remote || !transport)
			return 0;
		if (transport_connect(transport, transport->url, "git-upload-file", fd))
			return 0;
		connected = 1;
		atexit(on_demand_cleanup);
	}
	return 1;
}

void *read_remote_on_demand(const unsigned char *sha1, enum object_type *type,
			    unsigned long *size)
{
	const char *line;
	const char *arg;
	int line_size;

	if (!on_demand_connect())
		return NULL;

	packet_write_fmt(fd[1], "get %s\n", sha1_to_hex(sha1));

	line = packet_read_line(fd[0], &line_size);

	if (line_size == 0)
		return NULL;

	if (skip_prefix(line, "missing ", &arg))
		return NULL;

	if (skip_prefix(line, "found ", &arg)) {
		char *end = NULL;
		void *buffer;
		unsigned char file_sha1[GIT_SHA1_RAWSZ];
		enum object_type file_type;
		unsigned long file_size;
		ssize_t size_read;

		if (get_sha1_hex(arg, file_sha1))
			die("git on-demand: protocol error, "
			    "expected to get sha in '%s'", line);
		arg += GIT_SHA1_HEXSZ;

		file_type = strtol(arg, &end, 0);
		if (!end || file_type < 0 || file_type >= OBJ_MAX)
			die("git on-demand: protocol error, "
			    "invalid object type in '%s'", line);
		arg = end;

		file_size = strtoul(arg, &end, 0);
		if (!end || *end || file_size > LONG_MAX)
			die("git on-demand: protocol error, "
			    "invalid file size in '%s'", line);

		buffer = xmalloc(file_size);
		if (!buffer)
			die("git on-demand: failed to allocate "
			    "buffer for %ld bytes", file_size);

		size_read = read_in_full(fd[0], buffer, file_size);
		if (size_read != (ssize_t)file_size)
			die("git on-demand: protocol error, "
			    "failed to read file data");

		trace_printf_key(&trace_on_demand, "on-demand: fetched %s\n",
				 sha1_to_hex(sha1));
		*type = file_type;
		*size = file_size;
		return buffer;
	}

	die("git on-demand: protocol error, "
	    "unexpected response: '%s'", line);
}

int object_info_on_demand(const unsigned char *sha1, struct object_info *oi)
{
	const char *line;
	const char *arg;
	int line_size;

	if (!on_demand_connect())
		return -1;

	packet_write_fmt(fd[1], "info %s\n", sha1_to_hex(sha1));

	line = packet_read_line(fd[0], &line_size);

	if (line_size == 0)
		return -1;

	if (skip_prefix(line, "missing ", &arg))
		return -1;

	if (skip_prefix(line, "found ", &arg)) {
		char *end = NULL;
		unsigned char sha1[GIT_SHA1_RAWSZ];
		enum object_type file_type;
		unsigned long file_size;

		if (get_sha1_hex(arg, sha1))
			die("git on-demand: protocol error, "
			    "expected to get sha in '%s'", line);
		arg += GIT_SHA1_HEXSZ;

		file_type = strtol(arg, &end, 0);
		if (!end || file_type < 0 || file_type >= OBJ_MAX)
			die("git on-demand: protocol error, "
			    "invalid object type in '%s'", line);
		arg = end;

		file_size = strtoul(arg, &end, 0);
		if (!end || *end || file_size > LONG_MAX)
			die("git on-demand: protocol error, "
			    "invalid file size in '%s'", line);

		if (oi->typep)
			*oi->typep = file_type;
		if (oi->sizep)
			*oi->typep = file_size;
		if (oi->disk_sizep)
			*oi->disk_sizep = 0;
		oi->whence = OI_ONDEMAND;
		return 0;
	}

	die("git on-demand: protocol error, "
	    "unexpected response: '%s'", line);
}

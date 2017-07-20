#include "cache.h"
#include "promised-object.h"
#include "sha1-lookup.h"
#include "strbuf.h"
#include "run-command.h"
#include "sha1-array.h"
#include "config.h"
#include "sigchain.h"
#include "sub-process.h"
#include "pkt-line.h"

#define ENTRY_SIZE (GIT_SHA1_RAWSZ + 1 + 8)
/*
 * A mmap-ed byte array of size (promised_object_nr * ENTRY_SIZE). Each
 * ENTRY_SIZE-sized entry consists of the SHA-1 of the promised object, its
 * 8-bit object type, and its 64-bit size in network byte order. The entries
 * are sorted in ascending SHA-1 order.
 */
static char *promised_objects;
static int64_t promised_object_nr = -1;

static void prepare_promised_objects(void)
{
	char *filename;
	int fd;
	struct stat st;

	if (promised_object_nr >= 0)
		return;

	if (!repository_format_promised_objects ||
	    getenv("GIT_IGNORE_PROMISED_OBJECTS")) {
		promised_object_nr = 0;
		return;
	}

	filename = xstrfmt("%s/promised", get_object_directory());
	fd = git_open(filename);
	if (fd < 0) {
		if (errno == ENOENT) {
			promised_object_nr = 0;
			goto cleanup;
		}
		perror("prepare_promised_objects");
		die("Could not open %s", filename);
	}
	if (fstat(fd, &st)) {
		perror("prepare_promised_objects");
		die("Could not stat %s", filename);
	}
	if (st.st_size == 0) {
		promised_object_nr = 0;
		goto cleanup;
	}
	if (st.st_size % ENTRY_SIZE) {
		die("Size of %s is not a multiple of %d", filename, ENTRY_SIZE);
	}

	promised_objects = xmmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	promised_object_nr = st.st_size / ENTRY_SIZE;

cleanup:
	free(filename);
	if (fd >= 0)
		close(fd);
}

int is_promised_object(const struct object_id *oid, enum object_type *type,
		       unsigned long *size)
{
	int result;

	prepare_promised_objects();
	result = sha1_entry_pos(promised_objects, ENTRY_SIZE, 0, 0,
				promised_object_nr, promised_object_nr,
				oid->hash);
	if (result >= 0) {
		if (type) {
			char *ptr = promised_objects +
				    result * ENTRY_SIZE + GIT_SHA1_RAWSZ;
			*type = *ptr;
		}
		if (size) {
			uint64_t size_nbo;
			char *ptr = promised_objects +
				    result * ENTRY_SIZE + GIT_SHA1_RAWSZ + 1;
			memcpy(&size_nbo, ptr, sizeof(size_nbo));
			*size = ntohll(size_nbo);
		}
		return 1;
	}
	return 0;
}

int for_each_promised_object(each_promised_object_fn cb, void *data)
{
	struct object_id oid;
	int i, r;

	prepare_promised_objects();
	for (i = 0; i < promised_object_nr; i++) {
		memcpy(oid.hash, &promised_objects[i * ENTRY_SIZE],
		       GIT_SHA1_RAWSZ);
		r = cb(&oid, data);
		if (r)
			return r;
	}
	return 0;
}

int fsck_promised_objects(void)
{
	int i;
	prepare_promised_objects();
	for (i = 0; i < promised_object_nr; i++) {
		enum object_type type;
		if (i != 0 && memcmp(&promised_objects[(i - 1) * ENTRY_SIZE],
				     &promised_objects[i * ENTRY_SIZE],
				     GIT_SHA1_RAWSZ) >= 0)
			return error("Error in list of promised objects: not "
				     "in ascending order of object name");
		type = promised_objects[i * ENTRY_SIZE + GIT_SHA1_RAWSZ];
		switch (type) {
			case OBJ_BLOB:
			case OBJ_TREE:
			case OBJ_COMMIT:
			case OBJ_TAG:
				break;
			default:
				return error("Error in list of promised "
					     "objects: found object of type %d",
					     type);
		}
	}
	return 0;
}

#define CAP_GET    (1u<<0)

static int subprocess_map_initialized;
static struct hashmap subprocess_map;

struct read_object_process {
	struct subprocess_entry subprocess;
	unsigned int supported_capabilities;
};

int start_read_object_fn(struct subprocess_entry *subprocess)
{
	int err;
	struct read_object_process *entry = (struct read_object_process *)subprocess;
	struct child_process *process;
	struct string_list cap_list = STRING_LIST_INIT_NODUP;
	char *cap_buf;
	const char *cap_name;

	process = subprocess_get_child_process(&entry->subprocess);

	sigchain_push(SIGPIPE, SIG_IGN);

	err = packet_writel(process->in, "git-read-object-client", "version=1", NULL);
	if (err)
		goto done;

	err = strcmp(packet_read_line(process->out, NULL), "git-read-object-server");
	if (err) {
		error("external process '%s' does not support read-object protocol version 1", subprocess->cmd);
		goto done;
	}
	err = strcmp(packet_read_line(process->out, NULL), "version=1");
	if (err)
		goto done;
	err = packet_read_line(process->out, NULL) != NULL;
	if (err)
		goto done;

	err = packet_writel(process->in, "capability=get", NULL);
	if (err)
		goto done;

	for (;;) {
		cap_buf = packet_read_line(process->out, NULL);
		if (!cap_buf)
			break;
		string_list_split_in_place(&cap_list, cap_buf, '=', 1);

		if (cap_list.nr != 2 || strcmp(cap_list.items[0].string, "capability"))
			continue;

		cap_name = cap_list.items[1].string;
		if (!strcmp(cap_name, "get")) {
			entry->supported_capabilities |= CAP_GET;
		}
		else {
			warning(
				"external process '%s' requested unsupported read-object capability '%s'",
				subprocess->cmd, cap_name
			);
		}

		string_list_clear(&cap_list, 0);
	}

done:
	sigchain_pop(SIGPIPE);

	if (err || errno == EPIPE)
		return err ? err : errno;

	return 0;
}

static int read_object_process(const unsigned char *sha1)
{
	int err;
	struct read_object_process *entry;
	struct child_process *process;
	struct strbuf status = STRBUF_INIT;
	uint64_t start;

	start = getnanotime();

	if (!repository_format_promised_objects)
		die("BUG: if extensions.promisedObjects is not set, there "
		    "should not be any promised objects");

	if (!subprocess_map_initialized) {
		subprocess_map_initialized = 1;
		hashmap_init(&subprocess_map, (hashmap_cmp_fn)cmd2process_cmp, 0);
		entry = NULL;
	} else {
		entry = (struct read_object_process *)subprocess_find_entry(&subprocess_map, repository_format_promised_objects);
	}
	if (!entry) {
		entry = xmalloc(sizeof(*entry));
		entry->supported_capabilities = 0;

		if (subprocess_start(&subprocess_map, &entry->subprocess, repository_format_promised_objects, start_read_object_fn)) {
			free(entry);
			return -1;
		}
	}
	process = subprocess_get_child_process(&entry->subprocess);

	if (!(CAP_GET & entry->supported_capabilities))
		return -1;

	sigchain_push(SIGPIPE, SIG_IGN);

	err = packet_write_fmt_gently(process->in, "command=get\n");
	if (err)
		goto done;

	err = packet_write_fmt_gently(process->in, "sha1=%s\n", sha1_to_hex(sha1));
	if (err)
		goto done;

	err = packet_flush_gently(process->in);
	if (err)
		goto done;

	err = subprocess_read_status(process->out, &status);
	err = err ? err : strcmp(status.buf, "success");

done:
	sigchain_pop(SIGPIPE);

	if (err || errno == EPIPE) {
		err = err ? err : errno;
		if (!strcmp(status.buf, "error")) {
			/* The process signaled a problem with the file. */
		}
		else if (!strcmp(status.buf, "abort")) {
			/*
			 * The process signaled a permanent problem. Don't try to read
			 * objects with the same command for the lifetime of the current
			 * Git process.
			 */
			entry->supported_capabilities &= ~CAP_GET;
		}
		else {
			/*
			 * Something went wrong with the read-object process.
			 * Force shutdown and restart if needed.
			 */
			error("external process '%s' failed", repository_format_promised_objects);
			subprocess_stop(&subprocess_map, (struct subprocess_entry *)entry);
			free(entry);
		}
	}

	trace_performance_since(start, "read_object_process");

	return err;
}

int request_promised_objects(const struct oid_array *oids)
{
	int oids_requested = 0;
	int i;

	for (i = 0; i < oids->nr; i++) {
		if (is_promised_object(&oids->oid[i], NULL, NULL))
			break;
	}

	if (i == oids->nr)
		/* Nothing to fetch */
		return 0;

	for (; i < oids->nr; i++) {
		if (is_promised_object(&oids->oid[i], NULL, NULL)) {
			read_object_process(oids->oid[i].hash);
			oids_requested++;
		}
	}

	/*
	 * The command above may have updated packfiles, so update our record
	 * of them.
	 */
	reprepare_packed_git();
	return oids_requested;
}

#include "cache.h"
#include "promised-object.h"
#include "sha1-lookup.h"
#include "strbuf.h"

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

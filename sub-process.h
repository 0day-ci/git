#ifndef SUBPROCESS_H
#define SUBPROCESS_H

#include "git-compat-util.h"
#include "hashmap.h"
#include "run-command.h"

/*
 * Generic implementation of background process infrastructure.
 * See: Documentation/technical/api-sub-process.txt
 */

 /* data structures */

struct subprocess_entry {
	struct hashmap_entry ent; /* must be the first member! */
	const char *cmd;
	struct child_process process;
};

struct subprocess_capability {
	const char *name;
	unsigned int flag;
};

/* subprocess functions */

extern int cmd2process_cmp(const void *unused_cmp_data,
			   const struct subprocess_entry *e1,
			   const struct subprocess_entry *e2,
			   const void *unused_keydata);

typedef int(*subprocess_start_fn)(struct subprocess_entry *entry);
int subprocess_start(struct hashmap *hashmap, struct subprocess_entry *entry, const char *cmd,
		subprocess_start_fn startfn);

void subprocess_stop(struct hashmap *hashmap, struct subprocess_entry *entry);

struct subprocess_entry *subprocess_find_entry(struct hashmap *hashmap, const char *cmd);

/* subprocess helper functions */

static inline struct child_process *subprocess_get_child_process(
		struct subprocess_entry *entry)
{
	return &entry->process;
}

/*
 * Perform the handshake to a long-running process as described in the
 * gitattributes documentation using the given requested versions and
 * capabilities. The "versions" and "capabilities" parameters are arrays
 * terminated by a 0 or blank struct.
 */
int subprocess_handshake(struct subprocess_entry *entry,
			 const char *welcome_prefix,
			 int *versions,
			 int *chosen_version,
			 struct subprocess_capability *capabilities,
			 unsigned int *supported_capabilities);

/*
 * Helper function that will read packets looking for "status=<foo>"
 * key/value pairs and return the value from the last "status" packet
 */

int subprocess_read_status(int fd, struct strbuf *status);

#endif

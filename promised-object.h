#ifndef PROMISED_OBJECT_H
#define PROMISED_OBJECT_H

#include "cache.h"
#include "sha1-array.h"

/*
 * Returns 1 if oid is the name of a promised object. For non-blobs, 0 is
 * reported as their size.
 */
int is_promised_object(const struct object_id *oid, enum object_type *type,
		       unsigned long *size);

typedef int each_promised_object_fn(const struct object_id *oid, void *data);
int for_each_promised_object(each_promised_object_fn, void *);

/*
 * Returns 0 if there is no list of promised objects or if the list of promised
 * objects is valid.
 */
int fsck_promised_objects(void);

/*
 * If any of the given objects are promised objects, invokes
 * core.promisedobjectcommand with those objects and returns the number of
 * objects requested. No check is made as to whether the invocation actually
 * populated the repository with the promised objects.
 *
 * If none of the given objects are promised objects, this function does not
 * invoke anything and returns 0.
 */
int request_promised_objects(const struct oid_array *oids);

#endif

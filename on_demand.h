#ifndef ON_DEMAND_H
#define ON_DEMAND_H

void *read_remote_on_demand(const unsigned char *sha1, enum object_type *type,
			    unsigned long *size);
int object_info_on_demand(const unsigned char *sha1, struct object_info *oi);

#endif

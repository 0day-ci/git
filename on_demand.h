#ifndef ON_DEMAND_H
#define ON_DEMAND_H

void *read_remote_on_demand(const unsigned char *sha1, enum object_type *type,
			    unsigned long *size);
int object_info_on_demand(const unsigned char *sha1, struct object_info *oi);
void register_on_demand_cutoff(const unsigned char *sha1);
int on_demand_include_check(struct commit *commit, void *data);
void on_demand_show_commit_tree(struct commit *commit, void *data);
int on_demand_show_tree_check(struct commit *commit, void *data);

#endif

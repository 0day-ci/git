#ifndef LIST_OBJECTS_H
#define LIST_OBJECTS_H

typedef void (*show_commit_fn)(struct commit *, void *);
typedef void (*show_object_fn)(struct object *, const char *, void *);
typedef int (*show_tree_check_fn)(struct commit *, void *);
void traverse_commit_list_extended(struct rev_info *, show_commit_fn,
				   show_object_fn, show_tree_check_fn, void *);

inline void traverse_commit_list(struct rev_info *revs,
				 show_commit_fn show_commit,
				 show_object_fn show_object,
				 void *data)
{
	traverse_commit_list_extended(revs, show_commit, show_object, NULL,
				      data);
}

typedef void (*show_edge_fn)(struct commit *);
void mark_edges_uninteresting(struct rev_info *, show_edge_fn);

#endif

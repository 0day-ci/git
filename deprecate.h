#ifndef DEPRECATE_H
#define DEPRECATE_H

extern void deprecate(int *state, const char *message,
		      int dep_at, int warn_at, int die_at, int remove_at);

#endif

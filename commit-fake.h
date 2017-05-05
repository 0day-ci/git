#ifndef COMMIT_FAKE_H
#define COMMIT_FAKE_H

#include "commit.h"
#include "diff.h"

struct commit *fake_working_tree_commit(struct diff_options *opt, const char *path, const char *contents_from);

#endif /* COMMIT_FAKE_H */

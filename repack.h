#ifndef REPACK_H
#define REPACK_H

#include "git-compat-util.h"

const char *lock_repo_for_pack_manipulation(int force, pid_t* ret_pid);

#endif /* REPACK_H */

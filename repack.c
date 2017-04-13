#include "builtin.h"
#include "repack.h"
#include "strbuf.h"
#include "lockfile.h"
#include "tempfile.h"

static struct tempfile pidfile;

/*
 * Commands should call this before doing any operation which might
 * delete a pack file (e.g. gc or repack).  We don't want to allow
 * multiple operations of this type to operate at the same time.
 *
 * For historical reasons, the pid file created is called "gc.pid",
 * even though it is also used for git-repack.
 *
 * The lock will persist until the process ends.
 *
 * If force is non-zero, any existing lock will be disregarded.
 *
 * return NULL on success, else hostname running the pack manipulation
 * operation.
 *
 * It is safe to call this function multiple times in the same process;
 * calls after the first successful call will always return NULL.
 */
const char *lock_repo_for_pack_manipulation(int force, pid_t* ret_pid)
{
	static struct lock_file lock;
	char my_host[128];
	struct strbuf sb = STRBUF_INIT;
	struct stat st;
	uintmax_t pid;
	FILE *fp;
	int fd;
	char *pidfile_path;

	if (is_tempfile_active(&pidfile))
		/* already locked */
		return NULL;

	if (gethostname(my_host, sizeof(my_host)))
		xsnprintf(my_host, sizeof(my_host), "unknown");

	pidfile_path = git_pathdup("gc.pid");
	fd = hold_lock_file_for_update(&lock, pidfile_path,
				       LOCK_DIE_ON_ERROR);
	if (!force) {
		static char locking_host[128];
		int should_exit;
		fp = fopen(pidfile_path, "r");
		memset(locking_host, 0, sizeof(locking_host));
		should_exit =
			fp != NULL &&
			!fstat(fileno(fp), &st) &&
			/*
			 * 12 hour limit is very generous as gc should
			 * never take that long. On the other hand we
			 * don't really need a strict limit here,
			 * running gc --auto one day late is not a big
			 * problem. --force can be used in manual gc
			 * after the user verifies that no gc is
			 * running.
			 */
			time(NULL) - st.st_mtime <= 12 * 3600 &&
			fscanf(fp, "%"SCNuMAX" %127c", &pid, locking_host) == 2 &&
			/* be gentle to concurrent "gc" on remote hosts */
			(strcmp(locking_host, my_host) || !kill(pid, 0) || errno == EPERM);
		if (fp != NULL)
			fclose(fp);
		if (should_exit) {
			if (fd >= 0)
				rollback_lock_file(&lock);
			*ret_pid = pid;
			free(pidfile_path);
			return locking_host;
		}
	}

	strbuf_addf(&sb, "%"PRIuMAX" %s",
		    (uintmax_t) getpid(), my_host);
	write_in_full(fd, sb.buf, sb.len);
	strbuf_release(&sb);
	commit_lock_file(&lock);
	register_tempfile(&pidfile, pidfile_path);
	free(pidfile_path);
	return NULL;
}

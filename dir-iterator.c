#include "cache.h"
#include "dir.h"
#include "iterator.h"
#include "dir-iterator.h"

struct dir_iterator_level {
	DIR *dir;

	/*
	 * The length of the directory part of path at this level
	 * (including a trailing '/'):
	 */
	size_t prefix_len;

	/*
	 * The last action that has been taken with the current entry
	 * (needed for directories, which have to be included in the
	 * iteration and also iterated into):
	 */
	enum {
		DIR_STATE_PUSHED,
		DIR_STATE_PRE_ITERATION,
		DIR_STATE_ITERATING,
		DIR_STATE_POST_ITERATION,
		DIR_STATE_EXHAUSTED
	} dir_state;
};

/*
 * The full data structure used to manage the internal directory
 * iteration state. It includes members that are not part of the
 * public interface.
 */
struct dir_iterator_int {
	struct dir_iterator base;

	/*
	 * The number of levels currently on the stack. This is always
	 * at least 1, because when it becomes zero the iteration is
	 * ended and this struct is freed.
	 */
	size_t levels_nr;

	/* The number of levels that have been allocated on the stack */
	size_t levels_alloc;

	/*
	 * A stack of levels. levels[0] is the uppermost directory
	 * that will be included in this iteration.
	 */
	struct dir_iterator_level *levels;

	/* Holds the flags passed to dir_iterator_begin(). */
	unsigned flags;
};

static inline void push_dir_level(struct dir_iterator_int *iter, struct dir_iterator_level *level)
{
	ALLOC_GROW(iter->levels, iter->levels_nr + 1,
		   iter->levels_alloc);

	/* Push a new level */
	level = &iter->levels[iter->levels_nr++];
	level->dir = NULL;
	level->dir_state = DIR_STATE_PUSHED;
}

static inline int pop_dir_level(struct dir_iterator_int *iter)
{
	return --iter->levels_nr;
}

static inline int set_iterator_data(struct dir_iterator_int *iter, struct dir_iterator_level *level)
{
	if (lstat(iter->base.path.buf, &iter->base.st) < 0) {
		if (errno != ENOENT)
			warning("error reading path '%s': %s",
				iter->base.path.buf,
				strerror(errno));
		return -1;
	}

	/*
	 * Check if we are dealing with the root directory as an
	 * item that's being iterated through.
	 */
	if (level->dir_state != DIR_STATE_ITERATING &&
		iter->levels_nr == 1) {
		iter->base.relative_path = ".";
	}
	else {
		iter->base.relative_path =
			iter->base.path.buf + iter->levels[0].prefix_len;
	}
	iter->base.basename =
		iter->base.path.buf + level->prefix_len;

	return 0;
}

/*
 * This function uses a state machine with the following states:
 * -> DIR_STATE_PUSHED: the directory has been pushed to the
 * iterator traversal tree.
 * -> DIR_STATE_PRE_ITERATION: the directory is *NOT* initialized. The
 * dirpath has already been returned if pre-order traversal is set.
 * -> DIR_STATE_ITERATING: the directory is initialized. We are traversing
 * through it.
 * -> DIR_STATE_POST_ITERATION: the directory has been iterated through.
 * We are ready to close it.
 * -> DIR_STATE_EXHAUSTED: the directory is closed and ready to be popped.
 */
int dir_iterator_advance(struct dir_iterator *dir_iterator)
{
	struct dir_iterator_int *iter =
		(struct dir_iterator_int *)dir_iterator;

	while (1) {
		struct dir_iterator_level *level =
			&iter->levels[iter->levels_nr - 1];
		struct dirent *de;

		if (level->dir_state == DIR_STATE_PUSHED) {
			level->dir_state = DIR_STATE_PRE_ITERATION;

			if (iter->flags & DIR_ITERATOR_PRE_ORDER_TRAVERSAL) {
				/* We may not want the root directory to be iterated over */
				if (iter->levels_nr != 1 ||
					(iter->flags & DIR_ITERATOR_LIST_ROOT_DIR)) {
					set_iterator_data(iter, level);
					return ITER_OK;
				}
			}
		} else if (level->dir_state == DIR_STATE_PRE_ITERATION) {
			/*
			 * Note: dir_iterator_begin() ensures that
			 * path is not the empty string.
			 */
			if (!is_dir_sep(iter->base.path.buf[iter->base.path.len - 1]))
				strbuf_addch(&iter->base.path, '/');
			level->prefix_len = iter->base.path.len;

			level->dir = opendir(iter->base.path.buf);
			if (!level->dir) {
				/*
				 * This level wasn't opened sucessfully; pretend we
				 * iterated through it already.
				 */
				if (errno != ENOENT) {
					warning("error opening directory %s: %s",
						iter->base.path.buf, strerror(errno));
				}

				level->dir_state = DIR_STATE_POST_ITERATION;
				continue;
			}

			level->dir_state = DIR_STATE_ITERATING;
		} else if (level->dir_state == DIR_STATE_ITERATING) {
			strbuf_setlen(&iter->base.path, level->prefix_len);
			errno = 0;
			de = readdir(level->dir);

			if (!de) {
				/* In case of readdir() error */
				if (errno) {
					warning("error reading directory %s: %s",
						iter->base.path.buf, strerror(errno));
				}

				level->dir_state = DIR_STATE_POST_ITERATION;
				continue;
			}

			if (is_dot_or_dotdot(de->d_name))
				continue;

			strbuf_addstr(&iter->base.path, de->d_name);

			if (set_iterator_data(iter, level))
				continue;

			if (S_ISDIR(iter->base.st.st_mode)) {
				push_dir_level(iter, level);
				continue;
			}

			return ITER_OK;
		} else if (level->dir_state == DIR_STATE_POST_ITERATION) {
			if (level->dir != NULL && closedir(level->dir)) {
				warning("error closing directory %s: %s",
					iter->base.path.buf, strerror(errno));
			}
			level->dir_state = DIR_STATE_EXHAUSTED;

			strbuf_setlen(&iter->base.path, level->prefix_len);
			/*
			 * Since we are iterating through the dirpath
			 * after we have gone through it, we still need
			 * to get rid of the trailing slash we appended.
			 */
			strbuf_strip_suffix(&iter->base.path, "/");

			if (iter->flags & DIR_ITERATOR_POST_ORDER_TRAVERSAL) {
				/* We may not want the root directory to be iterated over */
				if (iter->levels_nr != 1 ||
					(iter->flags & DIR_ITERATOR_LIST_ROOT_DIR)) {
					set_iterator_data(iter, level);
					return ITER_OK;
				}
			}
		} else if (level->dir_state == DIR_STATE_EXHAUSTED) {
			if (pop_dir_level(iter) == 0)
				return dir_iterator_abort(dir_iterator);
		}
	}
}

int dir_iterator_abort(struct dir_iterator *dir_iterator)
{
	struct dir_iterator_int *iter = (struct dir_iterator_int *)dir_iterator;

	for (; iter->levels_nr; iter->levels_nr--) {
		struct dir_iterator_level *level =
			&iter->levels[iter->levels_nr - 1];

		if (level->dir && closedir(level->dir)) {
			strbuf_setlen(&iter->base.path, level->prefix_len);
			warning("error closing directory %s: %s",
				iter->base.path.buf, strerror(errno));
		}
	}

	free(iter->levels);
	strbuf_release(&iter->base.path);
	free(iter);
	return ITER_DONE;
}

struct dir_iterator *dir_iterator_begin(const char *path, unsigned flags)
{
	struct dir_iterator_int *iter = xcalloc(1, sizeof(*iter));
	struct dir_iterator *dir_iterator = &iter->base;

	if (!path || !*path)
		die("BUG: empty path passed to dir_iterator_begin()");

	iter->flags = flags;

	strbuf_init(&iter->base.path, PATH_MAX);
	strbuf_addstr(&iter->base.path, path);

	ALLOC_GROW(iter->levels, 10, iter->levels_alloc);

	iter->levels_nr = 1;
	iter->levels[0].dir = NULL;
	iter->levels[0].dir_state = DIR_STATE_PUSHED;

	return dir_iterator;
}

#include "cache.h"

/*
 * Do not use this for inspecting *tracked* content.  When path is a
 * symlink to a directory, we do not want to say it is a directory when
 * dealing with tracked content in the working tree.
 */
int is_directory(const char *path)
{
	struct stat st;
	return (!stat(path, &st) && S_ISDIR(st.st_mode));
}

/* removes the last path component from 'path' except if 'path' is root */
static void strip_last_component(struct strbuf *path)
{
	if (path->len > 1) {
		char *last_slash = find_last_dir_sep(path->buf);
		strbuf_setlen(path, last_slash - path->buf);
	}
}

/* gets the next component in 'remaining' and places it in 'next' */
static void get_next_component(struct strbuf *next, struct strbuf *remaining)
{
	char *start = NULL;
	char *end = NULL;

	strbuf_reset(next);

	/* look for the next component */
	/* Skip sequences of multiple path-separators */
	for (start = remaining->buf; is_dir_sep(*start); start++)
		/* nothing */;
	/* Find end of the path component */
	for (end = start; *end && !is_dir_sep(*end); end++)
		/* nothing */;

	strbuf_add(next, start, end - start);
	/* remove the component from 'remaining' */
	strbuf_remove(remaining, 0, end - remaining->buf);
}

/* We allow "recursive" symbolic links. Only within reason, though. */
#define MAXSYMLINKS 5

/*
 * Return the real path (i.e., absolute path, with symlinks resolved
 * and extra slashes removed) equivalent to the specified path.  (If
 * you want an absolute path but don't mind links, use
 * absolute_path().)  The return value is a pointer to a static
 * buffer.
 *
 * The directory part of path (i.e., everything up to the last
 * dir_sep) must denote a valid, existing directory, but the last
 * component need not exist.  If die_on_error is set, then die with an
 * informative error message if there is a problem.  Otherwise, return
 * NULL on errors (without generating any output).
 *
 * If path is our buffer, then return path, as it's already what the
 * user wants.
 */
static const char *real_path_internal(const char *path, int die_on_error)
{
	static struct strbuf resolved = STRBUF_INIT;
	struct strbuf remaining = STRBUF_INIT;
	struct strbuf next = STRBUF_INIT;
	struct strbuf symlink = STRBUF_INIT;
	char *retval = NULL;
	int num_symlinks = 0;
	struct stat st;

	/* We've already done it */
	if (path == resolved.buf)
		return path;

	if (!*path) {
		if (die_on_error)
			die("The empty string is not a valid path");
		else
			goto error_out;
	}

	strbuf_reset(&resolved);

	if (is_absolute_path(path)) {
		/* absolute path; start with only root as being resolved */
		strbuf_addch(&resolved, '/');
		strbuf_addstr(&remaining, path + 1);
	} else {
		/* relative path; can use CWD as the initial resolved path */
		if (strbuf_getcwd(&resolved)) {
			if (die_on_error)
				die_errno("Could not get current working directory");
			else
				goto error_out;
		}
		strbuf_addstr(&remaining, path);
	}

	/* Iterate over the remaining path components */
	while (remaining.len > 0) {
		get_next_component(&next, &remaining);

		if (next.len == 0) {
			continue; /* empty component */
		} else if (next.len == 1 && !strcmp(next.buf, ".")) {
			continue; /* '.' component */
		} else if (next.len == 2 && !strcmp(next.buf, "..")) {
			/* '..' component; strip the last path component */
			strip_last_component(&resolved);
			continue;
		}

		/* append the next component and resolve resultant path */
		if (resolved.buf[resolved.len - 1] != '/')
			strbuf_addch(&resolved, '/');
		strbuf_addbuf(&resolved, &next);

		if (lstat(resolved.buf, &st)) {
			/* error out unless this was the last component */
			if (!(errno == ENOENT && !remaining.len)) {
				if (die_on_error)
					die_errno("Invalid path '%s'",
						  resolved.buf);
				else
					goto error_out;
			}
		} else if (S_ISLNK(st.st_mode)) {
			ssize_t len;
			strbuf_reset(&symlink);

			if (num_symlinks++ > MAXSYMLINKS) {
				if (die_on_error)
					die("More than %d nested symlinks "
					    "on path '%s'", MAXSYMLINKS, path);
				else
					goto error_out;
			}

			len = strbuf_readlink(&symlink, resolved.buf,
					      st.st_size);
			if (len < 0) {
				if (die_on_error)
					die_errno("Invalid symlink '%s'",
						  resolved.buf);
				else
					goto error_out;
			}

			if (is_absolute_path(symlink.buf)) {
				/*
				 * absolute symlink
				 * reset resolved path to root
				 */
				strbuf_setlen(&resolved, 1);
			} else {
				/*
				 * relative symlink
				 * strip off the last component since it will
				 * be replaced with the contents of the symlink
				 */
				strip_last_component(&resolved);
			}

			/*
			 * if there are still remaining components to resolve
			 * then append them to symlink
			 */
			if (remaining.len) {
				strbuf_addch(&symlink, '/');
				strbuf_addbuf(&symlink, &remaining);
			}

			/*
			 * use the symlink as the remaining components that
			 * need to be resloved
			 */
			strbuf_swap(&symlink, &remaining);
		}
	}

	retval = resolved.buf;

error_out:
	//strbuf_release(&resolved);
	strbuf_release(&remaining);
	strbuf_release(&next);
	strbuf_release(&symlink);

	return retval;
}

const char *real_path(const char *path)
{
	return real_path_internal(path, 1);
}

const char *real_path_if_valid(const char *path)
{
	return real_path_internal(path, 0);
}

/*
 * Use this to get an absolute path from a relative one. If you want
 * to resolve links, you should use real_path.
 */
const char *absolute_path(const char *path)
{
	static struct strbuf sb = STRBUF_INIT;
	strbuf_reset(&sb);
	strbuf_add_absolute_path(&sb, path);
	return sb.buf;
}

/*
 * Unlike prefix_path, this should be used if the named file does
 * not have to interact with index entry; i.e. name of a random file
 * on the filesystem.
 */
const char *prefix_filename(const char *pfx, int pfx_len, const char *arg)
{
	static struct strbuf path = STRBUF_INIT;
#ifndef GIT_WINDOWS_NATIVE
	if (!pfx_len || is_absolute_path(arg))
		return arg;
	strbuf_reset(&path);
	strbuf_add(&path, pfx, pfx_len);
	strbuf_addstr(&path, arg);
#else
	/* don't add prefix to absolute paths, but still replace '\' by '/' */
	strbuf_reset(&path);
	if (is_absolute_path(arg))
		pfx_len = 0;
	else if (pfx_len)
		strbuf_add(&path, pfx, pfx_len);
	strbuf_addstr(&path, arg);
	convert_slashes(path.buf + pfx_len);
#endif
	return path.buf;
}

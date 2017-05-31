#include "cache.h"
#include "repo.h"

/*
 * This may be the wrong place for this.
 * It may be better to go in env.c or setup for the time being?
 */
struct repo the_repository;

static char *git_path_from_env(const char *envvar, const char *git_dir,
			       const char *path, int fromenv)
{
	if (fromenv) {
		const char *value = getenv(envvar);
		if (value)
			return xstrdup(value);
	}

	return xstrfmt("%s/%s", git_dir, path);
}

static int find_common_dir(struct strbuf *sb, const char *gitdir, int fromenv)
{
	if (fromenv) {
		const char *value = getenv(GIT_COMMON_DIR_ENVIRONMENT);
		if (value) {
			strbuf_addstr(sb, value);
			return 1;
		}
	}

	return get_common_dir_noenv(sb, gitdir);
}

/* called after setting gitdir */
static void repo_setup_env(struct repo *repo)
{
	struct strbuf sb = STRBUF_INIT;

	if (!repo->gitdir)
		BUG("gitdir wasn't set before setting up the environment");

	repo->different_commondir = find_common_dir(&sb, repo->gitdir,
						    !repo->ignore_env);
	repo->commondir = strbuf_detach(&sb, NULL);
	repo->objectdir = git_path_from_env(DB_ENVIRONMENT, repo->commondir,
					    "objects", !repo->ignore_env);
	repo->index_file = git_path_from_env(INDEX_ENVIRONMENT, repo->gitdir,
					     "index", !repo->ignore_env);
	repo->graft_file = git_path_from_env(GRAFT_ENVIRONMENT, repo->commondir,
					     "info/grafts", !repo->ignore_env);
	repo->namespace = expand_namespace(repo->ignore_env ? NULL :
					   getenv(GIT_NAMESPACE_ENVIRONMENT));
}

static void repo_clear_env(struct repo *repo)
{
	free(repo->gitdir);
	repo->gitdir = NULL;
	free(repo->commondir);
	repo->commondir = NULL;
	free(repo->objectdir);
	repo->objectdir = NULL;
	free(repo->index_file);
	repo->index_file = NULL;
	free(repo->graft_file);
	repo->graft_file = NULL;
	free(repo->namespace);
	repo->namespace = NULL;
}

void repo_set_gitdir(struct repo *repo, const char *path)
{
	const char *gitfile = read_gitfile(path);

	/*
	 * NEEDSWORK: Eventually we want to be able to free gitdir and the rest
	 * of the environment before reinitializing it again, but we have some
	 * crazy code paths where we try to set gitdir with the current gitdir
	 * and we don't want to free gitdir before copying the passed in value.
	 */
	repo->gitdir = xstrdup(gitfile ? gitfile : path);

	repo_setup_env(repo);
}

int repo_init(struct repo *repo, const char *gitdir)
{
	int error = 0;
	char *abspath = real_pathdup(gitdir, 1);
	char *suspect = xstrfmt("%s/.git", abspath);
	struct strbuf sb = STRBUF_INIT;
	const char *resolved_gitdir;

	memset(repo, 0, sizeof(struct repo));

	repo->ignore_env = 1;

	/* First assume 'gitdir' references the gitdir directly */
	resolved_gitdir = resolve_gitdir_gently(abspath, &error);
	/* otherwise; try 'gitdir'.git */
	if (!resolved_gitdir) {
		resolved_gitdir = resolve_gitdir_gently(suspect, &error);
		if (!resolved_gitdir) {
			die("'%s' is not a repository\n", abspath);
			return error;
		}
	}

	repo_set_gitdir(repo, resolved_gitdir);

	/* NEEDSWORK: Verify repository format version */

	free(abspath);
	free(suspect);
	strbuf_release(&sb);

	return error;
}

void repo_clear(struct repo *repo)
{
	repo_clear_env(repo);
}

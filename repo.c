#include "cache.h"
#include "repo.h"
#include "config.h"
#include "submodule-config.h"

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

void repo_set_worktree(struct repo *repo, const char *path)
{
	repo->worktree = real_pathdup(path, 1);
}

void repo_read_config(struct repo *repo)
{
	struct config_options opts = { 1, repo->commondir };

	if (!repo->config)
		repo->config = xcalloc(1, sizeof(struct config_set));
	else
		git_configset_clear(repo->config);

	git_configset_init(repo->config);

	git_config_with_options(config_set_callback, repo->config, NULL, &opts);
}

void repo_read_index(struct repo *repo)
{
	if (!repo->index)
		repo->index = xcalloc(1, sizeof(struct index_state));
	else
		discard_index(repo->index);

	if (read_index_from(repo->index, repo->index_file) < 0)
		die(_("failure reading index"));
}

static int gitmodules_cb(const char *var, const char *value, void *data)
{
	struct repo *repo = data;
	return submodule_config_option(repo, var, value);
}

void repo_read_gitmodules(struct repo *repo)
{
	char *gitmodules_path = xstrfmt("%s/.gitmodules", repo->worktree);

	git_config_from_file(gitmodules_cb, gitmodules_path, repo);
	free(gitmodules_path);
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
	free(repo->worktree);
	repo->worktree = NULL;

	if (repo->config) {
		git_configset_clear(repo->config);
		free(repo->config);
		repo->config = NULL;
	}

	if (repo->index) {
		discard_index(repo->index);
		free(repo->index);
		repo->index = NULL;
	}

	if (repo->submodule_cache) {
		submodule_cache_free(repo->submodule_cache);
		repo->submodule_cache = NULL;
	}
}
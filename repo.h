#ifndef REPO_H
#define REPO_H

struct config_set;
struct index_state;
struct submodule_cache;

struct repo {
	/* Environment */
	char *gitdir;
	char *commondir;
	char *objectdir;
	char *index_file;
	char *graft_file;
	char *namespace;
	char *worktree;

	/* Subsystems */
	/*
	 * Repository's config which contains key-value pairs from the usual
	 * set of config config files (i.e repo specific .git/config, user wide
	 * ~/.gitconfig, XDG config file and the global /etc/gitconfig)
	 */
	struct config_set *config;
	struct index_state *index;
	struct submodule_cache *submodule_cache;

	/* Configurations */
	unsigned ignore_env:1;
	/* Indicates if a repository has a different 'commondir' from 'gitdir' */
	unsigned different_commondir:1;
};

extern struct repo the_repository;

extern void repo_set_gitdir(struct repo *repo, const char *path);
extern void repo_set_worktree(struct repo *repo, const char *path);
extern void repo_read_config(struct repo *repo);
extern void repo_read_index(struct repo *repo);
extern void repo_read_gitmodules(struct repo *repo);
extern int repo_init(struct repo *repo, const char *path);
extern void repo_clear(struct repo *repo);

#endif /* REPO_H */

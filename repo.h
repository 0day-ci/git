#ifndef REPO_H
#define REPO_H

struct repo {
	/* Environment */
	char *gitdir;
	char *commondir;
	char *objectdir;
	char *index_file;
	char *graft_file;
	char *namespace;

	/* Configurations */
	unsigned ignore_env:1;
	/* Indicates if a repository has a different 'commondir' from 'gitdir' */
	unsigned different_commondir:1;
};

extern struct repo the_repository;

extern void repo_set_gitdir(struct repo *repo, const char *path);
extern int repo_init(struct repo *repo, const char *path);
extern void repo_clear(struct repo *repo);

#endif /* REPO_H */

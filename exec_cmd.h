#ifndef GIT_EXEC_CMD_H
#define GIT_EXEC_CMD_H

struct argv_array;

extern void git_set_argv_exec_path(const char *exec_path);
extern void git_extract_argv0_path(const char *path);
extern const char *git_exec_path(void);
extern void setup_path(void);
extern const char **prepare_git_cmd(struct argv_array *out, const char **argv);
extern int execv_git_cmd(const char **argv); /* NULL terminated */
LAST_ARG_MUST_BE_NULL
extern int execl_git_cmd(const char *cmd, ...);
extern char *system_path(const char *path);
extern int is_executable(const char *name);

#endif /* GIT_EXEC_CMD_H */

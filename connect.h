#ifndef CONNECT_H
#define CONNECT_H

enum protocol {
	PROTO_LOCAL = 1,
	PROTO_FILE,
	PROTO_SSH,
	PROTO_GIT
};
enum protocol parse_connect_url(const char *url_orig, char **ret_host,
				char **ret_path);

#define CONNECT_VERBOSE       (1u << 0)
#define CONNECT_DIAG_URL      (1u << 1)
#define CONNECT_IPV4          (1u << 2)
#define CONNECT_IPV6          (1u << 3)
extern struct child_process *git_connect(int fd[2], const char *url, const char *prog, int flags);
extern int finish_connect(struct child_process *conn);
extern int git_connection_is_socket(struct child_process *conn);
extern int server_supports(const char *feature);
extern int parse_feature_request(const char *features, const char *feature);
extern const char *server_feature_value(const char *feature, int *len_ret);
extern int url_is_local_not_ssh(const char *url);

#endif

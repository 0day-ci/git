/*
 * This program takes payloads in pkt-line format (one or more pkt-lines
 * followed by a flush pkt) and runs the given command once per payload.
 */
#include "cache.h"
#include "pkt-line.h"
#include "run-command.h"

int cmd_main(int argc, const char **argv)
{
	struct child_process *cmd = NULL;
	char buffer[LARGE_PACKET_MAX];
	int size;

	while ((size = packet_read(0, NULL, NULL, buffer, sizeof(buffer),
				   PACKET_READ_GENTLE_ON_EOF)) >= 0) {
		if (size > 0) {
			if (!cmd) {
				cmd = xmalloc(sizeof(*cmd));
				child_process_init(cmd);
				cmd->argv = argv + 1;
				cmd->git_cmd = 1;
				cmd->in = -1;
				cmd->out = 0;
				if (start_command(cmd))
					die("could not start command");
			}
			write_in_full(cmd->in, buffer, size);
		} else if (cmd) {
			close(cmd->in);
			if (finish_command(cmd))
				die("could not finish command");
			free(cmd);
			cmd = NULL;
			if (size < 0)
				break;
		}
	}
	return 0;
}

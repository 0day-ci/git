#ifndef SCOREBOARD_H
#define SCOREBOARD_H

#include "commit.h"
#include "revision.h"
#include "prio-queue.h"
#include "origin.h"

#define PICKAXE_BLAME_MOVE		01
#define PICKAXE_BLAME_COPY		02
#define PICKAXE_BLAME_COPY_HARDER	04
#define PICKAXE_BLAME_COPY_HARDEST	010

#define BLAME_DEFAULT_MOVE_SCORE	20
#define BLAME_DEFAULT_COPY_SCORE	40

/*
 * The current state of the blame assignment.
 */
struct scoreboard {
	/* the final commit (i.e. where we started digging from) */
	struct commit *final;
	/* Priority queue for commits with unassigned blame records */
	struct prio_queue commits;
	struct rev_info *revs;
	const char *path;
	const char *contents_from;

	/*
	 * The contents in the final image.
	 * Used by many functions to obtain contents of the nth line,
	 * indexed with scoreboard.lineno[blame_entry.lno].
	 */
	const char *final_buf;
	unsigned long final_buf_size;

	/* linked list of blames */
	struct blame_entry *ent;

	/* look-up a line in the final buffer */
	int num_lines;
	int *lineno;

	/*
	 * blame for a blame_entry with score lower than these thresholds
	 * is not passed to the parent using move/copy logic.
	 */
	unsigned blame_move_score;
	unsigned blame_copy_score;

	/* stats */
	int num_read_blob;
	int num_get_patch;
	int num_commits;

	/* options */
	int show_root;
	int reverse;
	int xdl_opts;
	int no_whole_file_rename;

	/* callbacks */
	void(*sanity_check)(struct scoreboard *);
	void(*found_guilty_entry)(struct blame_entry *, void *);

	void *found_guilty_entry_data;
};

void coalesce(struct scoreboard *sb);
void blame_sort_final(struct scoreboard *sb);
unsigned ent_score(struct scoreboard *sb, struct blame_entry *e);
void assign_blame(struct scoreboard *sb, int opt);
const char *nth_line(struct scoreboard *sb, long lno);

#endif /* SCOREBOARD_H */

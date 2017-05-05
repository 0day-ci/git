#ifndef ORIGIN_H
#define ORIGIN_H

#include "cache.h"
#include "commit.h"
#include "xdiff-interface.h"

/*
 * One blob in a commit that is being suspected
 */
struct origin {
	int refcnt;
	/* Record preceding blame record for this blob */
	struct origin *previous;
	/* origins are put in a list linked via `next' hanging off the
	 * corresponding commit's util field in order to make finding
	 * them fast.  The presence in this chain does not count
	 * towards the origin's reference count.  It is tempting to
	 * let it count as long as the commit is pending examination,
	 * but even under circumstances where the commit will be
	 * present multiple times in the priority queue of unexamined
	 * commits, processing the first instance will not leave any
	 * work requiring the origin data for the second instance.  An
	 * interspersed commit changing that would have to be
	 * preexisting with a different ancestry and with the same
	 * commit date in order to wedge itself between two instances
	 * of the same commit in the priority queue _and_ produce
	 * blame entries relevant for it.  While we don't want to let
	 * us get tripped up by this case, it certainly does not seem
	 * worth optimizing for.
	 */
	struct origin *next;
	struct commit *commit;
	/* `suspects' contains blame entries that may be attributed to
	 * this origin's commit or to parent commits.  When a commit
	 * is being processed, all suspects will be moved, either by
	 * assigning them to an origin in a different commit, or by
	 * shipping them to the scoreboard's ent list because they
	 * cannot be attributed to a different commit.
	 */
	struct blame_entry *suspects;
	mmfile_t file;
	struct object_id blob_oid;
	unsigned mode;
	/* guilty gets set when shipping any suspects to the final
	 * blame list instead of other commits
	 */
	char guilty;
	char path[FLEX_ARRAY];
};

/*
 * Each group of lines is described by a blame_entry; it can be split
 * as we pass blame to the parents.  They are arranged in linked lists
 * kept as `suspects' of some unprocessed origin, or entered (when the
 * blame origin has been finalized) into the scoreboard structure.
 * While the scoreboard structure is only sorted at the end of
 * processing (according to final image line number), the lists
 * attached to an origin are sorted by the target line number.
 */
struct blame_entry {
	struct blame_entry *next;

	/* the first line of this group in the final image;
	 * internally all line numbers are 0 based.
	 */
	int lno;

	/* how many lines this group has */
	int num_lines;

	/* the commit that introduced this group into the final image */
	struct origin *suspect;

	/* the line number of the first line of this group in the
	 * suspect's file; internally all line numbers are 0 based.
	 */
	int s_lno;

	/* how significant this entry is -- cached to avoid
	 * scanning the lines over and over.
	 */
	unsigned score;
};

/*
 * Origin is refcounted and usually we keep the blob contents to be
 * reused.
 */
static inline struct origin *origin_incref(struct origin *o)
{
	if (o)
		o->refcnt++;
	return o;
}
void origin_decref(struct origin *o);

struct origin *make_origin(struct commit *commit, const char *path);
struct origin *get_origin(struct commit *commit, const char *path);

#endif /* ORIGIN_H */

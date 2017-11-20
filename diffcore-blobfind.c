/*
 * Copyright (c) 2017 Google Inc.
 */
#include "cache.h"
#include "diff.h"
#include "diffcore.h"

static void diffcore_filter_blobs(struct diff_queue_struct *q,
				  struct diff_options *options)
{
	int i, j = 0, c = q->nr;

	if (!options->blobfind)
		BUG("blobfind oidset not initialized???");

	for (i = 0; i < q->nr; i++) {
		struct diff_filepair *p = q->queue[i];

		if (DIFF_PAIR_UNMERGED(p) ||
		    (DIFF_FILE_VALID(p->one) &&
		     oidset_contains(options->blobfind, &p->one->oid)) ||
		    (DIFF_FILE_VALID(p->two) &&
		     oidset_contains(options->blobfind, &p->two->oid)))
			continue;

		diff_free_filepair(p);
		q->queue[i] = NULL;
		c--;
	}

	/* Keep it sorted. */
	i = 0; j = 0;
	while (i < c) {
		while (!q->queue[j])
			j++;
		q->queue[i] = q->queue[j];
		i++; j++;
	}

	q->nr = c;

	if (!c) {
		free(q->queue);
		DIFF_QUEUE_CLEAR(q);
	}
}

void diffcore_blobfind(struct diff_options *options)
{
	diffcore_filter_blobs(&diff_queued_diff, options);
}

/*
 * Builtin "git branch"
 *
 * Copyright (c) 2006 Kristian Høgsberg <krh@redhat.com>
 * Based on git-branch.sh by Junio C Hamano.
 */

#include "cache.h"
#include "color.h"
#include "refs.h"
#include "commit.h"
#include "builtin.h"
#include "remote.h"
#include "parse-options.h"
#include "branch.h"
#include "diff.h"
#include "revision.h"
#include "string-list.h"
#include "column.h"
#include "utf8.h"
#include "wt-status.h"
#include "ref-filter.h"
#include "worktree.h"

static const char * const builtin_branch_usage[] = {
	N_("git branch [<options>] [-r | -a] [--merged | --no-merged]"),
	N_("git branch [<options>] [-l] [-f] <branch-name> [<start-point>]"),
	N_("git branch [<options>] [-r] (-d | -D) <branch-name>..."),
	N_("git branch [<options>] (-m | -M) [<old-branch>] <new-branch>"),
	N_("git branch [<options>] [-r | -a] [--points-at]"),
	NULL
};

static const char *head;
static unsigned char head_sha1[20];

static int branch_use_color = -1;
static char branch_colors[][COLOR_MAXLEN] = {
	GIT_COLOR_RESET,
	GIT_COLOR_NORMAL,	/* PLAIN */
	GIT_COLOR_RED,		/* REMOTE */
	GIT_COLOR_NORMAL,	/* LOCAL */
	GIT_COLOR_GREEN,	/* CURRENT */
	GIT_COLOR_BLUE,		/* UPSTREAM */
};
enum color_branch {
	BRANCH_COLOR_RESET = 0,
	BRANCH_COLOR_PLAIN = 1,
	BRANCH_COLOR_REMOTE = 2,
	BRANCH_COLOR_LOCAL = 3,
	BRANCH_COLOR_CURRENT = 4,
	BRANCH_COLOR_UPSTREAM = 5
};

static struct string_list output = STRING_LIST_INIT_DUP;
static unsigned int colopts;

static int parse_branch_color_slot(const char *slot)
{
	if (!strcasecmp(slot, "plain"))
		return BRANCH_COLOR_PLAIN;
	if (!strcasecmp(slot, "reset"))
		return BRANCH_COLOR_RESET;
	if (!strcasecmp(slot, "remote"))
		return BRANCH_COLOR_REMOTE;
	if (!strcasecmp(slot, "local"))
		return BRANCH_COLOR_LOCAL;
	if (!strcasecmp(slot, "current"))
		return BRANCH_COLOR_CURRENT;
	if (!strcasecmp(slot, "upstream"))
		return BRANCH_COLOR_UPSTREAM;
	return -1;
}

static int git_branch_config(const char *var, const char *value, void *cb)
{
	const char *slot_name;

	if (starts_with(var, "column."))
		return git_column_config(var, value, "branch", &colopts);
	if (!strcmp(var, "color.branch")) {
		branch_use_color = git_config_colorbool(var, value);
		return 0;
	}
	if (skip_prefix(var, "color.branch.", &slot_name)) {
		int slot = parse_branch_color_slot(slot_name);
		if (slot < 0)
			return 0;
		if (!value)
			return config_error_nonbool(var);
		return color_parse(value, branch_colors[slot]);
	}
	return git_color_default_config(var, value, cb);
}

static const char *branch_get_color(enum color_branch ix)
{
	if (want_color(branch_use_color))
		return branch_colors[ix];
	return "";
}

static int branch_merged(int kind, const char *name,
			 struct commit *rev, struct commit *head_rev)
{
	/*
	 * This checks whether the merge bases of branch and HEAD (or
	 * the other branch this branch builds upon) contains the
	 * branch, which means that the branch has already been merged
	 * safely to HEAD (or the other branch).
	 */
	struct commit *reference_rev = NULL;
	const char *reference_name = NULL;
	void *reference_name_to_free = NULL;
	int merged;

	if (kind == FILTER_REFS_BRANCHES) {
		struct branch *branch = branch_get(name);
		const char *upstream = branch_get_upstream(branch, NULL);
		unsigned char sha1[20];

		if (upstream &&
		    (reference_name = reference_name_to_free =
		     resolve_refdup(upstream, RESOLVE_REF_READING,
				    sha1, NULL)) != NULL)
			reference_rev = lookup_commit_reference(sha1);
	}
	if (!reference_rev)
		reference_rev = head_rev;

	merged = in_merge_bases(rev, reference_rev);

	/*
	 * After the safety valve is fully redefined to "check with
	 * upstream, if any, otherwise with HEAD", we should just
	 * return the result of the in_merge_bases() above without
	 * any of the following code, but during the transition period,
	 * a gentle reminder is in order.
	 */
	if ((head_rev != reference_rev) &&
	    in_merge_bases(rev, head_rev) != merged) {
		if (merged)
			warning(_("deleting branch '%s' that has been merged to\n"
				"         '%s', but not yet merged to HEAD."),
				name, reference_name);
		else
			warning(_("not deleting branch '%s' that is not yet merged to\n"
				"         '%s', even though it is merged to HEAD."),
				name, reference_name);
	}
	free(reference_name_to_free);
	return merged;
}

static int check_branch_commit(const char *branchname, const char *refname,
			       const unsigned char *sha1, struct commit *head_rev,
			       int kinds, int force)
{
	struct commit *rev = lookup_commit_reference(sha1);
	if (!rev) {
		error(_("Couldn't look up commit object for '%s'"), refname);
		return -1;
	}
	if (!force && !branch_merged(kinds, branchname, rev, head_rev)) {
		error(_("The branch '%s' is not fully merged.\n"
		      "If you are sure you want to delete it, "
		      "run 'git branch -D %s'."), branchname, branchname);
		return -1;
	}
	return 0;
}

static void delete_branch_config(const char *branchname)
{
	struct strbuf buf = STRBUF_INIT;
	strbuf_addf(&buf, "branch.%s", branchname);
	if (git_config_rename_section(buf.buf, NULL) < 0)
		warning(_("Update of config-file failed"));
	strbuf_release(&buf);
}

static int delete_branches(int argc, const char **argv, int force, int kinds,
			   int quiet)
{
	struct commit *head_rev = NULL;
	unsigned char sha1[20];
	char *name = NULL;
	const char *fmt;
	int i;
	int ret = 0;
	int remote_branch = 0;
	struct strbuf bname = STRBUF_INIT;

	switch (kinds) {
	case FILTER_REFS_REMOTES:
		fmt = "refs/remotes/%s";
		/* For subsequent UI messages */
		remote_branch = 1;

		force = 1;
		break;
	case FILTER_REFS_BRANCHES:
		fmt = "refs/heads/%s";
		break;
	default:
		die(_("cannot use -a with -d"));
	}

	if (!force) {
		head_rev = lookup_commit_reference(head_sha1);
		if (!head_rev)
			die(_("Couldn't look up commit object for HEAD"));
	}
	for (i = 0; i < argc; i++, strbuf_release(&bname)) {
		const char *target;
		int flags = 0;

		strbuf_branchname(&bname, argv[i]);
		free(name);
		name = mkpathdup(fmt, bname.buf);

		if (kinds == FILTER_REFS_BRANCHES) {
			const struct worktree *wt =
				find_shared_symref("HEAD", name);
			if (wt) {
				error(_("Cannot delete branch '%s' "
					"checked out at '%s'"),
				      bname.buf, wt->path);
				ret = 1;
				continue;
			}
		}

		target = resolve_ref_unsafe(name,
					    RESOLVE_REF_READING
					    | RESOLVE_REF_NO_RECURSE
					    | RESOLVE_REF_ALLOW_BAD_NAME,
					    sha1, &flags);
		if (!target) {
			error(remote_branch
			      ? _("remote-tracking branch '%s' not found.")
			      : _("branch '%s' not found."), bname.buf);
			ret = 1;
			continue;
		}

		if (!(flags & (REF_ISSYMREF|REF_ISBROKEN)) &&
		    check_branch_commit(bname.buf, name, sha1, head_rev, kinds,
					force)) {
			ret = 1;
			continue;
		}

		if (delete_ref(name, is_null_sha1(sha1) ? NULL : sha1,
			       REF_NODEREF)) {
			error(remote_branch
			      ? _("Error deleting remote-tracking branch '%s'")
			      : _("Error deleting branch '%s'"),
			      bname.buf);
			ret = 1;
			continue;
		}
		if (!quiet) {
			printf(remote_branch
			       ? _("Deleted remote-tracking branch %s (was %s).\n")
			       : _("Deleted branch %s (was %s).\n"),
			       bname.buf,
			       (flags & REF_ISBROKEN) ? "broken"
			       : (flags & REF_ISSYMREF) ? target
			       : find_unique_abbrev(sha1, DEFAULT_ABBREV));
		}
		delete_branch_config(bname.buf);
	}

	free(name);

	return(ret);
}

static void fill_tracking_info(struct strbuf *stat, const char *branch_name,
		int show_upstream_ref)
{
	int ours, theirs;
	char *ref = NULL;
	struct branch *branch = branch_get(branch_name);
	const char *upstream;
	struct strbuf fancy = STRBUF_INIT;
	int upstream_is_gone = 0;
	int added_decoration = 1;

	if (stat_tracking_info(branch, &ours, &theirs, &upstream) < 0) {
		if (!upstream)
			return;
		upstream_is_gone = 1;
	}

	if (show_upstream_ref) {
		ref = shorten_unambiguous_ref(upstream, 0);
		if (want_color(branch_use_color))
			strbuf_addf(&fancy, "%s%s%s",
					branch_get_color(BRANCH_COLOR_UPSTREAM),
					ref, branch_get_color(BRANCH_COLOR_RESET));
		else
			strbuf_addstr(&fancy, ref);
	}

	if (upstream_is_gone) {
		if (show_upstream_ref)
			strbuf_addf(stat, _("[%s: gone]"), fancy.buf);
		else
			added_decoration = 0;
	} else if (!ours && !theirs) {
		if (show_upstream_ref)
			strbuf_addf(stat, _("[%s]"), fancy.buf);
		else
			added_decoration = 0;
	} else if (!ours) {
		if (show_upstream_ref)
			strbuf_addf(stat, _("[%s: behind %d]"), fancy.buf, theirs);
		else
			strbuf_addf(stat, _("[behind %d]"), theirs);

	} else if (!theirs) {
		if (show_upstream_ref)
			strbuf_addf(stat, _("[%s: ahead %d]"), fancy.buf, ours);
		else
			strbuf_addf(stat, _("[ahead %d]"), ours);
	} else {
		if (show_upstream_ref)
			strbuf_addf(stat, _("[%s: ahead %d, behind %d]"),
				    fancy.buf, ours, theirs);
		else
			strbuf_addf(stat, _("[ahead %d, behind %d]"),
				    ours, theirs);
	}
	strbuf_release(&fancy);
	if (added_decoration)
		strbuf_addch(stat, ' ');
	free(ref);
}

static void add_verbose_info(struct strbuf *out, struct ref_array_item *item,
			     struct ref_filter *filter, const char *refname)
{
	struct strbuf subject = STRBUF_INIT, stat = STRBUF_INIT;
	const char *sub = _(" **** invalid ref ****");
	struct commit *commit = item->commit;

	if (!parse_commit(commit)) {
		pp_commit_easy(CMIT_FMT_ONELINE, commit, &subject);
		sub = subject.buf;
	}

	if (item->kind == FILTER_REFS_BRANCHES)
		fill_tracking_info(&stat, refname, filter->verbose > 1);

	strbuf_addf(out, " %s %s%s",
		find_unique_abbrev(item->commit->object.oid.hash, filter->abbrev),
		stat.buf, sub);
	strbuf_release(&stat);
	strbuf_release(&subject);
}

static char *get_head_description(void)
{
	struct strbuf desc = STRBUF_INIT;
	struct wt_status_state state;
	memset(&state, 0, sizeof(state));
	wt_status_get_state(&state, 1);
	if (state.rebase_in_progress ||
	    state.rebase_interactive_in_progress)
		strbuf_addf(&desc, _("(no branch, rebasing %s)"),
			    state.branch);
	else if (state.bisect_in_progress)
		strbuf_addf(&desc, _("(no branch, bisect started on %s)"),
			    state.branch);
	else if (state.detached_from) {
		if (state.detached_at)
			/* TRANSLATORS: make sure this matches
			   "HEAD detached at " in wt-status.c */
			strbuf_addf(&desc, _("(HEAD detached at %s)"),
				state.detached_from);
		else
			/* TRANSLATORS: make sure this matches
			   "HEAD detached from " in wt-status.c */
			strbuf_addf(&desc, _("(HEAD detached from %s)"),
				state.detached_from);
	}
	else
		strbuf_addstr(&desc, _("(no branch)"));
	free(state.branch);
	free(state.onto);
	free(state.detached_from);
	return strbuf_detach(&desc, NULL);
}

static void format_and_print_ref_item(struct ref_array_item *item, int maxwidth,
				      struct ref_filter *filter, const char *remote_prefix)
{
	char c;
	int current = 0;
	int color;
	struct strbuf out = STRBUF_INIT, name = STRBUF_INIT;
	const char *prefix_to_show = "";
	const char *prefix_to_skip = NULL;
	const char *desc = item->refname;
	char *to_free = NULL;

	switch (item->kind) {
	case FILTER_REFS_BRANCHES:
		prefix_to_skip = "refs/heads/";
		skip_prefix(desc, prefix_to_skip, &desc);
		if (!filter->detached && !strcmp(desc, head))
			current = 1;
		else
			color = BRANCH_COLOR_LOCAL;
		break;
	case FILTER_REFS_REMOTES:
		prefix_to_skip = "refs/remotes/";
		skip_prefix(desc, prefix_to_skip, &desc);
		color = BRANCH_COLOR_REMOTE;
		prefix_to_show = remote_prefix;
		break;
	case FILTER_REFS_DETACHED_HEAD:
		desc = to_free = get_head_description();
		current = 1;
		break;
	default:
		color = BRANCH_COLOR_PLAIN;
		break;
	}

	c = ' ';
	if (current) {
		c = '*';
		color = BRANCH_COLOR_CURRENT;
	}

	strbuf_addf(&name, "%s%s", prefix_to_show, desc);
	if (filter->verbose) {
		int utf8_compensation = strlen(name.buf) - utf8_strwidth(name.buf);
		strbuf_addf(&out, "%c %s%-*s%s", c, branch_get_color(color),
			    maxwidth + utf8_compensation, name.buf,
			    branch_get_color(BRANCH_COLOR_RESET));
	} else
		strbuf_addf(&out, "%c %s%s%s", c, branch_get_color(color),
			    name.buf, branch_get_color(BRANCH_COLOR_RESET));

	if (item->symref) {
		const char *symref = item->symref;
		if (prefix_to_skip)
			skip_prefix(symref, prefix_to_skip, &symref);
		strbuf_addf(&out, " -> %s", symref);
	}
	else if (filter->verbose)
		/* " f7c0c00 [ahead 58, behind 197] vcs-svn: drop obj_pool.h" */
		add_verbose_info(&out, item, filter, desc);
	if (column_active(colopts)) {
		assert(!filter->verbose && "--column and --verbose are incompatible");
		string_list_append(&output, out.buf);
	} else {
		printf("%s\n", out.buf);
	}
	strbuf_release(&name);
	strbuf_release(&out);
	free(to_free);
}

static int calc_maxwidth(struct ref_array *refs, int remote_bonus)
{
	int i, max = 0;
	for (i = 0; i < refs->nr; i++) {
		struct ref_array_item *it = refs->items[i];
		const char *desc = it->refname;
		int w;

		skip_prefix(it->refname, "refs/heads/", &desc);
		skip_prefix(it->refname, "refs/remotes/", &desc);
		w = utf8_strwidth(desc);

		if (it->kind == FILTER_REFS_REMOTES)
			w += remote_bonus;
		if (w > max)
			max = w;
	}
	return max;
}

static void print_ref_list(struct ref_filter *filter, struct ref_sorting *sorting)
{
	int i;
	struct ref_array array;
	int maxwidth = 0;
	const char *remote_prefix = "";

	/*
	 * If we are listing more than just remote branches,
	 * then remote branches will have a "remotes/" prefix.
	 * We need to account for this in the width.
	 */
	if (filter->kind != FILTER_REFS_REMOTES)
		remote_prefix = "remotes/";

	memset(&array, 0, sizeof(array));

	verify_ref_format("%(refname)%(symref)");
	filter_refs(&array, filter, filter->kind | FILTER_REFS_INCLUDE_BROKEN);

	if (filter->verbose)
		maxwidth = calc_maxwidth(&array, strlen(remote_prefix));

	/*
	 * If no sorting parameter is given then we default to sorting
	 * by 'refname'. This would give us an alphabetically sorted
	 * array with the 'HEAD' ref at the beginning followed by
	 * local branches 'refs/heads/...' and finally remote-tacking
	 * branches 'refs/remotes/...'.
	 */
	if (!sorting)
		sorting = ref_default_sorting();
	ref_array_sort(sorting, &array);

	for (i = 0; i < array.nr; i++)
		format_and_print_ref_item(array.items[i], maxwidth, filter, remote_prefix);

	ref_array_clear(&array);
}

static void reject_rebase_or_bisect_branch(const char *target)
{
	struct worktree **worktrees = get_worktrees();
	int i;

	for (i = 0; worktrees[i]; i++) {
		struct worktree *wt = worktrees[i];

		if (!wt->is_detached)
			continue;

		if (is_worktree_being_rebased(wt, target))
			die(_("Branch %s is being rebased at %s"),
			    target, wt->path);

		if (is_worktree_being_bisected(wt, target))
			die(_("Branch %s is being bisected at %s"),
			    target, wt->path);
	}

	free_worktrees(worktrees);
}

static void rename_branch(const char *oldname, const char *newname, int force)
{
	struct strbuf oldref = STRBUF_INIT, newref = STRBUF_INIT, logmsg = STRBUF_INIT;
	struct strbuf oldsection = STRBUF_INIT, newsection = STRBUF_INIT;
	int recovery = 0;
	int clobber_head_ok;

	if (!oldname)
		die(_("cannot rename the current branch while not on any."));

	if (strbuf_check_branch_ref(&oldref, oldname)) {
		/*
		 * Bad name --- this could be an attempt to rename a
		 * ref that we used to allow to be created by accident.
		 */
		if (ref_exists(oldref.buf))
			recovery = 1;
		else
			die(_("Invalid branch name: '%s'"), oldname);
	}

	/*
	 * A command like "git branch -M currentbranch currentbranch" cannot
	 * cause the worktree to become inconsistent with HEAD, so allow it.
	 */
	clobber_head_ok = !strcmp(oldname, newname);

	validate_new_branchname(newname, &newref, force, clobber_head_ok);

	reject_rebase_or_bisect_branch(oldref.buf);

	strbuf_addf(&logmsg, "Branch: renamed %s to %s",
		 oldref.buf, newref.buf);

	if (rename_ref(oldref.buf, newref.buf, logmsg.buf))
		die(_("Branch rename failed"));
	strbuf_release(&logmsg);

	if (recovery)
		warning(_("Renamed a misnamed branch '%s' away"), oldref.buf + 11);

	if (replace_each_worktree_head_symref(oldref.buf, newref.buf))
		die(_("Branch renamed to %s, but HEAD is not updated!"), newname);

	strbuf_addf(&oldsection, "branch.%s", oldref.buf + 11);
	strbuf_release(&oldref);
	strbuf_addf(&newsection, "branch.%s", newref.buf + 11);
	strbuf_release(&newref);
	if (git_config_rename_section(oldsection.buf, newsection.buf) < 0)
		die(_("Branch is renamed, but update of config-file failed"));
	strbuf_release(&oldsection);
	strbuf_release(&newsection);
}

static const char edit_description[] = "BRANCH_DESCRIPTION";

static int edit_branch_description(const char *branch_name)
{
	struct strbuf buf = STRBUF_INIT;
	struct strbuf name = STRBUF_INIT;

	read_branch_desc(&buf, branch_name);
	if (!buf.len || buf.buf[buf.len-1] != '\n')
		strbuf_addch(&buf, '\n');
	strbuf_commented_addf(&buf,
		    "Please edit the description for the branch\n"
		    "  %s\n"
		    "Lines starting with '%c' will be stripped.\n",
		    branch_name, comment_line_char);
	write_file(git_path(edit_description), "%s", buf.buf);
	strbuf_reset(&buf);
	if (launch_editor(git_path(edit_description), &buf, NULL)) {
		strbuf_release(&buf);
		return -1;
	}
	strbuf_stripspace(&buf, 1);

	strbuf_addf(&name, "branch.%s.description", branch_name);
	git_config_set(name.buf, buf.len ? buf.buf : NULL);
	strbuf_release(&name);
	strbuf_release(&buf);

	return 0;
}

int cmd_branch(int argc, const char **argv, const char *prefix)
{
	int delete = 0, rename = 0, force = 0, list = 0;
	int reflog = 0, edit_description = 0;
	int quiet = 0, unset_upstream = 0;
	const char *new_upstream = NULL;
	enum branch_track track;
	struct ref_filter filter;
	static struct ref_sorting *sorting = NULL, **sorting_tail = &sorting;

	struct option options[] = {
		OPT_GROUP(N_("Generic options")),
		OPT__VERBOSE(&filter.verbose,
			N_("show hash and subject, give twice for upstream branch")),
		OPT__QUIET(&quiet, N_("suppress informational messages")),
		OPT_SET_INT('t', "track",  &track, N_("set up tracking mode (see git-pull(1))"),
			BRANCH_TRACK_EXPLICIT),
		OPT_SET_INT( 0, "set-upstream",  &track, N_("change upstream info"),
			BRANCH_TRACK_OVERRIDE),
		OPT_STRING('u', "set-upstream-to", &new_upstream, N_("upstream"), N_("change the upstream info")),
		OPT_BOOL(0, "unset-upstream", &unset_upstream, "Unset the upstream info"),
		OPT__COLOR(&branch_use_color, N_("use colored output")),
		OPT_SET_INT('r', "remotes",     &filter.kind, N_("act on remote-tracking branches"),
			FILTER_REFS_REMOTES),
		OPT_CONTAINS(&filter.with_commit, N_("print only branches that contain the commit")),
		OPT_WITH(&filter.with_commit, N_("print only branches that contain the commit")),
		OPT__ABBREV(&filter.abbrev),

		OPT_GROUP(N_("Specific git-branch actions:")),
		OPT_SET_INT('a', "all", &filter.kind, N_("list both remote-tracking and local branches"),
			FILTER_REFS_REMOTES | FILTER_REFS_BRANCHES),
		OPT_BIT('d', "delete", &delete, N_("delete fully merged branch"), 1),
		OPT_BIT('D', NULL, &delete, N_("delete branch (even if not merged)"), 2),
		OPT_BIT('m', "move", &rename, N_("move/rename a branch and its reflog"), 1),
		OPT_BIT('M', NULL, &rename, N_("move/rename a branch, even if target exists"), 2),
		OPT_BOOL(0, "list", &list, N_("list branch names")),
		OPT_BOOL('l', "create-reflog", &reflog, N_("create the branch's reflog")),
		OPT_BOOL(0, "edit-description", &edit_description,
			 N_("edit the description for the branch")),
		OPT__FORCE(&force, N_("force creation, move/rename, deletion")),
		OPT_MERGED(&filter, N_("print only branches that are merged")),
		OPT_NO_MERGED(&filter, N_("print only branches that are not merged")),
		OPT_COLUMN(0, "column", &colopts, N_("list branches in columns")),
		OPT_CALLBACK(0 , "sort", sorting_tail, N_("key"),
			     N_("field name to sort on"), &parse_opt_ref_sorting),
		{
			OPTION_CALLBACK, 0, "points-at", &filter.points_at, N_("object"),
			N_("print only branches of the object"), 0, parse_opt_object_name
		},
		OPT_END(),
	};

	memset(&filter, 0, sizeof(filter));
	filter.kind = FILTER_REFS_BRANCHES;
	filter.abbrev = -1;

	if (argc == 2 && !strcmp(argv[1], "-h"))
		usage_with_options(builtin_branch_usage, options);

	git_config(git_branch_config, NULL);

	track = git_branch_track;

	head = resolve_refdup("HEAD", 0, head_sha1, NULL);
	if (!head)
		die(_("Failed to resolve HEAD as a valid ref."));
	if (!strcmp(head, "HEAD"))
		filter.detached = 1;
	else if (!skip_prefix(head, "refs/heads/", &head))
		die(_("HEAD not found below refs/heads!"));

	argc = parse_options(argc, argv, prefix, options, builtin_branch_usage,
			     0);

	if (!delete && !rename && !edit_description && !new_upstream && !unset_upstream && argc == 0)
		list = 1;

	if (filter.with_commit || filter.merge != REF_FILTER_MERGED_NONE || filter.points_at.nr)
		list = 1;

	if (!!delete + !!rename + !!new_upstream +
	    list + unset_upstream > 1)
		usage_with_options(builtin_branch_usage, options);

	if (filter.abbrev == -1)
		filter.abbrev = DEFAULT_ABBREV;
	finalize_colopts(&colopts, -1);
	if (filter.verbose) {
		if (explicitly_enable_column(colopts))
			die(_("--column and --verbose are incompatible"));
		colopts = 0;
	}

	if (force) {
		delete *= 2;
		rename *= 2;
	}

	if (delete) {
		if (!argc)
			die(_("branch name required"));
		return delete_branches(argc, argv, delete > 1, filter.kind, quiet);
	} else if (list) {
		/*  git branch --local also shows HEAD when it is detached */
		if ((filter.kind & FILTER_REFS_BRANCHES) && filter.detached)
			filter.kind |= FILTER_REFS_DETACHED_HEAD;
		filter.name_patterns = argv;
		print_ref_list(&filter, sorting);
		print_columns(&output, colopts, NULL);
		string_list_clear(&output, 0);
		return 0;
	}
	else if (edit_description) {
		const char *branch_name;
		struct strbuf branch_ref = STRBUF_INIT;

		if (!argc) {
			if (filter.detached)
				die(_("Cannot give description to detached HEAD"));
			branch_name = head;
		} else if (argc == 1)
			branch_name = argv[0];
		else
			die(_("cannot edit description of more than one branch"));

		strbuf_addf(&branch_ref, "refs/heads/%s", branch_name);
		if (!ref_exists(branch_ref.buf)) {
			strbuf_release(&branch_ref);

			if (!argc)
				return error(_("No commit on branch '%s' yet."),
					     branch_name);
			else
				return error(_("No branch named '%s'."),
					     branch_name);
		}
		strbuf_release(&branch_ref);

		if (edit_branch_description(branch_name))
			return 1;
	} else if (rename) {
		if (!argc)
			die(_("branch name required"));
		else if (argc == 1)
			rename_branch(head, argv[0], rename > 1);
		else if (argc == 2)
			rename_branch(argv[0], argv[1], rename > 1);
		else
			die(_("too many branches for a rename operation"));
	} else if (new_upstream) {
		struct branch *branch = branch_get(argv[0]);

		if (argc > 1)
			die(_("too many branches to set new upstream"));

		if (!branch) {
			if (!argc || !strcmp(argv[0], "HEAD"))
				die(_("could not set upstream of HEAD to %s when "
				      "it does not point to any branch."),
				    new_upstream);
			die(_("no such branch '%s'"), argv[0]);
		}

		if (!ref_exists(branch->refname))
			die(_("branch '%s' does not exist"), branch->name);

		/*
		 * create_branch takes care of setting up the tracking
		 * info and making sure new_upstream is correct
		 */
		create_branch(head, branch->name, new_upstream, 0, 0, 0, quiet, BRANCH_TRACK_OVERRIDE);
	} else if (unset_upstream) {
		struct branch *branch = branch_get(argv[0]);
		struct strbuf buf = STRBUF_INIT;

		if (argc > 1)
			die(_("too many branches to unset upstream"));

		if (!branch) {
			if (!argc || !strcmp(argv[0], "HEAD"))
				die(_("could not unset upstream of HEAD when "
				      "it does not point to any branch."));
			die(_("no such branch '%s'"), argv[0]);
		}

		if (!branch_has_merge_config(branch))
			die(_("Branch '%s' has no upstream information"), branch->name);

		strbuf_addf(&buf, "branch.%s.remote", branch->name);
		git_config_set_multivar(buf.buf, NULL, NULL, 1);
		strbuf_reset(&buf);
		strbuf_addf(&buf, "branch.%s.merge", branch->name);
		git_config_set_multivar(buf.buf, NULL, NULL, 1);
		strbuf_release(&buf);
	} else if (argc > 0 && argc <= 2) {
		struct branch *branch = branch_get(argv[0]);
		int branch_existed = 0, remote_tracking = 0;
		struct strbuf buf = STRBUF_INIT;

		if (!strcmp(argv[0], "HEAD"))
			die(_("it does not make sense to create 'HEAD' manually"));

		if (!branch)
			die(_("no such branch '%s'"), argv[0]);

		if (filter.kind != FILTER_REFS_BRANCHES)
			die(_("-a and -r options to 'git branch' do not make sense with a branch name"));

		if (track == BRANCH_TRACK_OVERRIDE)
			fprintf(stderr, _("The --set-upstream flag is deprecated and will be removed. Consider using --track or --set-upstream-to\n"));

		strbuf_addf(&buf, "refs/remotes/%s", branch->name);
		remote_tracking = ref_exists(buf.buf);
		strbuf_release(&buf);

		branch_existed = ref_exists(branch->refname);
		create_branch(head, argv[0], (argc == 2) ? argv[1] : head,
			      force, reflog, 0, quiet, track);

		/*
		 * We only show the instructions if the user gave us
		 * one branch which doesn't exist locally, but is the
		 * name of a remote-tracking branch.
		 */
		if (argc == 1 && track == BRANCH_TRACK_OVERRIDE &&
		    !branch_existed && remote_tracking) {
			fprintf(stderr, _("\nIf you wanted to make '%s' track '%s', do this:\n\n"), head, branch->name);
			fprintf(stderr, "    git branch -d %s\n", branch->name);
			fprintf(stderr, "    git branch --set-upstream-to %s\n", branch->name);
		}

	} else
		usage_with_options(builtin_branch_usage, options);

	return 0;
}

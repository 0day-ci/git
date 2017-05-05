#include "builtin.h"
#include "color.h"
#include "diff.h"
#include "diffcore.h"
#include "revision.h"

#define ADD_INTERACTIVE_HEADER_INDENT "      "

enum add_interactive_collection_mode {
	COLLECTION_MODE_NONE,
	COLLECTION_MODE_WORKTREE,
	COLLECTION_MODE_INDEX
};

struct add_interactive_file_status {
	int selected;

	char path[PATH_MAX];

	int lines_added_index;
	int lines_deleted_index;
	int lines_added_worktree;
	int lines_deleted_worktree;
};

struct add_interactive_status {
	enum add_interactive_collection_mode current_mode;

	const char *reference;
	struct pathspec pathspec;

	int file_count;
	struct add_interactive_file_status *files;
};

static int add_interactive_use_color = -1;
enum color_add_interactive {
	ADD_INTERACTIVE_PROMPT,
	ADD_INTERACTIVE_HEADER,
	ADD_INTERACTIVE_HELP,
	ADD_INTERACTIVE_ERROR
};

static char add_interactive_colors[][COLOR_MAXLEN] = {
	GIT_COLOR_BOLD_BLUE, /* Prompt */
	GIT_COLOR_BOLD,      /* Header */
	GIT_COLOR_BOLD_RED,  /* Help */
	GIT_COLOR_BOLD_RED   /* Error */
};

static const char *add_interactive_get_color(enum color_add_interactive ix)
{
	if (want_color(add_interactive_use_color))
		return add_interactive_colors[ix];
	return "";
}

static int parse_add_interactive_color_slot(const char *slot)
{
	if (!strcasecmp(slot, "prompt"))
		return ADD_INTERACTIVE_PROMPT;
	if (!strcasecmp(slot, "header"))
		return ADD_INTERACTIVE_HEADER;
	if (!strcasecmp(slot, "help"))
		return ADD_INTERACTIVE_HELP;
	if (!strcasecmp(slot, "error"))
		return ADD_INTERACTIVE_ERROR;

	return -1;
}

static int git_add_interactive_config(const char *var,
		const char *value, void *cbdata)
{
	const char *name;

	if (!strcmp(var, "color.interactive")) {
		add_interactive_use_color = git_config_colorbool(var, value);
		return 0;
	}

	if (skip_prefix(var, "color.interactive", &name)) {
		int slot = parse_add_interactive_color_slot(name);
		if (slot < 0)
			return 0;
		if (!value)
			return config_error_nonbool(var);
		return color_parse(value, add_interactive_colors[slot]);
	}

	return git_default_config(var, value, cbdata);
}

static void add_interactive_status_collect_changed_cb(struct diff_queue_struct *q,
					 struct diff_options *options,
					 void *data)
{
	struct add_interactive_status *s = data;
	struct diffstat_t stat;
	int i, j;

	if (!q->nr)
		return;

	memset(&stat, 0, sizeof(struct diffstat_t));
	for (i = 0; i < q->nr; i++) {
		struct diff_filepair *p;
		p = q->queue[i];
		diff_flush_stat(p, options, &stat);
	}

	for (i = 0; i < stat.nr; i++) {
		int file_index = s->file_count;
		for (j = 0; j < s->file_count; j++) {
			if (!strcmp(s->files[j].path, stat.files[i]->name)) {
				file_index = j;
				break;
			}
		}

		if (file_index == s->file_count) {
			s->file_count++;
			s->files = realloc(s->files,
					(q->nr + s->file_count) * sizeof(*s->files));
			memset(&s->files[file_index], 0,
					sizeof(struct add_interactive_file_status));
		}

		memcpy(s->files[file_index].path, stat.files[i]->name,
				strlen(stat.files[i]->name) + 1);
		if (s->current_mode == COLLECTION_MODE_WORKTREE) {
			s->files[file_index].lines_added_worktree = stat.files[i]->added;
			s->files[file_index].lines_deleted_worktree = stat.files[i]->deleted;
		} else if (s->current_mode == COLLECTION_MODE_INDEX) {
			s->files[file_index].lines_added_index = stat.files[i]->added;
			s->files[file_index].lines_deleted_index = stat.files[i]->deleted;
		}
	}
}

static void add_interactive_status_collect_changes_worktree(struct add_interactive_status *s)
{
	struct rev_info rev;

	s->current_mode = COLLECTION_MODE_WORKTREE;

	init_revisions(&rev, NULL);
	setup_revisions(0, NULL, &rev, NULL);

	rev.max_count = 0;

	rev.diffopt.output_format = DIFF_FORMAT_CALLBACK;
	rev.diffopt.format_callback = add_interactive_status_collect_changed_cb;
	rev.diffopt.format_callback_data = s;

	run_diff_files(&rev, 0);
}

static void add_interactive_status_collect_changes_index(struct add_interactive_status *s)
{
	struct rev_info rev;
	struct setup_revision_opt opt;

	s->current_mode = COLLECTION_MODE_INDEX;

	init_revisions(&rev, NULL);
	memset(&opt, 0, sizeof(opt));
	opt.def = s->reference;
	setup_revisions(0, NULL, &rev, &opt);

	rev.diffopt.output_format = DIFF_FORMAT_CALLBACK;
	rev.diffopt.format_callback = add_interactive_status_collect_changed_cb;
	rev.diffopt.format_callback_data = s;

	run_diff_index(&rev, 1);
}

static void list_modified_into_status(struct add_interactive_status *s)
{
	add_interactive_status_collect_changes_worktree(s);
	add_interactive_status_collect_changes_index(s);
}

static void print_modified(void)
{
	int i;
	struct add_interactive_status s;
	const char *modified_fmt = _("%12s %12s %s");
	const char *header_color = add_interactive_get_color(ADD_INTERACTIVE_HEADER);
	unsigned char sha1[20];

	if (read_cache() < 0)
		return;

	s.current_mode = COLLECTION_MODE_NONE;
	s.reference = !get_sha1("HEAD", sha1) ? "HEAD": EMPTY_TREE_SHA1_HEX;
	s.file_count = 0;
	s.files = NULL;
	list_modified_into_status(&s);

	printf(ADD_INTERACTIVE_HEADER_INDENT);
	color_fprintf(stdout, header_color, modified_fmt, _("staged"),
			_("unstaged"), _("path"));
	printf("\n");

	for (i = 0; i < s.file_count; i++) {
		struct add_interactive_file_status f = s.files[i];
		char selection = f.selected ? '*' : ' ';

		char worktree_changes[50];
		char index_changes[50];

		if (f.lines_added_worktree != 0 || f.lines_deleted_worktree != 0)
			snprintf(worktree_changes, 50, "+%d/-%d", f.lines_added_worktree,
					f.lines_deleted_worktree);
		else
			snprintf(worktree_changes, 50, "%s", _("nothing"));

		if (f.lines_added_index != 0 || f.lines_deleted_index != 0)
			snprintf(index_changes, 50, "+%d/-%d", f.lines_added_index,
					f.lines_deleted_index);
		else
			snprintf(index_changes, 50, "%s", _("unchanged"));

		printf("%c%2d: ", selection, i + 1);
		printf(modified_fmt, index_changes, worktree_changes, f.path);
		printf("\n");
	}
}

static void status_cmd(void)
{
	print_modified();
}

static const char add_interactive_helper_usage[] =
"git add-interactive--helper <command>";

int cmd_add_interactive__helper(int argc, const char **argv, const char *prefix)
{
	int i, found_opt = 0;

	git_config(git_add_interactive_config, NULL);

	for (i = 1; i < argc; i++) {
		const char *arg = argv[i];

		if (!strcmp(arg, "--status")) {
			status_cmd();
			found_opt = 1;
		}
	}

	if (!found_opt)
		usage(add_interactive_helper_usage);

	return 0;
}

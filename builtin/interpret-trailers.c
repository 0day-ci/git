/*
 * Builtin "git interpret-trailers"
 *
 * Copyright (c) 2013, 2014 Christian Couder <chriscool@tuxfamily.org>
 *
 */

#include "cache.h"
#include "builtin.h"
#include "parse-options.h"
#include "string-list.h"
#include "trailer.h"

static const char * const git_interpret_trailers_usage[] = {
	N_("git interpret-trailers [--in-place] [--trim-empty] [(--trailer <token>[(=|:)<value>])...] [<file>...]"),
	NULL
};

static int option_parse_where(const struct option *opt,
			      const char *arg, int unset)
{
	enum action_where *where = opt->value;

	if (unset)
		return 0;

	return set_where(where, arg);
}

static int option_parse_if_exists(const struct option *opt,
				  const char *arg, int unset)
{
	enum action_if_exists *if_exists = opt->value;

	if (unset)
		return 0;

	return set_if_exists(if_exists, arg);
}

static int option_parse_if_missing(const struct option *opt,
				   const char *arg, int unset)
{
	enum action_if_missing *if_missing = opt->value;

	if (unset)
		return 0;

	return set_if_missing(if_missing, arg);
}

int cmd_interpret_trailers(int argc, const char **argv, const char *prefix)
{
	struct trailer_opts opts = { 0 };
	struct string_list trailers = STRING_LIST_INIT_NODUP;

	struct option options[] = {
		OPT_BOOL(0, "in-place", &opts.in_place, N_("edit files in place")),
		OPT_BOOL(0, "trim-empty", &opts.trim_empty, N_("trim empty trailers")),
		OPT_CALLBACK(0, "where", &opts.where, N_("action"),
			     N_("where to place the new trailer"), option_parse_where),
		OPT_CALLBACK(0, "if-exists", &opts.if_exists, N_("action"),
			     N_("action if trailer already exists"), option_parse_if_exists),
		OPT_CALLBACK(0, "if-missing", &opts.if_missing, N_("action"),
			     N_("action if trailer is missing"), option_parse_if_missing),

		OPT_STRING_LIST(0, "trailer", &trailers, N_("trailer"),
				N_("trailer(s) to add")),
		OPT_END()
	};

	argc = parse_options(argc, argv, prefix, options,
			     git_interpret_trailers_usage, 0);

	if (argc) {
		int i;
		for (i = 0; i < argc; i++)
			process_trailers(argv[i], &opts, &trailers);
	} else {
		if (opts.in_place)
			die(_("no input file given for in-place editing"));
		process_trailers(NULL, &opts, &trailers);
	}

	string_list_clear(&trailers, 0);

	return 0;
}

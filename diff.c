static int diff_indent_heuristic; /* experimental */
		options->submodule_format = DIFF_SUBMODULE_LOG;
	else if (!strcmp(value, "diff"))
		options->submodule_format = DIFF_SUBMODULE_DIFF;
		options->submodule_format = DIFF_SUBMODULE_SHORT;
	if (!strcmp(var, "diff.indentheuristic")) {
		diff_indent_heuristic = git_config_bool(var, value);
		return 0;
	}

	if (!opt->output_prefix) {
		if (opt->line_prefix)
			return opt->line_prefix;
		else
			return "";
	}
	/* line prefix must be printed before the output_prefix() */
	if (opt->line_prefix)
		strbuf_insert(msgbuf, 0, opt->line_prefix, strlen(opt->line_prefix));
		width = term_columns() - strlen(line_prefix);
	diff_set_mnemonic_prefix(o, "a/", "b/");
	if (DIFF_OPT_TST(o, REVERSE_DIFF)) {
		a_prefix = o->b_prefix;
		b_prefix = o->a_prefix;
	} else {
		a_prefix = o->a_prefix;
		b_prefix = o->b_prefix;
	}

	if (o->submodule_format == DIFF_SUBMODULE_LOG &&
	    (!one->mode || S_ISGITLINK(one->mode)) &&
	    (!two->mode || S_ISGITLINK(two->mode))) {
	} else if (o->submodule_format == DIFF_SUBMODULE_DIFF &&
		   (!one->mode || S_ISGITLINK(one->mode)) &&
		   (!two->mode || S_ISGITLINK(two->mode))) {
		show_submodule_diff(o->file, one->path ? one->path : two->path,
				line_prefix,
				one->oid.hash, two->oid.hash,
				two->dirty_submodule,
				meta, a_prefix, b_prefix, reset);
		return;
	if (diff_indent_heuristic)
		DIFF_XDL_SET(options, INDENT_HEURISTIC);
	else if (!strcmp(arg, "--indent-heuristic"))
		DIFF_XDL_SET(options, INDENT_HEURISTIC);
	else if (!strcmp(arg, "--no-indent-heuristic"))
		DIFF_XDL_CLR(options, INDENT_HEURISTIC);
		options->submodule_format = DIFF_SUBMODULE_LOG;
	else if ((argcount = parse_long_opt("diff-line-prefix", av, &optarg))) {
		options->line_prefix = optarg;
		return argcount;
	}
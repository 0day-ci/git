#ifndef TRAILER_H
#define TRAILER_H

struct trailer_info {
	/*
	 * True if there is a blank line before the location pointed to by
	 * trailer_start.
	 */
	int blank_line_before_trailer;

	/*
	 * Pointers to the start and end of the trailer block found. If there
	 * is no trailer block found, these 2 pointers point to the end of the
	 * input string.
	 */
	const char *trailer_start, *trailer_end;

	/*
	 * Array of trailers found.
	 */
	char **trailers;
	size_t trailer_nr;
};

struct process_trailer_options {
	int in_place;
	int trim_empty;
	int only_trailers;
	int only_input;
	int normalize;
};

#define PROCESS_TRAILER_OPTIONS_INIT {0}

void process_trailers(const char *file,
		      const struct process_trailer_options *opts,
		      struct string_list *trailers);

void trailer_info_get(struct trailer_info *info, const char *str);

void trailer_info_release(struct trailer_info *info);

#endif /* TRAILER_H */

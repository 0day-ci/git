#ifndef TRAILER_H
#define TRAILER_H

enum action_where {
	WHERE_DEFAULT,
	WHERE_END,
	WHERE_AFTER,
	WHERE_BEFORE,
	WHERE_START
};
enum action_if_exists {
	EXISTS_DEFAULT,
	EXISTS_ADD_IF_DIFFERENT_NEIGHBOR,
	EXISTS_ADD_IF_DIFFERENT,
	EXISTS_ADD,
	EXISTS_REPLACE,
	EXISTS_DO_NOTHING
};
enum action_if_missing {
	MISSING_DEFAULT,
	MISSING_ADD,
	MISSING_DO_NOTHING
};

struct trailer_opts {
	int in_place;
	int trim_empty;
	enum action_where where;
	enum action_if_exists if_exists;
	enum action_if_missing if_missing;
};

int set_where(enum action_where *item, const char *value);
int set_if_exists(enum action_if_exists *item, const char *value);
int set_if_missing(enum action_if_missing *item, const char *value);

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

void process_trailers(const char *file, struct trailer_opts *opts,
		      struct string_list *trailers);

void trailer_info_get(struct trailer_info *info, const char *str);

void trailer_info_release(struct trailer_info *info);

#endif /* TRAILER_H */

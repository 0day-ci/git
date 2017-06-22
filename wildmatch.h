#ifndef WILDMATCH_H
#define WILDMATCH_H

#define WM_CASEFOLD 1
#define WM_PATHNAME 2

#define WM_ABORT_MALFORMED 2
#define WM_NOMATCH 1
#define WM_MATCH 0
#define WM_ABORT_ALL -1
#define WM_ABORT_TO_STARSTAR -2

struct wildmatch_compiled {
	const char *pattern;
	unsigned int flags;
};

int wildmatch(const char *pattern, const char *text, unsigned int flags);
struct wildmatch_compiled *wildmatch_compile(const char *pattern, unsigned int flags);
int wildmatch_match(struct wildmatch_compiled *code, const char *text);
void wildmatch_free(struct wildmatch_compiled *code);

#endif

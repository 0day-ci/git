#include "cache.h"

struct test_data {
	const char *s1;
	const char *s2;
	int first_change;
};

static struct test_data data[] = {
	{ "abc", "abc", 0 },
	{ "abc", "def", 0 },

	{ "abc", "abz", 2 },

	{ "abc", "abcdef", 3 },

	{ "abc\xF0zzz", "abc\xFFzzz", 3 },

	{ NULL, NULL, 0 }
};

int try_pair(const char *sa, const char *sb, int first_change)
{
	int failed = 0;
	int offset, r_exp, r_tst;
	int r_exp_sign, r_tst_sign;

	/*
	 * Because differnt CRTs behave differently, only rely on signs
	 * of the result values.
	 */
	r_exp = strcmp(sa, sb);
	r_exp_sign = ((r_exp < 0) ? -1 : ((r_exp == 0) ? 0 : 1));

	r_tst = strcmp_offset(sa, sb, &offset);
	r_tst_sign = ((r_tst < 0) ? -1 : ((r_tst == 0) ? 0 : 1));

	if (r_tst_sign != r_exp_sign) {
		error("FAIL: '%s' vs '%s', result expect %d, observed %d\n",
			  sa, sb, r_exp_sign, r_tst_sign);
		failed = 1;
	}

	if (offset != first_change) {
		error("FAIL: '%s' vs '%s', offset expect %d, observed %d\n",
			  sa, sb, first_change, offset);
		failed = 1;
	}

	return failed;
}

int cmd_main(int argc, const char **argv)
{
	int failed = 0;
	int k;

	for (k=0; data[k].s1; k++) {
		failed += try_pair(data[k].s1, data[k].s2, data[k].first_change);
		failed += try_pair(data[k].s2, data[k].s1, data[k].first_change);
	}

	return failed;
}

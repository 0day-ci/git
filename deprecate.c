#include "cache.h"
#include "deprecate.h"

void deprecate(int *state, const char *message,
		int dep_at, int warn_at, int die_at, int remove_at)
{
	/*
	 * If we're going to warn let's do it once per-process, not
	 * spew lots of warnings in a loop.
	 */
	if (*state == 1)
		return;
	else
		*state = 1;

	if (remove_at >= GIT_VERSION_INT) {
		die("BUG: The '%s' deprecation should be removed in this release!");
	} else if (die_at >= GIT_VERSION_INT) {
		die(_("Deprecation error: %s"), message);
	} else if (warn_at >= GIT_VERSION_INT) {
		warning(_("Deprecation warning: %s"), message);
	} else if (1) {
		/*
		 * TODO: Instead of `if 1` we should check a
		 * core.version variable here.
		 *
		 * I.e. if set to core.version=2.13 the user is opting
		 * in to get deprecations set at dep_at right away,
		 * and also perhaps experimental features from a
		 * sister experimental() interface.
		 */
		die(_("Early bird deprecation error: %s"), message);
	}
}

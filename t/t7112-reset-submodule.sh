#!/bin/sh

test_fails_on_unusual_directory_names=1
test_description='reset can handle submodules'

. ./test-lib.sh
. "$TEST_DIRECTORY"/lib-submodule-update.sh

test_submodule_switch "git reset --keep"

test_submodule_switch "git reset --merge"

test_submodule_forced_switch "git reset --hard"

test_done

#!/bin/sh

test_description='test show-branch with long refname'

. ./test-lib.sh

test_expect_success 'setup' '

	test_commit first &&
	long_refname=$(printf "%s_" $(seq 1 50)) &&
	git checkout -b "$long_refname"
'

test_expect_success 'show-branch with long refname' '

	git show-branch first
'

test_done

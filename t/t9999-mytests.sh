#!/bin/sh

test_description='my tests'
. ./test-lib.sh
. "$TEST_DIRECTORY"/lib-httpd.sh
start_httpd

test_expect_success 'setup' '
	server="$HTTPD_DOCUMENT_ROOT_PATH/blobs" &&
	git init "$server" &&
	(
		cd "$server" &&
		test_commit myfile
	) &&

	# A reachable blob.
	reachable=$(
		git -C "$server" cat-file -p \
		$(git -C "$server" cat-file -p HEAD | grep "^tree " | cut -c6-) | \
		grep myfile | cut -c13-52) &&
	test -n "$reachable" &&

	# 2 unreachable blobs. Only one will be fetched. (2 are included here
	# to demonstrate that there is no whole-repo copying or anything like
	# that.)
	unreachable=$(echo abc123 | git -C "$server" hash-object -w --stdin) &&
	test -n "$unreachable" &&
	another_unreachable=$(echo def456 | git -C "$server" hash-object -w --stdin) &&
	test -n "$another_unreachable" &&

	git init --bare client.git &&
	git -C client.git remote add origin "$HTTPD_URL/smart/blobs" &&

	# These fetches are supposed to fail (and do)
	test_must_fail git -C client.git fetch origin $reachable &&
	test_must_fail git -C client.git fetch origin $unreachable
'

test_expect_success 'allowtipsha1inwant suddenly allows blobs' '
	test -n "$reachable" &&

	git -C "$server" config uploadpack.allowtipsha1inwant 1 &&

	# This fetch passes
	git -C client.git fetch origin $reachable &&
	test "myfile" = $(git -C client.git cat-file -p $reachable)
'

test_expect_success 'even unreachable ones' '
	test -n "$unreachable" &&

	# This fetch is supposed to fail (for multiple reasons), but passes.
	# Only the wanted blob is fetched.
	git -C client.git fetch origin $unreachable &&
	git -C client.git cat-file -e $unreachable &&
	test_must_fail git -C client.git cat-file -e $another_unreachable &&
	test "abc123" = $(git -C client.git cat-file -p $unreachable)
'

stop_httpd
test_done

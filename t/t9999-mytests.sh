#!/bin/sh

test_description='my tests'

. ./test-lib.sh

test_expect_success 'fetch-pack --new-way basic' '
	rm -rf server &&
	git init server &&
	(
		cd server &&
		test_commit 0 &&

		git checkout -b one
		test_commit 1 &&
		git checkout master &&

		git checkout -b two
		test_commit 2 &&
		git checkout master &&

		git checkout -b dont_fetch_this
		test_commit 3 &&
		git checkout master
	) &&
	git fetch-pack --new-way --exec=git-server-endpoint server refs/heads/one refs/heads/two\* >actual &&

	grep "$(printf "%s refs/heads/one" $(git -C server rev-parse --verify one))" actual &&
	grep "$(printf "%s refs/heads/two" $(git -C server rev-parse --verify two))" actual &&
	! grep dont_fetch_this actual
'

test_expect_success 'fetch-pack --new-way hideRefs' '
	rm -rf server &&
	git init server &&
	test_config -C server transfer.hideRefs refs/heads/b2 &&
	(
		cd server &&
		test_commit 0 &&

		git checkout -b b1
		test_commit 1 &&
		git checkout master &&

		git checkout -b b2
		test_commit 2 &&
		git checkout master
	) &&
	git fetch-pack --new-way --exec=git-server-endpoint server refs/heads/b\* >actual &&

	grep "$(printf "%s refs/heads/b1" $(git -C server rev-parse --verify 1))" actual &&
	! grep refs/heads/b2 actual
'

test_expect_success 'fetch-pack --new-way long negotiation' '
	rm -rf server client &&
	git init server &&
	test_commit -C server 0 &&

	git clone server client &&

	test_commit -C server 1 &&

	git -C client checkout -b sidebranch &&
	for i in $(seq 2 32)
	do
		test_commit -C client $i
	done &&

	git -C client fetch-pack --new-way --exec=git-server-endpoint ../server refs/heads/master >actual &&

	grep "$(printf "%s refs/heads/master" $(git -C server rev-parse --verify 1))" actual
'

test_expect_success 'fetch-pack --new-way with shallow client' '
	rm -rf server &&
	git init server &&
	(
		cd server &&
		test_commit 0 &&
		test_commit 1
	) &&
	rm -rf client &&
	git clone --depth=1 "file://$(pwd)/server" client &&
	(
		cd server &&
		git checkout -b two 0 &&
		test_commit 2
	) &&

	# check that the shallow clone does not include this parent commit
	test_must_fail git -C client cat-file -e $(git -C server rev-parse 0) &&

	git -C client fetch-pack --new-way --exec=git-server-endpoint ../server refs/heads/two >actual &&
	# 0 is the parent of 2, so it must be included now
	git -C client cat-file -e $(git -C server rev-parse 0)
'

test_expect_success 'fetch-pack --new-way --depth' '
	rm -rf server &&
	git init server &&
	(
		cd server &&
		test_commit 0
	) &&
	rm -rf client &&
	git clone server client &&
	(
		cd server &&
		test_commit 1 &&
		test_commit 2
	) &&

	git -C client fetch-pack --new-way --exec=git-server-endpoint --depth=1 ../server refs/heads/master >actual &&
	git -C client cat-file -e $(git -C server rev-parse 2) &&
	test_must_fail git -C client cat-file -e $(git -C server rev-parse 1)
'

test_expect_success 'fetch-pack --new-way --shallow-since' '
	rm -rf server &&
	git init server &&
	(
		cd server &&
		test_commit 0
	) &&
	rm -rf client &&
	git clone server client &&
	(
		cd server &&
		test_commit 1 &&
		test_commit 2
	) &&
	DATE=$(git -C server log --format="format:%at" --no-walk 2) &&

	git -C client fetch-pack --new-way --exec=git-server-endpoint --shallow-since=$DATE ../server refs/heads/master >actual &&
	git -C client cat-file -e $(git -C server rev-parse 2) &&
	test_must_fail git -C client cat-file -e $(git -C server rev-parse 1)
'

test_expect_success 'fetch-pack --new-way --shallow-exclude' '
	rm -rf server &&
	git init server &&
	(
		cd server &&
		test_commit 0
	) &&
	rm -rf client &&
	git clone server client &&
	(
		cd server &&
		test_commit 1 &&
		test_commit 2
	) &&

	git -C client fetch-pack --new-way --exec=git-server-endpoint --shallow-exclude=1 ../server refs/heads/master >actual &&
	git -C client cat-file -e $(git -C server rev-parse 2) &&
	test_must_fail git -C client cat-file -e $(git -C server rev-parse 1)
'

test_expect_success PIPE 'fetch-pack --new-way --stateless-rpc' '
	rm -rf server &&
	git init server &&
	(
		cd server &&
		test_commit 0 &&

		git checkout -b one
		test_commit 1 &&
		git checkout master &&

		git checkout -b two
		test_commit 2 &&
		git checkout master &&

		git checkout -b dont_fetch_this
		test_commit 3 &&
		git checkout master
	) &&
	rm -rf client &&
	git init client &&

	mkfifo se-out &&

	git -C client fetch-pack --new-way --stateless-rpc ../server refs/heads/one refs/heads/two\* <se-out | test-un-pkt server-endpoint server --stateless-rpc >se-out &&

	git -C client cat-file -e $(git -C server rev-parse 1) &&
	git -C client cat-file -e $(git -C server rev-parse 2) &&
	test_must_fail git -C client cat-file -e $(git -C server rev-parse 3)
'

test_expect_success 'fetch-pack --new-way --stateless-rpc long negotiation' '
	rm -rf server client &&
	git init server &&
	test_commit -C server 0 &&

	git clone server client &&

	test_commit -C server 1 &&

	git -C client checkout -b sidebranch &&
	for i in $(seq 2 32)
	do
		test_commit -C client $i
	done &&

	rm -f se-out &&
	mkfifo se-out &&

	git -C client fetch-pack --new-way --stateless-rpc ../server refs/heads/master <se-out | test-un-pkt server-endpoint server --stateless-rpc >se-out &&

	git -C client cat-file -e $(git -C server rev-parse 1)
'

test_expect_success 'fetch-pack --new-way --stateless-rpc namespaces' '
	rm -rf server &&
	git init server &&
	(
		cd server &&
		test_commit 0 &&

		git checkout -b mybranch
		test_commit 1 &&
		git checkout master &&

		git checkout -b mybranch_ns
		test_commit 2 &&
		git checkout master &&
		git update-ref refs/namespaces/ns/refs/heads/mybranch mybranch_ns
	) &&
	rm -rf client &&
	git init client &&

	rm -f se-out &&
	mkfifo se-out &&

	git -C client fetch-pack --new-way --stateless-rpc ../server refs/heads/mybranch <se-out | GIT_NAMESPACE=ns test-un-pkt server-endpoint server --stateless-rpc >se-out &&

	git -C client cat-file -e $(git -C server rev-parse 2) &&
	test_must_fail git -C client cat-file -e $(git -C server rev-parse 1)
'

test_done

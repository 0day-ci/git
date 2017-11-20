#!/bin/sh

test_description='test finding specific blobs in the revision walking'
. ./test-lib.sh

test_expect_success 'setup ' '
	git commit --allow-empty -m "empty initial commit" &&

	echo "Hello, world!" >greeting &&
	git add greeting &&
	git commit -m "add the greeting blob" && # borrowed from Git from the Bottom Up
	git tag -m "the blob" greeting $(git rev-parse HEAD:greeting) &&

	echo asdf >unrelated &&
	git add unrelated &&
	git commit -m "unrelated history" &&

	git revert HEAD^ &&

	git commit --allow-empty -m "another unrelated commit"
'

test_expect_success 'find the greeting blob' '
	cat >expect <<-EOF &&
	Revert "add the greeting blob"
	add the greeting blob
	EOF

	git log --abbrev=12 --oneline --blobfind=greeting^{blob} >actual.raw &&
	cut -c 14- actual.raw >actual &&

	test_cmp expect actual
'

test_done

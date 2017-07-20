#!/bin/sh

test_description='promised objects'

. ./test-lib.sh

test_expect_success 'fsck fails on missing objects' '
	test_create_repo repo &&

	test_commit -C repo 1 &&
	test_commit -C repo 2 &&
	test_commit -C repo 3 &&
	git -C repo tag -a annotated_tag -m "annotated tag" &&
	C=$(git -C repo rev-parse 1) &&
	T=$(git -C repo rev-parse 2^{tree}) &&
	B=$(git hash-object repo/3.t) &&
	AT=$(git -C repo rev-parse annotated_tag) &&

	# missing commit, tree, blob, and tag
	rm repo/.git/objects/$(echo $C | cut -c1-2)/$(echo $C | cut -c3-40) &&
	rm repo/.git/objects/$(echo $T | cut -c1-2)/$(echo $T | cut -c3-40) &&
	rm repo/.git/objects/$(echo $B | cut -c1-2)/$(echo $B | cut -c3-40) &&
	rm repo/.git/objects/$(echo $AT | cut -c1-2)/$(echo $AT | cut -c3-40) &&
	test_must_fail git -C repo fsck
'

test_expect_success '...but succeeds if they are promised objects' '
	printf "%s01%016x\n%s02%016x\n%s03%016x\n%s04%016x" \
		"$C" 0 "$T" 0 "$B" "$(wc -c <repo/3.t)" "$AT" 0 |
		sort | tr -d "\n" | hex_pack >repo/.git/objects/promised &&
	git -C repo config core.repositoryformatversion 1 &&
	git -C repo config extensions.promisedobjects "arbitrary string" &&
	git -C repo fsck
'

test_expect_success '...but fails again with GIT_IGNORE_PROMISED_OBJECTS' '
	GIT_IGNORE_PROMISED_OBJECTS=1 test_must_fail git -C repo fsck &&
	unset GIT_IGNORE_PROMISED_OBJECTS
'

test_done

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

test_expect_success 'sha1_object_info_extended (through git cat-file)' '
	test_create_repo server &&
	test_commit -C server 1 1.t abcdefgh &&
	HASH=$(git hash-object server/1.t) &&

	test_create_repo client &&
	test_must_fail git -C client cat-file -p "$HASH"
'

test_expect_success '...succeeds if it is a promised object' '
	printf "%s03%016x" "$HASH" "$(wc -c <server/1.t)" |
		hex_pack >client/.git/objects/promised &&
	git -C client config core.repositoryformatversion 1 &&
	git -C client config extensions.promisedobjects \
		"\"$TEST_DIRECTORY/t3907/read-object\" \"$(pwd)/server/.git\"" &&
	git -C client cat-file -p "$HASH"
'

test_expect_success 'cat-file --batch-all-objects with promised objects' '
	rm -rf client &&
	test_create_repo client &&
	git -C client config core.repositoryformatversion 1 &&
	git -C client config extensions.promisedobjects \
		"\"$TEST_DIRECTORY/t3907/read-object\" \"$(pwd)/server/.git\"" &&
	printf "%s03%016x" "$HASH" "$(wc -c <server/1.t)" |
		hex_pack >client/.git/objects/promised &&

	# Verify that the promised object is printed
	git -C client cat-file --batch --batch-all-objects | tee out |
		grep abcdefgh
'

test_done

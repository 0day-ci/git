#!/bin/sh

test_description='core.missingblobcommand option'

. ./test-lib.sh

test_expect_success 'sha1_object_info_extended and read_sha1_file (through git cat-file -p)' '
	rm -rf server client &&

	git init server &&
	test_commit -C server 1 &&
	test_config -C server uploadpack.allowanysha1inwant 1 &&
	HASH=$(git hash-object server/1.t) &&

	git init client &&
	test_config -C client core.missingblobcommand \
		"git -C \"$(pwd)/server\" pack-objects --stdout | git unpack-objects" &&
	git -C client cat-file -p "$HASH"
'

test_expect_success 'has_sha1_file (through git cat-file -e)' '
	rm -rf server client &&

	git init server &&
	test_commit -C server 1 &&
	test_config -C server uploadpack.allowanysha1inwant 1 &&
	HASH=$(git hash-object server/1.t) &&

	git init client &&
	test_config -C client core.missingblobcommand \
		"git -C \"$(pwd)/server\" pack-objects --stdout | git unpack-objects" &&
	git -C client cat-file -e "$HASH"
'

test_expect_success 'fsck' '
	rm -rf server client &&

	git init server &&
	test_commit -C server 1 &&
	test_config -C server uploadpack.allowanysha1inwant 1 &&
	HASH=$(git hash-object server/1.t) &&
	echo hash is $HASH &&

	cp -r server client &&
	test_config -C client core.missingblobcommand "this-command-is-not-actually-run" &&
	rm client/.git/objects/$(echo $HASH | cut -c1-2)/$(echo $HASH | cut -c3-40) &&
	git -C client fsck
'

test_done

#!/bin/sh

test_description='anchored diff algorithm'

. ./test-lib.sh

test_expect_success '--anchored' '
	test_write_lines a b c >pre &&
	test_write_lines c a b >post &&

	# normally, c is moved to produce the smallest diff
	test_expect_code 1 git diff --no-index pre post >diff &&
	grep "^+c" diff &&

	# with anchor, a is moved
	test_expect_code 1 git diff --no-index --anchored=c pre post >diff &&
	grep "^+a" diff
'

test_expect_success '--anchored multiple' '
	test_write_lines a b c d e f >pre &&
	test_write_lines c a b f d e >post &&

	# with 1 anchor, c is not moved, but f is moved
	test_expect_code 1 git diff --no-index --anchored=c pre post >diff &&
	grep "^+a" diff && # a is moved instead of c
	grep "^+f" diff &&

	# with 2 anchors, c and f are not moved
	test_expect_code 1 git diff --no-index --anchored=c --anchored=f pre post >diff &&
	grep "^+a" diff &&
	grep "^+d" diff # d is moved instead of f
'

test_expect_success '--anchored with nonexistent line has no effect' '
	test_write_lines a b c >pre &&
	test_write_lines c a b >post &&

	test_expect_code 1 git diff --no-index --anchored=x pre post >diff &&
	grep "^+c" diff
'

test_expect_success '--anchored with non-unique line has no effect' '
	test_write_lines a b c d e c >pre &&
	test_write_lines c a b c d e >post &&

	test_expect_code 1 git diff --no-index --anchored=c pre post >diff &&
	grep "^+c" diff
'

test_expect_success 'diff still produced with impossible multiple --anchored' '
	test_write_lines a b c >pre &&
	test_write_lines c a b >post &&

	test_expect_code 1 git diff --no-index --anchored=a --anchored=c pre post >diff &&
	mv post expected_post &&

	# Ensure that the diff is correct by applying it and then
	# comparing the result with the original
	git apply diff &&
	diff expected_post post
'

test_expect_success 'later algorithm arguments override earlier ones' '
	test_write_lines a b c >pre &&
	test_write_lines c a b >post &&

	test_expect_code 1 git diff --no-index --patience --anchored=c pre post >diff &&
	grep "^+a" diff &&

	test_expect_code 1 git diff --no-index --anchored=c --patience pre post >diff &&
	grep "^+c" diff &&

	test_expect_code 1 git diff --no-index --histogram --anchored=c pre post >diff &&
	grep "^+a" diff &&

	test_expect_code 1 git diff --no-index --anchored=c --histogram pre post >diff &&
	grep "^+c" diff &&

	test_expect_code 1 git diff --no-index --diff-algorithm=patience --anchored=c pre post >diff &&
	grep "^+a" diff &&

	test_expect_code 1 git diff --no-index --anchored=c --diff-algorithm=patience pre post >diff &&
	grep "^+c" diff
'

test_expect_success '--anchored works with other commands like "git show"' '
	test_write_lines a b c >file &&
	git add file &&
	git commit -m foo &&
	test_write_lines c a b >file &&
	git add file &&
	git commit -m foo &&

	# with anchor, a is moved
	git show --patience --anchored=c >diff &&
	grep "^+a" diff
'

test_done
